#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStyleFactory>
#include <QSettings>
#include <QStandardPaths>

#ifdef _WIN32
#include <windows.h>
#endif

#include "appversion.h"
#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "proxystyle.h"
#include "settings.h"
#include "sharedresources.h"
#include "utils/actions.h"
#include "utils/cmdoptionsrunner.h"
#include "utils/iconfontmanager.h"
#include "utils/inputmap.h"

//------------------------------------------------------------------------------
ProxyStyleColors proxyStyleColors(const ColorScheme &colors) {
  return {
      .icons = colors.icons,
      .control = colors.button,
      .controlHover = colors.button_hover,
      .controlPressed = colors.button_pressed,
      .controlBorder = colors.widget_border,
      .controlFocusBorder = colors.accent,
  };
}
//------------------------------------------------------------------------------
void initSingletons(ProxyStyle &proxyStyle) {
  // Must run before any IconWidget/StyledComboBox is constructed, since
  // both resolve glyphs through IconFontManager::pixmap() during their
  // first paint. Safe to call unconditionally: init() is idempotent and
  // a failed font load just means glyph rendering will log a warning and
  // return null pixmaps instead of crashing.
  IconFontManager::init();
  inputMap = InputMap::getInstance();
  appActions = Actions::getInstance();
  settings = Settings::getInstance();
  proxyStyle.setColors(proxyStyleColors(settings->colorScheme()));
  Settings *appSettings = settings;
  QObject::connect(appSettings, &Settings::settingsChanged, &proxyStyle,
                   [appSettings, &proxyStyle]() {
                     proxyStyle.setColors(
                         proxyStyleColors(appSettings->colorScheme()));
                   });
  scriptManager = ScriptManager::getInstance();
  actionManager = ActionManager::getInstance();
  shrRes = SharedResources::getInstance();
}
//------------------------------------------------------------------------------
void cleanupSingletons() {
  delete actionManager;
  actionManager = nullptr;
  delete scriptManager;
  scriptManager = nullptr;
  delete settings;
  settings = nullptr;
  delete inputMap;
  inputMap = nullptr;
  delete appActions;
  appActions = nullptr;
  delete shrRes;
  shrRes = nullptr;
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
int main(int argc, char *argv[]) {

  // force some env variables
  qputenv("QT_PLUGIN_PATH", "");

  QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

  QApplication a(argc, argv);
  QCoreApplication::setLibraryPaths(QStringList()
                                    << QCoreApplication::applicationDirPath());

  // use some style workarounds with platform-independent Fusion base to prevent
  // uxtheme clashes in Windows 11
  auto *proxyStyle = new ProxyStyle(QStyleFactory::create("fusion"));
  a.setStyle(proxyStyle);

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
  parser.process(a);

  int exitCode = 0;
  if (parser.isSet("build-options")) {
    initSingletons(*proxyStyle);

    CmdOptionsRunner r;
    QTimer::singleShot(0, &r, &CmdOptionsRunner::showBuildOptions);
    exitCode = a.exec();
  } else if (parser.isSet("gen-thumbs")) {
    initSingletons(*proxyStyle);

    int size = settings->folderViewIconSize();
    if (parser.isSet("gen-thumbs-size"))
      size = parser.value("gen-thumbs-size").toInt();

    CmdOptionsRunner r;
    QTimer::singleShot(0, &r,
                       [&r, path = parser.value("gen-thumbs"), size] { r.generateThumbs(path, size); });
    exitCode = a.exec();
  } else {
    // -----------------------------------------------------------------------------

    bool isMultiInstance = false;
    {
      QString appDirPath = QCoreApplication::applicationDirPath();
      QString confPath = appDirPath + "/conf";
      if (!QFileInfo::exists(confPath + "/qimgv-plus.ini")) {
        confPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
      }
      QSettings tempSettings(confPath + "/qimgv-plus.ini", QSettings::IniFormat);
      isMultiInstance = tempSettings.value("multiInstance", false).toBool();
    }

    QString serverName = "qimgv-plus-single-instance-" +
                         QCryptographicHash::hash(QDir::tempPath().toUtf8(),
                                                  QCryptographicHash::Md5)
                             .toHex();
    QLocalServer *server = nullptr;

    if (!isMultiInstance) {
      QLocalSocket socket;
      socket.connectToServer(serverName);
      if (socket.waitForConnected(500)) {
        QByteArray data;
        QDataStream out(&data, QIODevice::WriteOnly);
        QString pathToSend;
        if (parser.positionalArguments().count()) {
          pathToSend =
              QFileInfo(parser.positionalArguments().at(0)).absoluteFilePath();
        }
        out << pathToSend;
#ifdef _WIN32
        AllowSetForegroundWindow(ASFW_ANY);
#endif
        socket.write(data);
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;
      }

      QLocalServer::removeServer(serverName);
      server = new QLocalServer(&a);
    }

    // Primary instance, initialize all singletons
    initSingletons(*proxyStyle);

    {
      QApplication::setQuitOnLastWindowClosed(false);
      Core core;

      if (server) {
        QObject::connect(
            server, &QLocalServer::newConnection, [server, &core]() {
              QLocalSocket *clientSocket = server->nextPendingConnection();
              if (!clientSocket)
                return;
              QObject::connect(clientSocket, &QLocalSocket::disconnected,
                               clientSocket, &QLocalSocket::deleteLater);
              QObject::connect(clientSocket, &QLocalSocket::readyRead,
                               [clientSocket, &core]() {
                                 QDataStream in(clientSocket);
                                 in.startTransaction();
                                 QString pathReceived;
                                 in >> pathReceived;
                                 if (!in.commitTransaction())
                                   return;
                                 core.raiseWindow(pathReceived);
                               });
            });
        server->listen(serverName);
      }

      if (parser.positionalArguments().count())
        core.loadPath(parser.positionalArguments().at(0));
      else if (settings->rememberLastFolder() && !settings->lastFolder().isEmpty() && QFileInfo(settings->lastFolder()).exists()) {
        core.loadPath(settings->lastFolder());
      } else if (settings->defaultViewMode() == MODE_FOLDERVIEW) {
        QStringList bookmarks = settings->bookmarks();
        if (!bookmarks.isEmpty() && QFileInfo(bookmarks.first()).exists())
          core.loadPath(bookmarks.first());
        else
          core.loadPath(QDir::homePath());
      }

      // wait for event queue to catch up before showing window
      // this avoids white background flicker on windows (or not?)
      qApp->processEvents();

      core.showGui();
      exitCode = a.exec();
    }
  }

  cleanupSingletons();
  return exitCode;
}
