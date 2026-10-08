#include "widgetportadapters.h"

#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>

#include "gui/dialogs/batchconverterdialog.h"
#include "gui/dialogs/printdialog.h"
#include "gui/mainwindow.h"

namespace {
constexpr qreal kConcealedWindowOpacity = 0.0;
constexpr qreal kRevealedWindowOpacity = 1.0;
} // namespace

//------------------------------------------------------------------------------
WidgetNotificationAdapter::WidgetNotificationAdapter(MW &window)
    : window(window) {
}

void WidgetNotificationAdapter::showNotification(const NotificationRequest &request) {
    window.showNotification(request);
}

void WidgetNotificationAdapter::hideNotifications() {
    window.hideMessage();
}

//------------------------------------------------------------------------------
WidgetDialogAdapter::WidgetDialogAdapter(MW &window)
    : window(window) {
}

ConfirmationResult WidgetDialogAdapter::confirm(const ConfirmationRequest &request) {
    return {window.showConfirmation(request.title, request.message)};
}

FileReplaceDecision WidgetDialogAdapter::resolveFileReplace(const FileReplaceRequest &request) {
    return window.fileReplaceDialog(request);
}

SavePathResult WidgetDialogAdapter::requestSavePath(const SavePathRequest &request) {
    return {window.getSaveFileName(request.suggestedPath)};
}

std::optional<ResizeRequest> WidgetDialogAdapter::requestResize(QSize initialSize) {
    return window.showResizeDialog(initialSize);
}

TextInputResult WidgetDialogAdapter::requestText(const TextInputRequest &request) {
    TextInputResult result;
    result.text = QInputDialog::getText(&window, request.title, request.label,
                                        QLineEdit::Normal, request.initialText,
                                        &result.accepted);
    return result;
}

BatchConversionResult WidgetDialogAdapter::runBatchConverter(const BatchConversionRequest &request) {
    BatchConverterDialog dialog(request.filePaths, &window, request.defaultOutputDirectory);
    dialog.exec();
    return {dialog.conversionWasStarted()};
}

void WidgetDialogAdapter::print(const PrintRequest &request) {
    if (!request.image) {
        qWarning() << "Print request without an image; nothing to print";
        return;
    }
    PrintDialog dialog(&window);
    dialog.setImage(request.image);
    dialog.setOutputPath(request.pdfOutputPath);
    dialog.exec();
}

//------------------------------------------------------------------------------
WidgetViewerAdapter::WidgetViewerAdapter(MW &window)
    : window(window) {
}

void WidgetViewerAdapter::showImage(std::shared_ptr<const QImage> image, const QString &filePath) {
    window.showImage(std::move(image), filePath);
}

void WidgetViewerAdapter::showAnimation(const QString &filePath, const QString &format, QSize size) {
    window.showAnimation(filePath, format, size);
}

void WidgetViewerAdapter::closeImage() {
    window.closeImage();
}

void WidgetViewerAdapter::showScaledImage(const QImage &scaled) {
    window.onScalingFinished(scaled);
}

void WidgetViewerAdapter::showUpscaledCrop(const QImage &crop, const QRect &originalRect) {
    window.onUpscaleFinished(crop, originalRect);
}

void WidgetViewerAdapter::hideUpscaledCrop() {
    window.hideUpscaledCrop();
}

void WidgetViewerAdapter::refreshScaling() {
    window.refreshScaling();
}

QRect WidgetViewerAdapter::visibleOriginalImageRect() const {
    return window.visibleOriginalImageRect();
}

float WidgetViewerAdapter::currentScale() const {
    return window.currentScale();
}

float WidgetViewerAdapter::devicePixelRatio() const {
    return window.getDpr();
}

bool WidgetViewerAdapter::isBusyInteracting() const {
    return window.isBusyInteracting();
}

bool WidgetViewerAdapter::isRenderingSettled() const {
    return window.isDocumentRenderingSettled();
}

bool WidgetViewerAdapter::panoramaMode() const {
    return window.panoramaMode();
}

//------------------------------------------------------------------------------
WidgetShellAdapter::WidgetShellAdapter(MW &window)
    : window(window) {
}

void WidgetShellAdapter::setDirectoryPath(const QString &path) {
    window.setDirectoryPath(path);
}

void WidgetShellAdapter::setCurrentInfo(const ShellFileInfo &info) {
    window.setCurrentInfo(info);
}

void WidgetShellAdapter::setMetadata(const MetadataEntries &entries) {
    window.setExifInfo(entries);
}

void WidgetShellAdapter::notifySortingChanged(SortingMode mode) {
    window.onSortingChanged(mode);
}

void WidgetShellAdapter::notifyFolderSortingChanged(SortingMode mode) {
    window.onFolderSortingChanged(mode);
}

void WidgetShellAdapter::refreshFolderTree(const QString &directoryPath) {
    const std::shared_ptr<FolderViewProxy> folderView = window.getFolderView();
    if (!folderView) {
        qWarning() << "Cannot refresh the folder tree: the folder view does not exist";
        return;
    }
    folderView->refreshFilesystemModel(directoryPath);
}

void WidgetShellAdapter::setSaveOverlayVisible(bool visible) {
    if (visible)
        window.showSaveOverlay();
    else
        window.hideSaveOverlay();
}

bool WidgetShellAdapter::isCropPanelActive() const {
    return window.isCropPanelActive();
}

void WidgetShellAdapter::toggleCropPanel() {
    window.triggerCropPanel();
}

void WidgetShellAdapter::toggleFullscreenInfoBar() {
    window.toggleFullscreenInfoBar();
}

void WidgetShellAdapter::toggleRenamePrompt(const QString &currentName) {
    window.toggleRenameOverlay(currentName);
}

//------------------------------------------------------------------------------
WidgetWindowAdapter::WidgetWindowAdapter(MW &window)
    : window(window) {
}

void WidgetWindowAdapter::showWindow() {
    window.showDefault();
}

void WidgetWindowAdapter::hideWindow() {
    window.hide();
}

bool WidgetWindowAdapter::isWindowVisible() const {
    return window.isVisible();
}

void WidgetWindowAdapter::setWindowConcealed(bool concealed) {
    window.setWindowOpacity(concealed ? kConcealedWindowOpacity : kRevealedWindowOpacity);
}

void WidgetWindowAdapter::raiseAndActivateWindow() {
    window.raise();
    window.activateWindow();
}

WId WidgetWindowAdapter::nativeWindowHandle() const {
    return window.winId();
}

void WidgetWindowAdapter::saveWindowGeometry() {
    window.saveWindowGeometry();
}

void WidgetWindowAdapter::setWindowUpdatesSuspended(bool suspended) {
    window.setUpdatesEnabled(!suspended);
    if (!suspended)
        window.repaint();
}
