#include "dialogcoordinator.h"

DialogCoordinator::DialogCoordinator(QObject *parent) : QObject(parent) {}

ConfirmationDialogModel *DialogCoordinator::confirmation() {
    return &mConfirmation;
}

FileReplaceDialogModel *DialogCoordinator::fileReplace() {
    return &mFileReplace;
}

ResizeDialogModel *DialogCoordinator::resize() {
    return &mResize;
}

SavePathDialogModel *DialogCoordinator::savePath() {
    return &mSavePath;
}

TextInputDialogModel *DialogCoordinator::textInput() {
    return &mTextInput;
}
