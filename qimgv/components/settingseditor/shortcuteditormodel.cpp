#include "shortcuteditormodel.h"

#include <QCoreApplication>

#include "settingsstores.h"

namespace {
// The texts keep the translation contexts of the widget dialogs.
constexpr char kCreatorContext[] = "ShortcutCreatorDialog";
constexpr char kSettingsContext[] = "SettingsDialog";
} // namespace

ShortcutEditorModel::ShortcutEditorModel(IShortcutScriptStore &store, QObject *parent)
    : EditorSession(parent), mStore(store) {}

QString ShortcutEditorModel::title() const {
    return mTitle;
}

QStringList ShortcutEditorModel::actions() const {
    return mActions;
}

QStringList ShortcutEditorModel::scripts() const {
    return mScripts;
}

bool ShortcutEditorModel::isScriptSelected() const {
    return mScriptSelected;
}

void ShortcutEditorModel::setScriptSelected(bool selected) {
    if (mScriptSelected == selected)
        return;
    mScriptSelected = selected;
    emit selectionChanged();
    emit shortcutChanged();
}

int ShortcutEditorModel::actionIndex() const {
    return mActionIndex;
}

void ShortcutEditorModel::setActionIndex(int index) {
    if (index < 0 || index >= mActions.size() || index == mActionIndex)
        return;
    mActionIndex = index;
    emit selectionChanged();
}

int ShortcutEditorModel::scriptIndex() const {
    return mScriptIndex;
}

void ShortcutEditorModel::setScriptIndex(int index) {
    if (index < 0 || index >= mScripts.size() || index == mScriptIndex)
        return;
    mScriptIndex = index;
    emit selectionChanged();
}

QString ShortcutEditorModel::shortcut() const {
    return mShortcut;
}

QString ShortcutEditorModel::shortcutText() const {
    if (mShortcut.isEmpty())
        return QCoreApplication::translate(kCreatorContext, QT_TRANSLATE_NOOP("ShortcutCreatorDialog", "[Enter shortcut]"));
    return mShortcut;
}

QString ShortcutEditorModel::warning() const {
    return mWarning;
}

bool ShortcutEditorModel::canAccept() const {
    return acceptable();
}

bool ShortcutEditorModel::acceptable() const {
    if (mShortcut.isEmpty())
        return false;
    return mScriptSelected ? !mScripts.isEmpty() : !mActions.isEmpty();
}

void ShortcutEditorModel::startAdd() {
    startRequest(QCoreApplication::translate(kCreatorContext, QT_TRANSLATE_NOOP("ShortcutCreatorDialog", "Add shortcut")), -1);
    open();
}

void ShortcutEditorModel::startEdit(const ShortcutEntry &entry, int row) {
    startRequest(QCoreApplication::translate(kSettingsContext, QT_TRANSLATE_NOOP("SettingsDialog", "Edit shortcut")), row);
    if (entry.action.startsWith(kScriptActionPrefix)) {
        mScriptSelected = true;
        const qsizetype index = mScripts.indexOf(entry.action.mid(kScriptActionPrefix.size()));
        if (index >= 0)
            mScriptIndex = static_cast<int>(index);
    } else {
        const qsizetype index = mActions.indexOf(entry.action);
        if (index >= 0)
            mActionIndex = static_cast<int>(index);
    }
    // The shortcut being edited is not a conflict.
    mShortcut = entry.shortcut;
    emit selectionChanged();
    emit shortcutChanged();
    open();
}

void ShortcutEditorModel::startRequest(const QString &title, int row) {
    mTitle = title;
    mActions = mStore.actionNames();
    mScripts = mStore.scriptNames();
    mScriptSelected = false;
    mActionIndex = 0;
    mScriptIndex = 0;
    mShortcut.clear();
    mWarning.clear();
    mEditedRow = row;
    emit requestChanged();
    emit selectionChanged();
    emit shortcutChanged();
}

void ShortcutEditorModel::setShortcut(const QString &shortcut) {
    if (shortcut.isEmpty())
        return;
    mShortcut = shortcut;
    const QString boundAction = mStore.actionForShortcut(shortcut);
    if (boundAction.isEmpty()) {
        mWarning.clear();
    } else {
        mWarning = QCoreApplication::translate(kCreatorContext, QT_TRANSLATE_NOOP("ShortcutCreatorDialog", "This shortcut is used for action: ")) +
                   boundAction + QCoreApplication::translate(kCreatorContext, QT_TRANSLATE_NOOP("ShortcutCreatorDialog", ". Replace?"));
    }
    emit shortcutChanged();
}

ShortcutEntry ShortcutEditorModel::result() const {
    QString action;
    if (mScriptSelected)
        action = kScriptActionPrefix.toString() + mScripts.value(mScriptIndex);
    else
        action = mActions.value(mActionIndex);
    return {.action = action, .shortcut = mShortcut};
}

int ShortcutEditorModel::editedRow() const {
    return mEditedRow;
}
