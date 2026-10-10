#include "scripteditormodel.h"

#include <QCoreApplication>
#include <QDebug>

#include "settingsstores.h"

namespace {
using namespace Qt::StringLiterals;

// The texts keep the translation context of the widget dialog.
constexpr char kContext[] = "ScriptEditorDialog";

QString translated(const char *text) {
    return QCoreApplication::translate(kContext, text);
}

// Placeholder of the command line that the current file replaces.
constexpr QStringView kFileKeyword = u"%file%";
} // namespace

ScriptEditorModel::ScriptEditorModel(IShortcutScriptStore &store, QObject *parent)
    : EditorSession(parent), mStore(store) {}

QString ScriptEditorModel::title() const {
    return mEditing ? translated("Edit") : translated("New application/script");
}

QString ScriptEditorModel::name() const {
    return mName;
}

void ScriptEditorModel::setName(const QString &name) {
    if (mName == name)
        return;
    mName = name;
    emit nameChanged();
}

QString ScriptEditorModel::command() const {
    return mCommand;
}

void ScriptEditorModel::setCommand(const QString &command) {
    if (mCommand == command)
        return;
    mCommand = command;
    emit commandChanged();
}

bool ScriptEditorModel::isBlocking() const {
    return mBlocking;
}

void ScriptEditorModel::setBlocking(bool blocking) {
    if (mBlocking == blocking)
        return;
    mBlocking = blocking;
    emit blockingChanged();
}

bool ScriptEditorModel::nameTaken() const {
    if (mEditing && mName == mEditTarget)
        return false;
    return mStore.scriptExists(mName);
}

QString ScriptEditorModel::message() const {
    if (mName.isEmpty())
        return translated("Enter script name");
    if (nameTaken())
        return translated("A script with this same name exists");
    return {};
}

QString ScriptEditorModel::acceptText() const {
    if (!mName.isEmpty() && nameTaken())
        return translated("Replace");
    return mEditing ? translated("Save") : translated("Create");
}

bool ScriptEditorModel::canAccept() const {
    return acceptable();
}

bool ScriptEditorModel::acceptable() const {
    return !mName.isEmpty();
}

QString ScriptEditorModel::keywordsText() {
    return translated("Keywords:") + u' ' + kFileKeyword.toString();
}

QString ScriptEditorModel::executableDialogTitle() {
    return translated("Select an executable/script");
}

QStringList ScriptEditorModel::executableFilters() {
    return {u"Executable/script (*.exe *.bat)"_s};
}

void ScriptEditorModel::startNew() {
    mEditing = false;
    mEditTarget.clear();
    mName.clear();
    mCommand.clear();
    mBlocking = false;
    emit requestChanged();
    emit nameChanged();
    emit commandChanged();
    emit blockingChanged();
    open();
}

void ScriptEditorModel::startEdit(const QString &name) {
    const ScriptDefinition script = mStore.script(name);
    mEditing = true;
    mEditTarget = name;
    mName = name;
    mCommand = script.command;
    mBlocking = script.blocking;
    emit requestChanged();
    emit nameChanged();
    emit commandChanged();
    emit blockingChanged();
    open();
}

void ScriptEditorModel::setExecutablePath(const QString &path) {
    if (path.isEmpty())
        return;
    setCommand(u'"' + path + u"\" "_s + kFileKeyword.toString());
}

void ScriptEditorModel::setExecutable(const QUrl &url) {
    if (!url.isLocalFile()) {
        qWarning() << "ScriptEditorModel: not a local file:" << url;
        return;
    }
    setExecutablePath(url.toLocalFile());
}

QString ScriptEditorModel::resultName() const {
    return mName;
}

ScriptDefinition ScriptEditorModel::resultScript() const {
    return {.command = mCommand, .blocking = mBlocking};
}
