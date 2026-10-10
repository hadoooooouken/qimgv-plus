#pragma once

#include <QImage>
#include <QPageLayout>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QtPrintSupport/QPrinter>

#include <memory>

// Printing of one image: the print dialog's printing, PDF export and page
// preview, so all three lay out identical pages.

struct PrintOptions {
    bool landscape = false;
    bool color = false;
    bool fitToPage = true;
};

// The print choices that are stored between dialogs.
struct PrintPreferences {
    QString lastPrinter;
    // The last output was a PDF; the dialog then offers the export first.
    bool pdfDefault = false;
    PrintOptions options;
};

// Side of the square area the print dialogs fit the page preview into.
inline constexpr int kPrintPreviewExtent = 160;

// How the preview of the page is drawn.
struct PrintPreviewStyle {
    // The area the page is fitted into, in logical pixels.
    QSize area;
    qreal devicePixelRatio = 1.0;
    // Draws a thin page border (for light window backgrounds).
    bool pageFrame = false;
};

// Where an image of imageSize goes on a printable area of printableSize
// (device pixels, origin at the printable area's top left): scaled to fit,
// keeping its aspect ratio, when fitToPage or when it does not fit; centred
// horizontally at the top.
[[nodiscard]] QRectF imagePrintRect(QSizeF printableSize, QSizeF imageSize, bool fitToPage);

// The selected printer and the PDF printer of one print dialog. The PDF
// printer prints A4 pages. Without a selected printer, previews show the PDF
// page. GUI thread only.
class ImagePrintSetup {
public:
    ImagePrintSetup();
    ~ImagePrintSetup();

    ImagePrintSetup(const ImagePrintSetup &) = delete;
    ImagePrintSetup &operator=(const ImagePrintSetup &) = delete;

    // The suggested PDF path; empty suggests nothing.
    void setPdfOutputPath(const QString &path);
    [[nodiscard]] QString pdfOutputPath() const;

    // Prints to the installed printer of that name from now on, in the
    // current orientation.
    void selectPrinter(const QString &name);
    [[nodiscard]] bool hasPrinter() const;
    void setLandscape(bool landscape);

    // The page as it will be printed (or exported, without a printer), fitted
    // into style.area; grayscale unless options.color.
    [[nodiscard]] QImage renderPreview(const QImage &image, const PrintOptions &options,
                                       const PrintPreviewStyle &style) const;

    // False without a selected printer or when the printer could not be
    // started.
    [[nodiscard]] bool print(const QImage &image, const PrintOptions &options);
    // Writes the PDF to path; false when it could not be written.
    [[nodiscard]] bool exportPdf(const QString &path, const QImage &image,
                                 const PrintOptions &options);

private:
    [[nodiscard]] const QPrinter &previewPrinter() const;
    [[nodiscard]] static bool paint(QPrinter &printer, const QImage &image,
                                    const PrintOptions &options);

    QPageLayout::Orientation mOrientation = QPageLayout::Portrait;
    QPrinter mPdfPrinter;
    std::unique_ptr<QPrinter> mPrinter;
};
