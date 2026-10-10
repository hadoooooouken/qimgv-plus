#pragma once

#include <QImage>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>

#include "components/printing/imageprintsetup.h"
#include "dialogsession.h"
#include "gui/quick/render/thumbnailitem.h"

// What the print dialog starts from.
struct PrintDialogInput {
    std::shared_ptr<const QImage> image;
    // Suggested path of an exported PDF.
    QString pdfOutputPath;
    // The installed printers; none leaves only the PDF export.
    QStringList printers;
    QString defaultPrinter;
    PrintPreferences preferences;
    // Device pixels per logical pixel of the preview.
    qreal devicePixelRatio = 1.0;
};

// Printing or exporting the current image as a PDF (PrintDialog.qml), with
// the rules of the former widget PrintDialog: printer, orientation, colour mode and
// fit to page, a preview of the page, the last used printer and output
// remembered. Printing and the preview go through ImagePrintSetup, so both
// show the same page.
//
// A failed print or export keeps the dialog open with errorText. Every
// close reports the preferences to store, as the widget dialog stored them
// when it closed.
//
// Owned by DialogCoordinator. GUI thread only.
class PrintDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(QStringList printers READ printers NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool printersAvailable READ printersAvailable NOTIFY stateChanged FINAL)
    Q_PROPERTY(int printerIndex READ printerIndex NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool landscape READ landscape NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool color READ color NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool fitToPage READ fitToPage NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool exportFirst READ exportFirst NOTIFY stateChanged FINAL)
    Q_PROPERTY(QUrl pdfFileUrl READ pdfFileUrl NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged FINAL)
    Q_PROPERTY(ThumbnailHandle preview READ preview NOTIFY previewChanged FINAL)
    Q_PROPERTY(int previewExtent READ previewExtent CONSTANT FINAL)

public:
    explicit PrintDialogModel(QObject *parent = nullptr);
    ~PrintDialogModel() override;

    [[nodiscard]] QStringList printers() const;
    [[nodiscard]] bool printersAvailable() const;
    // -1 without printers.
    [[nodiscard]] int printerIndex() const;
    [[nodiscard]] bool landscape() const;
    [[nodiscard]] bool color() const;
    [[nodiscard]] bool fitToPage() const;
    // The PDF export is offered first: the last output was a PDF, or there
    // is no printer.
    [[nodiscard]] bool exportFirst() const;
    [[nodiscard]] QUrl pdfFileUrl() const;
    [[nodiscard]] QString errorText() const;
    // The page as it will be printed.
    [[nodiscard]] ThumbnailHandle preview() const;
    [[nodiscard]] int previewExtent() const;

    [[nodiscard]] std::optional<PrintPreferences> preferencesToStore() const;

    // Opens the dialog for input; false while another request is open or
    // without an image.
    [[nodiscard]] bool start(const PrintDialogInput &input);

    Q_INVOKABLE void selectPrinter(int index);
    Q_INVOKABLE void setLandscape(bool landscape);
    Q_INVOKABLE void setColor(bool color);
    Q_INVOKABLE void setFitToPage(bool fit);
    // Prints on the selected printer and closes the dialog.
    Q_INVOKABLE void print();
    // Writes the PDF to url (a local file) and closes the dialog.
    Q_INVOKABLE void exportPdf(const QUrl &url);
    Q_INVOKABLE void reject();

signals:
    void stateChanged();
    void previewChanged();

private:
    void updatePreview();
    void fail(const QString &message);
    [[nodiscard]] PrintOptions options() const;

    PrintDialogInput mInput;
    std::unique_ptr<ImagePrintSetup> mSetup;
    int mPrinterIndex = -1;
    bool mPdfDefault = false;
    QString mErrorText;
    QImage mPreview;
    std::optional<PrintPreferences> mPreferences;
};
