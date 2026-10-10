#include "dialogcoordinator.h"

DialogCoordinator::DialogCoordinator(QObject *parent) : QObject(parent) {}

BatchConverterDialogModel *DialogCoordinator::batchConverter() {
    return &mBatchConverter;
}

ConfirmationDialogModel *DialogCoordinator::confirmation() {
    return &mConfirmation;
}

FileReplaceDialogModel *DialogCoordinator::fileReplace() {
    return &mFileReplace;
}

PrintDialogModel *DialogCoordinator::print() {
    return &mPrint;
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
