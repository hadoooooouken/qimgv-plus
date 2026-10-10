#include "editorsession.h"

EditorSession::EditorSession(QObject *parent) : QObject(parent) {}

bool EditorSession::isOpen() const {
    return mOpen;
}

bool EditorSession::isCreated() const {
    return mCreated;
}

void EditorSession::accept() {
    if (!mOpen || !acceptable())
        return;
    close();
    emit accepted();
}

void EditorSession::reject() {
    if (!mOpen)
        return;
    close();
    emit rejected();
}

void EditorSession::open() {
    if (!mCreated) {
        mCreated = true;
        emit createdChanged();
    }
    if (!mOpen) {
        mOpen = true;
        emit openChanged();
    }
}

void EditorSession::close() {
    mOpen = false;
    emit openChanged();
}
