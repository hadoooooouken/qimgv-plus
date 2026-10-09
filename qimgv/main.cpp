// core.h includes <windows.h> (through the directory watcher headers); its
// min/max macros would break Qt templates included after it (QRangeModel in
// the Quick UI bridges).
#define NOMINMAX

#include <QApplication>
#include <QCommandLineParser>
#include <QDataStream>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QStyleFactory>
#include <QSettings>
#include <QStandardPaths>

#include <cstdlib>
#include <memory>
#include <optional>

#include "appservices.h"
#include "apptranslator.h"
#include "appversion.h"
#include "components/actionmanager/actionmanager.h"
#include "components/singleinstance/singleinstancechannel.h"
#include "core.h"
#include "gui/quick/quickuihost.h"
#include "gui/widgetui/widgetui.h"
#include "proxystyle.h"
#include "settings.h"
#include "utils/cmdoptionsrunner.h"
#include "utils/startuptiming.h"

//------------------------------------------------------------------------------
ProxyStyleColors proxyStyleColors(const ColorScheme &colors) {
  return {
      .icons = colors.icons,
      .control = colors.combo_field,
      .controlHover = colors.combo_field_hover,
      .controlPressed = colors.combo_field_pressed,
      .controlBorder = colors.combo_field_border,
      .controlFocusBorder = colors.accent,
  };
}
//------------------------------------------------------------------------------
// Keeps the application style in sync with the current colour scheme.
void bindProxyStyleToSettings(ProxyStyle &proxyStyle, Settings &appSettings) {
  proxyStyle.setColors(proxyStyleColors(appSettings.colorScheme()));
  QObject::connect(&appSettings, &Settings::settingsChanged, &proxyStyle,
                   [&appSettings, &proxyStyle]() {
                     proxyStyle.setColors(
                         proxyStyleColors(appSettings.colorScheme()));
                   });
}
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
// User interface selected at startup with --ui. The widget UI stays the
// default until the Qt Quick UI reaches parity (docs/QML_MIGRATION_PLAN.md).
enum class UiMode { Widgets, Quick };

namespace {
using namespace Qt::StringLiterals;

constexpr QLatin1StringView uiOptionName = "ui"_L1;
constexpr QLatin1StringView uiModeWidgetsName = "widgets"_L1;
constexpr QLatin1StringView uiModeQuickName = "quick"_L1;
} // namespace

std::optional<UiMode> uiModeFromName(QStringView name) {
  if (name == uiModeWidgetsName)
    return UiMode::Widgets;
  if (name == uiModeQuickName)
    return UiMode::Quick;
  return std::nullopt;
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
// Runs Core over the given user interface until the application exits: opens
// the command-line path (or the default path), answers the paths sent by
// secondary instances through channel (nullptr in multi-instance mode) and
// shows the window.
int runCore(QApplication &app, const UiPorts &ports,
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

  QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

  QApplication a(argc, argv);
  QCoreApplication::setLibraryPaths(QStringList()
                                    << QCoreApplication::applicationDirPath());

  // use some style workarounds with platform-independent Fusion base to prevent
  // uxtheme clashes in Windows 11.
  // Ownership: ProxyStyle owns the Fusion base style, and setStyle() makes
  // QApplication the owner of ProxyStyle, which deletes it on destruction.
  // proxyStyle is a non-owning handle that stays valid for all of main().
  auto *proxyStyle = new ProxyStyle(QStyleFactory::create("fusion"));
  a.setStyle(proxyStyle);

  // Declared after the QApplication so it is destroyed first, while the
  // style and the event loop objects still exist. Engaged only on the paths
  // that need the services.
  std::optional<AppServices> services;
  const auto startServices = [&services, proxyStyle]() {
    services.emplace();
    bindProxyStyleToSettings(*proxyStyle, *settings);
  };

  QCoreApplication::setOrganizationName("qimgv-plus");
  QCoreApplication::setOrganizationDomain(
      "github.com/hadoooooouken/qimgv-plus");
  QCoreApplication::setApplicationName("qimgv-plus");
  QCoreApplication::setApplicationVersion(appVersion.toString());
  QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

  // use custom types in signals
  qRegisterMetaType<ScalerRequest>("ScalerRequest");
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
  const QCommandLineOption uiOption(
      uiOptionName,
      QCoreApplication::translate("main",
                                  "User interface: %1 (default) or %2.")
          .arg(uiModeWidgetsName, uiModeQuickName),
      QCoreApplication::translate("main", "ui"), uiModeWidgetsName);
  parser.addOption(uiOption);
  parser.process(a);

  const std::optional<UiMode> uiMode =
      uiModeFromName(parser.value(uiOption));
  if (!uiMode) {
    parser.showMessageAndExit(
        QCommandLineParser::MessageType::Error,
        QCoreApplication::translate("main", "Unknown user interface: %1")
            .arg(parser.value(uiOption)),
        EXIT_FAILURE);
  }

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
    // Primary or secondary instance; the same channel serves both UIs, so a
    // second launch raises whichever UI is running.
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
    QApplication::setQuitOnLastWindowClosed(false);
    // Installed before the user interface is built so that its strings are
    // translated; destroyed last, after the UI and Core.
    AppTranslator translator;
    SingleInstanceChannel *channelPtr = channel ? &*channel : nullptr;
    if (*uiMode == UiMode::Quick) {
      QuickUiHost quickUi(*settings, *actionManager);
      const std::optional<UiPorts> ports =
          quickUi.start() ? quickUi.ports() : std::nullopt;
      exitCode = ports ? runCore(a, *ports, channelPtr, parser.positionalArguments())
                       : EXIT_FAILURE;
    } else {
      WidgetUi widgetUi;
      exitCode = runCore(a, widgetUi.ports(), channelPtr, parser.positionalArguments());
    }
  }

  return exitCode;
}
