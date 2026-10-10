#include "quickdialogport.h"

#include <QDebug>
#include <QGuiApplication>
#include <QImageWriter>
#include <QScreen>

#include "gui/quick/ui/dialogs/dialogcoordinator.h"
#include "settings.h"

namespace {
QSize primaryScreenSize() {
    const QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "Qt Quick UI: no primary screen; the resize dialog's desktop sizes "
                      "keep the image size";
        return {};
    }
    return screen->size();
}
} // namespace

QuickDialogPort::QuickDialogPort(DialogCoordinator &dialogs, Settings &settings)
    : mDialogs(dialogs), mSettings(settings) {}

ConfirmationResult QuickDialogPort::confirm(const ConfirmationRequest &request) {
    ConfirmationDialogModel &dialog = *mDialogs.confirmation();
    if (!dialog.start(request))
        return {.accepted = false};
    runModalDialog(dialog);
    return dialog.result();
}

FileReplaceDecision QuickDialogPort::resolveFileReplace(const FileReplaceRequest &request) {
    FileReplaceDialogModel &dialog = *mDialogs.fileReplace();
    if (!dialog.start(request))
        return {.yes = false, .all = false, .cancel = true};
    runModalDialog(dialog);
    return dialog.result();
}

SavePathResult QuickDialogPort::requestSavePath(const SavePathRequest &request) {
    SavePathDialogModel &dialog = *mDialogs.savePath();
    if (!dialog.start(request, QImageWriter::supportedImageFormats()))
        return {};
    runModalDialog(dialog);
    return dialog.result();
}

std::optional<ResizeRequest> QuickDialogPort::requestResize(QSize initialSize) {
    ResizeDialogModel &dialog = *mDialogs.resize();
    const ResizeDialogInput input{
        .originalSize = initialSize,
        .desktopSize = primaryScreenSize(),
        .upscaylModels = mSettings.availableUpscaylModels(),
        .useUpscayl = mSettings.resizeUseUpscayl(),
        .upscaylModel = mSettings.resizeUpscaylModel(),
    };
    if (!dialog.start(input))
        return std::nullopt;
    runModalDialog(dialog);
    if (const std::optional<ResizePreferences> preferences = dialog.preferencesToStore()) {
        mSettings.setResizeUseUpscayl(preferences->useUpscayl);
        mSettings.setResizeUpscaylModel(preferences->upscaylModel);
        mSettings.sync();
    }
    return dialog.result();
}

TextInputResult QuickDialogPort::requestText(const TextInputRequest &request) {
    TextInputDialogModel &dialog = *mDialogs.textInput();
    if (!dialog.start(request))
        return {.accepted = false, .text = {}};
    runModalDialog(dialog);
    return dialog.result();
}

BatchConversionResult QuickDialogPort::runBatchConverter(const BatchConversionRequest &request) {
    return mNotYetAvailable.runBatchConverter(request);
}

void QuickDialogPort::print(const PrintRequest &request) {
    mNotYetAvailable.print(request);
}
