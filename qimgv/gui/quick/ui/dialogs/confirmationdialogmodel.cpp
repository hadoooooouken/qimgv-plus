#include "confirmationdialogmodel.h"

ConfirmationDialogModel::ConfirmationDialogModel(QObject *parent) : DialogSession(parent) {}

QString ConfirmationDialogModel::title() const {
    return mRequest.title;
}

QString ConfirmationDialogModel::message() const {
    return mRequest.message;
}

ConfirmationResult ConfirmationDialogModel::result() const {
    return mResult;
}

bool ConfirmationDialogModel::start(const ConfirmationRequest &request) {
    if (!canStart("confirmation"))
        return false;
    mResult = {.accepted = false};
    mRequest = request;
    emit requestChanged();
    open();
    return true;
}

void ConfirmationDialogModel::accept() {
    finish(true);
}

void ConfirmationDialogModel::reject() {
    finish(false);
}

void ConfirmationDialogModel::finish(bool accepted) {
    if (!isOpen())
        return;
    mResult = {.accepted = accepted};
    conclude();
}
