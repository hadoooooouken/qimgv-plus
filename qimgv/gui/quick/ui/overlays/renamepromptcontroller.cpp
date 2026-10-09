#include "renamepromptcontroller.h"

namespace {
constexpr QChar kExtensionSeparator = u'.';
} // namespace

RenamePromptController::RenamePromptController(QObject *parent) : QObject(parent) {}

QString RenamePromptController::name() const {
    return mName;
}

int RenamePromptController::selectionEnd() const {
    const qsizetype end = mName.lastIndexOf(kExtensionSeparator);
    return static_cast<int>(end < 0 ? mName.size() : end);
}

bool RenamePromptController::backdrop() const {
    return mBackdrop;
}

void RenamePromptController::setName(const QString &name) {
    if (mName == name)
        return;
    mName = name;
    emit nameChanged();
}

void RenamePromptController::setBackdrop(bool backdrop) {
    if (mBackdrop == backdrop)
        return;
    mBackdrop = backdrop;
    emit backdropChanged();
}

void RenamePromptController::setPassThroughShortcuts(const QStringList &shortcuts) {
    mPassThroughShortcuts = shortcuts;
}

bool RenamePromptController::accept(const QString &text) {
    if (text.isEmpty())
        return false;
    emit closeRequested();
    emit renameRequested(text);
    return true;
}

void RenamePromptController::cancel() {
    emit closeRequested();
}

bool RenamePromptController::passesThrough(const QString &shortcut) const {
    return !shortcut.isEmpty() && mPassThroughShortcuts.contains(shortcut);
}
