#include "settingsdialogcontroller.h"

#include <QDebug>

SettingsDialogController::SettingsDialogController(SettingsEditorModel &editor, QObject *parent)
    : QObject(parent), mEditor(editor) {}

bool SettingsDialogController::isOpen() const {
    return mOpen;
}

bool SettingsDialogController::isCreated() const {
    return mCreated;
}

int SettingsDialogController::page() const {
    return mPage;
}

void SettingsDialogController::setPage(int page) {
    if (page < 0 || page >= pageCount()) {
        qWarning() << "SettingsDialogController: no settings page" << page;
        return;
    }
    if (mPage == page)
        return;
    mPage = page;
    emit pageChanged();
}

int SettingsDialogController::pageCount() {
    return static_cast<int>(SettingsEditorModel::Page::Count);
}

SettingsEditorModel *SettingsDialogController::editor() {
    return &mEditor;
}

void SettingsDialogController::show(SettingsEditorModel::Page page) {
    setPage(static_cast<int>(page));
    if (mOpen)
        return;
    mEditor.load();
    if (!mCreated) {
        mCreated = true;
        emit createdChanged();
    }
    mOpen = true;
    emit openChanged();
}

void SettingsDialogController::accept() {
    if (!mOpen)
        return;
    mEditor.apply();
    close();
}

void SettingsDialogController::applyChanges() {
    if (!mOpen)
        return;
    mEditor.apply();
}

void SettingsDialogController::dismiss() {
    if (!mOpen)
        return;
    close();
}

void SettingsDialogController::close() {
    mOpen = false;
    emit openChanged();
}
