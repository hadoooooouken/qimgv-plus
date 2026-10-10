#include "imageprintsetup.h"

#include <QColor>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QtPrintSupport/QPrinterInfo>

namespace {
using namespace Qt::StringLiterals;

// An empty output file name switches a QPrinter back to the native format,
// so "no suggested path" is a blank name.
const QString kNoPdfOutputPath = u" "_s;

constexpr QColor kPageColor(255, 255, 255);
constexpr QColor kPageFrameColor(0, 0, 0);
constexpr qreal kPageFrameOpacity = 0.25;
// Half a pixel puts the one pixel frame on the pixel grid.
constexpr qreal kPageFrameInset = 0.5;
constexpr qreal kPageFrameShrink = 1.0;

QRectF imagePrintRect(const QPrinter &printer, QSize imageSize, bool fitToPage) {
    return imagePrintRect(printer.pageRect(QPrinter::DevicePixel).size(), QSizeF(imageSize),
                          fitToPage);
}
} // namespace

QRectF imagePrintRect(QSizeF printableSize, QSizeF imageSize, bool fitToPage) {
    const QRectF pageRect(QPointF(0, 0), printableSize);
    QRectF imageRect(QPointF(0, 0), imageSize);
    if (fitToPage || imageRect.width() > pageRect.width() ||
        imageRect.height() > pageRect.height())
        imageRect.setSize(imageRect.size().scaled(pageRect.size(), Qt::KeepAspectRatio));
    imageRect.moveCenter(pageRect.center());
    imageRect.moveTop(pageRect.top());
    return imageRect;
}

ImagePrintSetup::ImagePrintSetup() {
    mPdfPrinter.setOutputFormat(QPrinter::PdfFormat);
    mPdfPrinter.setPageSize(QPageSize(QPageSize::A4));
    mPdfPrinter.setOutputFileName(kNoPdfOutputPath);
}

ImagePrintSetup::~ImagePrintSetup() = default;

void ImagePrintSetup::setPdfOutputPath(const QString &path) {
    mPdfPrinter.setOutputFileName(path.isEmpty() ? kNoPdfOutputPath : path);
}

QString ImagePrintSetup::pdfOutputPath() const {
    const QString path = mPdfPrinter.outputFileName();
    return path == kNoPdfOutputPath ? QString() : path;
}

void ImagePrintSetup::selectPrinter(const QString &name) {
    mPrinter = std::make_unique<QPrinter>(QPrinterInfo::printerInfo(name));
    mPrinter->setPageOrientation(mOrientation);
}

bool ImagePrintSetup::hasPrinter() const {
    return mPrinter != nullptr;
}

void ImagePrintSetup::setLandscape(bool landscape) {
    mOrientation = landscape ? QPageLayout::Landscape : QPageLayout::Portrait;
    if (mPrinter)
        mPrinter->setPageOrientation(mOrientation);
    mPdfPrinter.setPageOrientation(mOrientation);
}

const QPrinter &ImagePrintSetup::previewPrinter() const {
    return mPrinter ? *mPrinter : mPdfPrinter;
}

QImage ImagePrintSetup::renderPreview(const QImage &image, const PrintOptions &options,
                                      const PrintPreviewStyle &style) const {
    if (image.isNull() || style.area.isEmpty())
        return {};
    const QPrinter &printer = previewPrinter();
    const QPageLayout layout = printer.pageLayout();
    const int resolution = printer.resolution();
    const QRectF imageRect = imagePrintRect(printer, image.size(), options.fitToPage);
    const QRectF fullRect = layout.fullRectPixels(resolution);
    const QMarginsF margins(layout.marginsPixels(resolution));
    if (fullRect.isEmpty())
        return {};

    // The page with its margins, scaled into the area; the image rectangle
    // is rounded to whole pixels (close enough for a preview).
    const QRect pageRect =
        QRectF(QPointF(0, 0), fullRect.size().scaled(QSizeF(style.area), Qt::KeepAspectRatio))
            .toRect();
    const qreal scale = pageRect.width() / fullRect.width();
    const QRect scaledImageRect =
        QRectF((imageRect.left() + margins.left()) * scale,
               (imageRect.top() + margins.top()) * scale, imageRect.width() * scale,
               imageRect.height() * scale)
            .toRect();

    const qreal dpr = style.devicePixelRatio;
    QImage page(pageRect.size() * dpr, QImage::Format_RGB32);
    page.setDevicePixelRatio(dpr);
    page.fill(kPageColor);

    QImage scaledImage = image.scaled(scaledImageRect.size() * dpr, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation);
    if (!options.color)
        scaledImage = scaledImage.convertToFormat(QImage::Format_Grayscale8);
    scaledImage.setDevicePixelRatio(dpr);

    QPainter painter(&page);
    painter.drawImage(scaledImageRect.topLeft(), scaledImage);
    if (style.pageFrame) {
        painter.setOpacity(kPageFrameOpacity);
        painter.setPen(kPageFrameColor);
        painter.drawRect(QRectF(QPointF(kPageFrameInset, kPageFrameInset),
                                page.deviceIndependentSize() -
                                    QSizeF(kPageFrameShrink, kPageFrameShrink)));
    }
    painter.end();
    return page;
}

bool ImagePrintSetup::print(const QImage &image, const PrintOptions &options) {
    if (!mPrinter)
        return false;
    return paint(*mPrinter, image, options);
}

bool ImagePrintSetup::exportPdf(const QString &path, const QImage &image,
                                const PrintOptions &options) {
    if (path.isEmpty())
        return false;
    mPdfPrinter.setOutputFileName(path);
    return paint(mPdfPrinter, image, options);
}

bool ImagePrintSetup::paint(QPrinter &printer, const QImage &image, const PrintOptions &options) {
    if (image.isNull())
        return false;
    printer.setColorMode(options.color ? QPrinter::Color : QPrinter::GrayScale);
    QPainter painter;
    if (!painter.begin(&printer))
        return false;
    painter.drawImage(imagePrintRect(printer, image.size(), options.fitToPage), image);
    return painter.end();
}
