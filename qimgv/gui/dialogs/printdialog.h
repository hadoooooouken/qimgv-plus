#ifndef PRINTDIALOG_H
#define PRINTDIALOG_H

#include <QDialog>
#include <QImage>
#include <memory>
#include "components/printing/imageprintsetup.h"
class QLabel;
class QComboBox;
class QRadioButton;
class QCheckBox;
class QPushButton;

class PrintDialog : public QDialog {
    Q_OBJECT

public:
    explicit PrintDialog(QWidget *parent = nullptr);
    ~PrintDialog();
    void setImage(std::shared_ptr<const QImage> _img);
    void setOutputPath(QString path);

private slots:
    void print();
    void exportPdf();
    void updatePreview();
    void setLandscape(bool mode);
    void onPrinterSelected(QString name);
    QString pdfPathDialog();

private:
    void setupUi();
    void saveSettings();
    [[nodiscard]] PrintOptions options() const;
    void showPrintFailure(const QString &message);

    QLabel *previewLabel = nullptr;
    QLabel *printerListPlaceholder = nullptr;
    QComboBox *printerListComboBox = nullptr;
    QRadioButton *portrait = nullptr;
    QRadioButton *landscape = nullptr;
    QRadioButton *grayscale = nullptr;
    QRadioButton *color = nullptr;
    QCheckBox *fitToPageCheckBox = nullptr;
    QPushButton *exportPdfButton = nullptr;
    QPushButton *printButton = nullptr;
    QPushButton *cancelButton = nullptr;

    std::shared_ptr<const QImage> img = nullptr;
    ImagePrintSetup printSetup;
    bool printPdfDefault = false;
};

#endif // PRINTDIALOG_H
