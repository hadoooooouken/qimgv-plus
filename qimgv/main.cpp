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
#include "utils/inputmap.h"

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

  // do we still need this?
  qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");

#if (QT_VERSION_MAJOR == 5)
  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

  // Qt6 hidpi rendering on windows still has artifacts
  // This disables it for scale factors < 1.75
  // In this case only fonts are scaled
#if (QT_VERSION_MAJOR == 6)
  QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::RoundPreferFloor);
#endif

  QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

  QApplication a(argc, argv);
  QCoreApplication::setLibraryPaths(QStringList()
                                    << QCoreApplication::applicationDirPath());

  // use some style workarounds
  a.setStyle(new ProxyStyle);

  QCoreApplication::setOrganizationName("qimgv-plus");
  QCoreApplication::setOrganizationDomain(
      "github.com/hadoooooouken/qimgv-plus");
  QCoreApplication::setApplicationName("qimgv-plus");
  QCoreApplication::setApplicationVersion(appVersion.toString());
  QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);

  // use custom types in signals
  qRegisterMetaType<ScalerRequest>("ScalerRequest");
  qRegisterMetaType<Script>("Script");
#ifdef USE_UPSCAYL
  qRegisterMetaType<QPixmap*>("QPixmap*");
#endif
  qRegisterMetaType<std::shared_ptr<Image>>("std::shared_ptr<Image>");
  qRegisterMetaType<std::shared_ptr<Thumbnail>>("std::shared_ptr<Thumbnail>");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qRegisterMetaTypeStreamOperators<Script>("Script");
#endif

  // globals
  inputMap = InputMap::getInstance();
  appActions = Actions::getInstance();
  settings = Settings::getInstance();
  scriptManager = ScriptManager::getInstance();
  actionManager = ActionManager::getInstance();
  shrRes = SharedResources::getInstance();

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
    CmdOptionsRunner r;
    QTimer::singleShot(0, &r, &CmdOptionsRunner::showBuildOptions);
    exitCode = a.exec();
  } else if (parser.isSet("gen-thumbs")) {
    int size = settings->folderViewIconSize();
    if (parser.isSet("gen-thumbs-size"))
      size = parser.value("gen-thumbs-size").toInt();

    CmdOptionsRunner r;
    QTimer::singleShot(0, &r,
                       std::bind(&CmdOptionsRunner::generateThumbs, &r,
                                 parser.value("gen-thumbs"), size));
    exitCode = a.exec();
  } else {
    // -----------------------------------------------------------------------------

    QString serverName = "qimgv-plus-single-instance-" +
                         QCryptographicHash::hash(QDir::tempPath().toUtf8(),
                                                  QCryptographicHash::Md5)
                             .toHex();
    QLocalServer *server = nullptr;

    if (!settings->multiInstance()) {
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
        cleanupSingletons();
        return 0;
      }

      QLocalServer::removeServer(serverName);
      server = new QLocalServer(&a);
    }

    {
      Core core;

      if (server) {
        QObject::connect(server, &QLocalServer::newConnection, [server, &core]() {
          QLocalSocket *clientSocket = server->nextPendingConnection();
          if (!clientSocket)
            return;
          QObject::connect(clientSocket, &QLocalSocket::disconnected, clientSocket,
                           &QLocalSocket::deleteLater);
          QObject::connect(clientSocket, &QLocalSocket::readyRead,
                           [clientSocket, &core]() {
                             QDataStream in(clientSocket);
                             QString pathReceived;
                             in >> pathReceived;
                             core.raiseWindow();
                             if (!pathReceived.isEmpty()) {
                               core.loadPath(pathReceived);
                             }
                           });
        });
        server->listen(serverName);
      }

      if (parser.positionalArguments().count())
        core.loadPath(parser.positionalArguments().at(0));
      else if (settings->defaultViewMode() == MODE_FOLDERVIEW) {
        QStringList bookmarks = settings->bookmarks();
        if (!bookmarks.isEmpty())
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
