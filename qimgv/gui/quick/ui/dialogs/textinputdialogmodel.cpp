#include "textinputdialogmodel.h"

TextInputDialogModel::TextInputDialogModel(QObject *parent) : DialogSession(parent) {}

QString TextInputDialogModel::title() const {
    return mRequest.title;
}

QString TextInputDialogModel::label() const {
    return mRequest.label;
}

QString TextInputDialogModel::initialText() const {
    return mRequest.initialText;
}

TextInputResult TextInputDialogModel::result() const {
    return mResult;
}

bool TextInputDialogModel::start(const TextInputRequest &request) {
    if (!canStart("text input"))
        return false;
    mResult = {.accepted = false, .text = {}};
    mRequest = request;
    emit requestChanged();
    open();
    return true;
}

void TextInputDialogModel::accept(const QString &text) {
    if (!isOpen())
        return;
    mResult = {.accepted = true, .text = text};
    conclude();
}

void TextInputDialogModel::reject() {
    if (!isOpen())
        return;
    mResult = {.accepted = false, .text = {}};
    conclude();
}
