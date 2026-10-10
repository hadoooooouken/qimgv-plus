// core.h includes <windows.h> (through the directory watcher headers); its
// min/max macros would break Qt templates included after it (QRangeModel in
// the Quick UI bridges).
#define NOMINMAX

#include <QCommandLineParser>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>
#include <QStandardPaths>

#include <cstdlib>
#include <memory>
#include <optional>

#include "appservices.h"
#include "apptranslator.h"
#include "appversion.h"
#include "components/actionmanager/actionmanager.h"
#include "components/scriptmanager/scriptmanager.h"
#include "components/singleinstance/singleinstancechannel.h"
#include "core.h"
#include "gui/quick/quickuihost.h"
#include "settings.h"
#include "utils/cmdoptionsrunner.h"
#include "utils/startuptiming.h"

//------------------------------------------------------------------------------
QDataStream &operator<<(QDataStream &out, const Script &v) {
  out << v.command << v.blocking;
  return out;
}
//------------------------------------------------------------------------------
QDataStream &operator>>(QDataStream &in, Script &v) {
  in >> v.command;
  in >> v.blocking;
  return in;
}
//------------------------------------------------------------------------------
// The "multiInstance" setting, read before the services exist (they are
// started only by the primary instance).
bool multiInstanceEnabled() {
  QString confPath = QCoreApplication::applicationDirPath() + "/conf";
  if (!QFileInfo::exists(confPath + "/qimgv-plus.ini"))
    confPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QSettings tempSettings(confPath + "/qimgv-plus.ini", QSettings::IniFormat);
  return tempSettings.value("multiInstance", false).toBool();
}
//------------------------------------------------------------------------------
// Runs Core over the user interface until the application exits: opens the
// command-line path (or the default path), answers the paths sent by
// secondary instances through channel (nullptr in multi-instance mode) and
// shows the window.
int runCore(QGuiApplication &app, const UiPorts &ports,
            SingleInstanceChannel *channel, const QStringList &paths) {
  QObject::connect(
      &ports.events, &UiEvents::documentRenderingSettled, &ports.events,
      []() { logStartupMilestone(u"first document rendering settled"); },
      Qt::SingleShotConnection);

  Core core(ports);
  if (channel) {
    QObject::connect(channel, &SingleInstanceChannel::pathReceived, &core,
                     [&core](const QString &path) { core.raiseWindow(path); });
    channel->listen();
  }

  if (!paths.isEmpty())
    core.loadPath(paths.constFirst());
  else
    core.loadDefaultPath();

  // wait for event queue to catch up before showing window
  // this avoids white background flicker on windows (or not?)
  qApp->processEvents();

  core.showGui();
  return app.exec();
}
//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {

  // force some env variables
  qputenv("QT_PLUGIN_PATH", "");

  QGuiApplication a(argc, argv);
  QCoreApplication::setLibraryPaths(QStringList()
                                    << QCoreApplication::applicationDirPath());

  // Declared after the QGuiApplication so it is destroyed first, while the
  // event loop objects still exist. Engaged only on the paths that need the
  // services.
  std::optional<AppServices> services;
  const auto startServices = [&services]() { services.emplace(); };

  QCoreApplication::setOrganizationName("qimgv-plus");
  QCoreApplication::setOrganizationDomain(
      "github.com/hadoooooouken/qimgv-plus");
  QCoreApplication::setApplicationName("qimgv-plus");
  QCoreApplication::setApplicationVersion(appVersion.toString());

  // use custom types in signals
  qRegisterMetaType<Script>("Script");
  qRegisterMetaType<QPixmap *>("QPixmap*");
  qRegisterMetaType<std::shared_ptr<Image>>("std::shared_ptr<Image>");
  qRegisterMetaType<std::shared_ptr<Thumbnail>>("std::shared_ptr<Thumbnail>");


  // parse args
  // ------------------------------------------------------------------
  QCommandLineParser parser;
  QString appDescription =
      qApp->applicationName() + " - Fast and configurable image viewer.";
  appDescription.append("\nVersion: " + qApp->applicationVersion());
  appDescription.append("\nLicense: GNU GPLv3");
  parser.setApplicationDescription(appDescription);
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(
      "path", QCoreApplication::translate("main", "File or directory path."));
  parser.addOptions({
      {"gen-thumbs",
       QCoreApplication::translate("main",
                                   "Generate all thumbnails for directory."),
       QCoreApplication::translate("main", "directory-path")},
      {"gen-thumbs-size",
       QCoreApplication::translate(
           "main", "Thumbnail size. Current size is used if not specified."),
       QCoreApplication::translate("main", "thumbnail-size")},
      {"build-options",
       QCoreApplication::translate("main", "Show build options.")},
  });
  parser.process(a);

  int exitCode = 0;
  if (parser.isSet("build-options")) {
    startServices();

    CmdOptionsRunner r;
    QTimer::singleShot(0, &r, &CmdOptionsRunner::showBuildOptions);
    exitCode = a.exec();
  } else if (parser.isSet("gen-thumbs")) {
    startServices();

    int size = settings->folderViewIconSize();
    if (parser.isSet("gen-thumbs-size"))
      size = parser.value("gen-thumbs-size").toInt();

    CmdOptionsRunner r;
    QTimer::singleShot(0, &r,
                       [&r, path = parser.value("gen-thumbs"), size] { r.generateThumbs(path, size); });
    exitCode = a.exec();
  } else {
    // Primary or secondary instance: a second launch hands its path to the
    // running instance, which raises its window.
    std::optional<SingleInstanceChannel> channel;
    if (!multiInstanceEnabled()) {
      channel.emplace(SingleInstanceChannel::serverNameFor(QDir::tempPath()));
      QString pathToSend;
      if (!parser.positionalArguments().isEmpty()) {
        pathToSend =
            QFileInfo(parser.positionalArguments().constFirst()).absoluteFilePath();
      }
      if (channel->forwardToPrimary(pathToSend))
        return 0;
    }

    // Primary instance, initialize all services
    startServices();

    // Closing the window may only suspend to standby; the exit action quits.
    QGuiApplication::setQuitOnLastWindowClosed(false);
    // Installed before the user interface is built so that its strings are
    // translated; destroyed last, after the UI and Core.
    AppTranslator translator;
    SingleInstanceChannel *channelPtr = channel ? &*channel : nullptr;
    QuickUiHost quickUi(*settings, *actionManager, *scriptManager);
    const std::optional<UiPorts> ports =
        quickUi.start() ? quickUi.ports() : std::nullopt;
    exitCode = ports ? runCore(a, *ports, channelPtr, parser.positionalArguments())
                     : EXIT_FAILURE;
  }

  return exitCode;
}
