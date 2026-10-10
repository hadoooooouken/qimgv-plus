#include "dialogsession.h"

#include <QDebug>
#include <QEventLoop>

DialogSession::DialogSession(QObject *parent) : QObject(parent) {}

bool DialogSession::isOpen() const {
    return mOpen;
}

bool DialogSession::isCreated() const {
    return mCreated;
}

bool DialogSession::wasAnswered() const {
    return mAnswered;
}

bool DialogSession::canStart(const char *dialogName) const {
    if (mOpen) {
        qWarning().noquote() << "Qt Quick UI: the" << dialogName
                             << "dialog is already open; the new request was declined";
        return false;
    }
    return true;
}

void DialogSession::open() {
    mAnswered = false;
    mOpen = true;
    if (!mCreated) {
        mCreated = true;
        emit createdChanged();
    }
    emit openChanged();
}

void DialogSession::conclude() {
    if (mOpen)
        close(true);
}

void DialogSession::abandon() {
    if (mOpen)
        close(false);
}

void DialogSession::close(bool answered) {
    mAnswered = answered;
    mOpen = false;
    emit openChanged();
    emit finished();
}

bool runModalDialog(DialogSession &dialog) {
    if (!dialog.isOpen()) {
        qWarning() << "Qt Quick UI: a modal dialog was awaited without an open request";
        return false;
    }
    QEventLoop loop;
    QObject::connect(&dialog, &DialogSession::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (dialog.isOpen()) {
        qWarning() << "Qt Quick UI: a modal dialog was closed without an answer because "
                      "the application is quitting";
        dialog.abandon();
    }
    return dialog.wasAnswered();
}
