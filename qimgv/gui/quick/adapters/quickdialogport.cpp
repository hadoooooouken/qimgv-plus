#include "quickdialogport.h"

#include <QDebug>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QScreen>
#include <QtPrintSupport/QPrinterInfo>

#include "gui/quick/adapters/appbatchconversionservice.h"
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

qreal primaryScreenDevicePixelRatio() {
    constexpr qreal kFallbackDevicePixelRatio = 1.0;
    const QScreen *screen = QGuiApplication::primaryScreen();
    return screen ? screen->devicePixelRatio() : kFallbackDevicePixelRatio;
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
    BatchConverterDialogModel &dialog = *mDialogs.batchConverter();
    // The resize fields start from the first file, as in the widget dialog.
    const QSize originalSize = request.filePaths.isEmpty()
                                   ? QSize()
                                   : QImageReader(request.filePaths.first()).size();
    const BatchConverterDialogInput input{
        .filePaths = request.filePaths,
        .defaultOutputDirectory = request.defaultOutputDirectory,
        .originalSize = originalSize,
        .saveQualities = {.jpeg = mSettings.JPEGSaveQuality(),
                          .png = mSettings.pngSaveQuality(),
                          .modern = mSettings.modernSaveQuality()},
        .upscaylModels = mSettings.availableUpscaylModels(),
        .useUpscayl = mSettings.resizeUseUpscayl(),
        .upscaylModel = mSettings.batchUpscaylModel(),
        .devicePixelRatio = primaryScreenDevicePixelRatio(),
    };
    // Destroyed after the dialog closed, stopping a batch that still runs.
    AppBatchConversionService service;
    if (!dialog.start(input, service))
        return {.conversionStarted = false};
    runModalDialog(dialog);
    if (const std::optional<BatchConverterPreferences> preferences = dialog.preferencesToStore()) {
        mSettings.setResizeUseUpscayl(preferences->useUpscayl);
        if (!preferences->upscaylModel.isEmpty())
            mSettings.setBatchUpscaylModel(preferences->upscaylModel);
        mSettings.sync();
    }
    return dialog.result();
}

void QuickDialogPort::print(const PrintRequest &request) {
    PrintDialogModel &dialog = *mDialogs.print();
    const PrintDialogInput input{
        .image = request.image,
        .pdfOutputPath = request.pdfOutputPath,
        .printers = QPrinterInfo::availablePrinterNames(),
        .defaultPrinter = QPrinterInfo::defaultPrinterName(),
        .preferences = {.lastPrinter = mSettings.lastPrinter(),
                        .pdfDefault = mSettings.printPdfDefault(),
                        .options = {.landscape = mSettings.printLandscape(),
                                    .color = mSettings.printColor(),
                                    .fitToPage = mSettings.printFitToPage()}},
        .devicePixelRatio = primaryScreenDevicePixelRatio(),
    };
    if (!dialog.start(input))
        return;
    runModalDialog(dialog);
    if (const std::optional<PrintPreferences> preferences = dialog.preferencesToStore()) {
        mSettings.setPrintLandscape(preferences->options.landscape);
        mSettings.setPrintColor(preferences->options.color);
        mSettings.setPrintFitToPage(preferences->options.fitToPage);
        mSettings.setPrintPdfDefault(preferences->pdfDefault);
        if (!preferences->lastPrinter.isEmpty())
            mSettings.setLastPrinter(preferences->lastPrinter);
    }
}
