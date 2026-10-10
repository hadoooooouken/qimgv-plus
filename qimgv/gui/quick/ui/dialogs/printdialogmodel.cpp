#include "printdialogmodel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSize>

#include <algorithm>

PrintDialogModel::PrintDialogModel(QObject *parent) : DialogSession(parent) {
    // Every close stores the choices; the printers and the image are
    // released until the next request.
    connect(this, &DialogSession::finished, this, [this]() {
        PrintPreferences preferences = mInput.preferences;
        preferences.options = options();
        preferences.pdfDefault = mPdfDefault;
        if (mPrinterIndex >= 0)
            preferences.lastPrinter = mInput.printers[mPrinterIndex];
        mPreferences = preferences;
        mSetup.reset();
        mInput.image.reset();
        mPreview = {};
        emit previewChanged();
    });
}

PrintDialogModel::~PrintDialogModel() = default;

QStringList PrintDialogModel::printers() const {
    return mInput.printers;
}

bool PrintDialogModel::printersAvailable() const {
    return !mInput.printers.isEmpty();
}

int PrintDialogModel::printerIndex() const {
    return mPrinterIndex;
}

bool PrintDialogModel::landscape() const {
    return mInput.preferences.options.landscape;
}

bool PrintDialogModel::color() const {
    return mInput.preferences.options.color;
}

bool PrintDialogModel::fitToPage() const {
    return mInput.preferences.options.fitToPage;
}

bool PrintDialogModel::exportFirst() const {
    return mPdfDefault || !printersAvailable();
}

QUrl PrintDialogModel::pdfFileUrl() const {
    return mInput.pdfOutputPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(mInput.pdfOutputPath);
}

QString PrintDialogModel::errorText() const {
    return mErrorText;
}

ThumbnailHandle PrintDialogModel::preview() const {
    // No source size: the page is drawn at the size it was rendered for.
    return {.image = mPreview, .sourceSize = QSize()};
}

int PrintDialogModel::previewExtent() const {
    return kPrintPreviewExtent;
}

std::optional<PrintPreferences> PrintDialogModel::preferencesToStore() const {
    return mPreferences;
}

PrintOptions PrintDialogModel::options() const {
    return mInput.preferences.options;
}

bool PrintDialogModel::start(const PrintDialogInput &input) {
    if (!canStart("print"))
        return false;
    if (!input.image || input.image->isNull()) {
        qWarning() << "Qt Quick UI: print request without an image; nothing to print";
        return false;
    }

    mInput = input;
    mPreferences.reset();
    mErrorText.clear();
    mSetup = std::make_unique<ImagePrintSetup>();
    mSetup->setPdfOutputPath(input.pdfOutputPath);
    mSetup->setLandscape(input.preferences.options.landscape);
    // The last printer when it is still installed, the default one otherwise.
    mPrinterIndex = -1;
    if (printersAvailable()) {
        qsizetype index = input.printers.indexOf(input.preferences.lastPrinter);
        if (index < 0)
            index = input.printers.indexOf(input.defaultPrinter);
        mPrinterIndex = static_cast<int>(std::max<qsizetype>(index, 0));
        mSetup->selectPrinter(input.printers[mPrinterIndex]);
    }
    // Without printers, the widget dialog never offered printing first.
    mPdfDefault = printersAvailable() && input.preferences.pdfDefault;
    updatePreview();
    emit stateChanged();
    open();
    return true;
}

// Called from start() and while the dialog is open, so the setup and the
// image exist.
void PrintDialogModel::updatePreview() {
    const PrintPreviewStyle style{.area = QSize(kPrintPreviewExtent, kPrintPreviewExtent),
                                  .devicePixelRatio = mInput.devicePixelRatio,
                                  .pageFrame = false};
    mPreview = mSetup->renderPreview(*mInput.image, options(), style);
    emit previewChanged();
}

void PrintDialogModel::selectPrinter(int index) {
    if (!isOpen() || index < 0 || index >= mInput.printers.size() || index == mPrinterIndex)
        return;
    mPrinterIndex = index;
    mSetup->selectPrinter(mInput.printers[index]);
    updatePreview();
    emit stateChanged();
}

void PrintDialogModel::setLandscape(bool landscape) {
    if (!isOpen())
        return;
    mInput.preferences.options.landscape = landscape;
    mSetup->setLandscape(landscape);
    updatePreview();
    emit stateChanged();
}

void PrintDialogModel::setColor(bool color) {
    if (!isOpen())
        return;
    mInput.preferences.options.color = color;
    updatePreview();
    emit stateChanged();
}

void PrintDialogModel::setFitToPage(bool fit) {
    if (!isOpen())
        return;
    mInput.preferences.options.fitToPage = fit;
    updatePreview();
    emit stateChanged();
}

void PrintDialogModel::fail(const QString &message) {
    qWarning().noquote() << "Qt Quick UI: print dialog:" << message;
    mErrorText = message;
    emit stateChanged();
}

void PrintDialogModel::print() {
    if (!isOpen())
        return;
    if (!mSetup->hasPrinter()) {
        qWarning() << "Qt Quick UI: print requested without a printer";
        conclude();
        return;
    }
    if (!mSetup->print(*mInput.image, options())) {
        fail(QCoreApplication::translate("PrintDialog", "Could not print the image."));
        return;
    }
    mPdfDefault = false;
    conclude();
}

void PrintDialogModel::exportPdf(const QUrl &url) {
    if (!isOpen())
        return;
    if (!url.isLocalFile()) {
        qWarning() << "Qt Quick UI: the PDF must be saved to a local file, not" << url;
        return;
    }
    if (!mSetup->exportPdf(url.toLocalFile(), *mInput.image, options())) {
        fail(QCoreApplication::translate("PrintDialog", "Could not export the PDF."));
        return;
    }
    mPdfDefault = true;
    conclude();
}

void PrintDialogModel::reject() {
    conclude();
}
