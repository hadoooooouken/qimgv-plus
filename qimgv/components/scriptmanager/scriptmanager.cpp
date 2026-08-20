#include "scriptmanager.h"
#include "settings.h"
#include <QEventLoop>
#include <QScopeGuard>
#include <QTimer>

ScriptManager *scriptManager = nullptr;

ScriptManager::ScriptManager(QObject *parent)
    : QObject(parent)
{
}

ScriptManager::~ScriptManager() {
    saveScripts();
    if (scriptManager == this) {
        scriptManager = nullptr;
    }
}

ScriptManager *ScriptManager::getInstance() {
    if(!scriptManager) {
        scriptManager = new ScriptManager();
        scriptManager->readScripts();
    }
    return scriptManager;
}

void ScriptManager::runScript(const QString &scriptName, std::shared_ptr<Image> img) {
    if(scripts.contains(scriptName)) {
        Script script = scripts.value(scriptName);
        if(script.command.trimmed().isEmpty()) {
            QString errorString = "Error: script \"" + scriptName + "\" has an empty command.";
            qWarning() << "[ScriptManager]" << errorString;
            emit error(errorString);
            return;
        }
        auto arguments = splitCommandLine(script.command);
        if(arguments.isEmpty()) {
            QString errorString = "Error: script \"" + scriptName + "\" could not be parsed into a valid command.";
            qWarning() << "[ScriptManager]" << errorString;
            emit error(errorString);
            return;
        }
        processArguments(arguments, img);
        QString program = arguments.takeAt(0);

        if(script.blocking) {
            if(blockingScriptActive) {
                QString errorString = "Error: script \"" + scriptName +
                        "\" was not started because another blocking script is still running.";
                qWarning() << "[ScriptManager]" << errorString;
                emit error(errorString);
                return;
            }
            blockingScriptActive = true;
            const auto activeGuard = qScopeGuard([this]() { blockingScriptActive = false; });

            // Heap-allocated with no QObject parent, deliberately: this object must be
            // able to outlive both this function call and qimgv's own shutdown without
            // its destructor force-killing the external process. QProcess::~QProcess()
            // kills the underlying process if it is still running when the QProcess
            // object is destroyed. A stack-local QProcess here would trigger exactly
            // that if qimgv quits mid-wait, because QCoreApplication::quit() unwinds
            // the nested QEventLoop below directly, without going through either the
            // "finished" or "timed out" signals this function watches for.
            //
            // The finished->deleteLater connection below only requests self-deletion
            // once the child process has genuinely exited on its own; at that point
            // there is nothing left to kill. If the process never exits before
            // qimgv's own process ends, this object is simply abandoned and reclaimed
            // by the OS along with everything else, and the external process is left
            // running — matching the (already correct) non-blocking behavior below.
            auto *exec = new QProcess();
            connect(exec, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                    exec, &QObject::deleteLater);

            exec->start(program, arguments);
            if(!exec->waitForStarted()) {
                qWarning() << "Unable not run application/script." << program << " Make sure it is an executable.";
                exec->deleteLater();
            } else {
                // Wait via a nested QEventLoop instead of QProcess::waitForFinished()
                // so the GUI event loop keeps pumping (repaints, input) while we wait.
                QEventLoop loop;
                QTimer timeoutTimer;
                timeoutTimer.setSingleShot(true);
                bool timedOut = false;

                connect(exec, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                        &loop, &QEventLoop::quit);
                connect(&timeoutTimer, &QTimer::timeout, &loop, [&timedOut, &loop]() {
                    timedOut = true;
                    loop.quit();
                });

                timeoutTimer.start(kBlockingScriptTimeoutMs);
                loop.exec();

                if(timedOut && exec->state() != QProcess::NotRunning) {
                    qWarning() << "Script execution timed out. Killing process.";
                    exec->kill();
                    exec->waitForFinished();
                }
                // If loop.exec() returned for neither reason above (e.g. qimgv itself
                // is quitting), we deliberately neither kill the process nor block
                // waiting for it — see the ownership comment above.
            }
        } else {
            // No QProcess instance needed here: QProcess::startDetached(program, arguments)
            // is a static member, so it does not depend on or manage any local object.
            if(!QProcess::startDetached(program, arguments)) {
                QFileInfo fi(program);
                QString errorString;
                if(fi.isFile() && !fi.isExecutable())
                     errorString = "Error:  " + program + "  is not an executable.";
                else
                    errorString = "Error: unable run application/script. See README for working examples.";
                emit error(errorString);
                qWarning() << errorString;
            }
        }
    } else {
        qWarning() << "[ScriptManager] File " << scriptName << " does not exist.";
    }
}

QString ScriptManager::runCommand(QString cmd) {
    QStringList cmdSplit = ScriptManager::splitCommandLine(cmd);
    if(cmdSplit.isEmpty()) {
        qWarning() << "[ScriptManager] runCommand() called with an empty or unparseable command.";
        return QString();
    }
    QProcess exec;
    exec.start(cmdSplit.takeAt(0), cmdSplit);
    if(!exec.waitForFinished(2000)) {
        exec.kill();
        exec.waitForFinished();
    }
    return exec.readAllStandardOutput();
}

void ScriptManager::runCommandDetached(QString cmd) {
    QStringList cmdSplit = ScriptManager::splitCommandLine(cmd);
    if(cmdSplit.isEmpty()) {
        qWarning() << "[ScriptManager] runCommandDetached() called with an empty or unparseable command.";
        return;
    }
    QProcess::startDetached(cmdSplit.takeAt(0), cmdSplit);
}

void ScriptManager::processArguments(QStringList &cmd, std::shared_ptr<Image> img) {
    for (auto &arg : cmd) {
        if (arg.contains("%file%")) {
            arg.replace("%file%", img->filePath());
        }
        // Windows always uses backslashes
        arg.replace("/", "\\");
        arg.replace("\\\\", "\\");
    }
}

// thanks stackoverflow
QStringList ScriptManager::splitCommandLine(const QString &cmdLine) {
    QStringList list;
    QString arg;
    bool escape = false;
    enum { Idle, Arg, QuotedArg } state = Idle;
    for (QChar const c : cmdLine) {
        //if(!escape && c == '\\') {
        //    escape = true;
        //    continue;
        //}
        switch (state) {
        case Idle:
            if(!escape && c == '"')
                state = QuotedArg;
            else if (escape || !c.isSpace()) {
                arg += c;
                state = Arg;
            }
            break;
        case Arg:
            if(!escape && c == '"')
                state = QuotedArg;
            else if(escape || !c.isSpace())
                arg += c;
            else {
                list << arg;
                arg.clear();
                state = Idle;
            }
            break;
        case QuotedArg:
            if(!escape && c == '"')
                state = arg.isEmpty() ? Idle : Arg;
            else
                arg += c;
            break;
        }
        escape = false;
    }
    if(!arg.isEmpty())
        list << arg;
    return list;
}


bool ScriptManager::scriptExists(QString scriptName) {
    return scripts.contains(scriptName);
}

void ScriptManager::readScripts() {
    settings->readScripts(scripts);
}

void ScriptManager::saveScripts() {
    settings->saveScripts(scripts);
}

// replaces if it already exists
void ScriptManager::addScript(QString scriptName, Script script) {
    if(scripts.contains(scriptName)) {
        qDebug() << "[ScriptManager] Replacing script" << scriptName;
        scripts.remove(scriptName);
    }
    scripts.insert(scriptName, script);
}

void ScriptManager::removeScript(QString scriptName) {
    scripts.remove(scriptName);
}

const QMap<QString, Script> &ScriptManager::allScripts() {
    return scriptManager->scripts;
}

QList<QString> ScriptManager::scriptNames() {
    return scriptManager->scripts.keys();
}

Script ScriptManager::getScript(QString scriptName) {
    return scripts.value(scriptName);
}
