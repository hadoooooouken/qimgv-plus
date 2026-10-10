#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "confirmationdialogmodel.h"
#include "filereplacedialogmodel.h"
#include "resizedialogmodel.h"
#include "savepathdialogmodel.h"
#include "textinputdialogmodel.h"

// The modal dialogs of the Qt Quick UI's dialog port: one view-model per
// dialog, each shown by its own window (DialogLayer.qml) while a request is
// open. The dialog port (QuickDialogPort) starts the requests and waits for
// their answers; QML presents them and forwards the person's answers.
//
// Settings-free. GUI thread only.
class DialogCoordinator final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(ConfirmationDialogModel *confirmation READ confirmation CONSTANT FINAL)
    Q_PROPERTY(FileReplaceDialogModel *fileReplace READ fileReplace CONSTANT FINAL)
    Q_PROPERTY(ResizeDialogModel *resize READ resize CONSTANT FINAL)
    Q_PROPERTY(SavePathDialogModel *savePath READ savePath CONSTANT FINAL)
    Q_PROPERTY(TextInputDialogModel *textInput READ textInput CONSTANT FINAL)

public:
    explicit DialogCoordinator(QObject *parent = nullptr);

    [[nodiscard]] ConfirmationDialogModel *confirmation();
    [[nodiscard]] FileReplaceDialogModel *fileReplace();
    [[nodiscard]] ResizeDialogModel *resize();
    [[nodiscard]] SavePathDialogModel *savePath();
    [[nodiscard]] TextInputDialogModel *textInput();

private:
    ConfirmationDialogModel mConfirmation;
    FileReplaceDialogModel mFileReplace;
    ResizeDialogModel mResize;
    SavePathDialogModel mSavePath;
    TextInputDialogModel mTextInput;
};
