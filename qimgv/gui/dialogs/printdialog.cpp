#include "printdialog.h"
#include "settings.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QDebug>
#include <QFrame>
#include <QFileDialog>
#include <QMessageBox>
#include <QPalette>
#include <QtPrintSupport/QPrinterInfo>

namespace {
// Window backgrounds lighter than this get a frame around the white page.
constexpr float kPageFrameWindowValue = 0.45f;
}

PrintDialog::PrintDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    previewLabel->setContentsMargins(0,0,0,0);
    QStringList printerList = QPrinterInfo::availablePrinterNames();
    if(printerList.isEmpty()) {
        printerListComboBox->hide();
        printButton->setEnabled(false);
        exportPdfButton->setFocus();
    } else {
        printerListPlaceholder->hide();
        printerListComboBox->addItems(printerList);
        // The last printer when it is still installed, the default one
        // otherwise; the list shows the printer that is used.
        const QString selectedPrinter = printerList.contains(settings->lastPrinter())
                                            ? settings->lastPrinter()
                                            : QPrinterInfo::defaultPrinterName();
        printerListComboBox->setCurrentText(selectedPrinter);
        onPrinterSelected(selectedPrinter);
        printPdfDefault = settings->printPdfDefault();
    }
    color->setChecked(settings->printColor());
    setLandscape(settings->printLandscape());
    fitToPageCheckBox->setChecked(settings->printFitToPage());
    if(printPdfDefault)
        exportPdfButton->setFocus();
    // signals
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::close);
    connect(printButton, &QPushButton::clicked, this, &PrintDialog::print);
    connect(exportPdfButton, &QPushButton::clicked, this, &PrintDialog::exportPdf);
    connect(printerListComboBox, &QComboBox::currentTextChanged, this, &PrintDialog::onPrinterSelected);
    connect(landscape, &QRadioButton::toggled, this, &PrintDialog::setLandscape);
    connect(fitToPageCheckBox, &QCheckBox::toggled, this, &PrintDialog::updatePreview);
    connect(color, &QRadioButton::toggled, this, &PrintDialog::updatePreview);
}

void PrintDialog::saveSettings() {
    settings->setPrintLandscape(landscape->isChecked());
    settings->setPrintColor(color->isChecked());
    settings->setPrintFitToPage(fitToPageCheckBox->isChecked());
    settings->setPrintPdfDefault(printPdfDefault);
    if(!printerListComboBox->currentText().isEmpty())
        settings->setLastPrinter(printerListComboBox->currentText());
}

PrintDialog::~PrintDialog() {
    saveSettings();
}

void PrintDialog::setupUi()
{
    setWindowTitle(tr("Print image"));
    resize(548, 231);
    setModal(true);

    QHBoxLayout *mainHorizontalLayout = new QHBoxLayout(this);
    mainHorizontalLayout->setSpacing(9);
    mainHorizontalLayout->setContentsMargins(9, 9, 9, 9);
    mainHorizontalLayout->setSizeConstraint(QLayout::SetFixedSize);

    // Left Column (Preview)
    QVBoxLayout *leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(3);
    leftColumn->setContentsMargins(4, 4, 4, 0);

    previewLabel = new QLabel(this);
    previewLabel->setFixedSize(kPrintPreviewExtent, kPrintPreviewExtent);
    previewLabel->setContextMenuPolicy(Qt::NoContextMenu);
    previewLabel->setAlignment(Qt::AlignCenter);
    leftColumn->addWidget(previewLabel);

    leftColumn->addStretch(1);

    QHBoxLayout *previewTextLayout = new QHBoxLayout();
    previewTextLayout->setContentsMargins(9, 5, 9, 5);

    QFrame *line1 = new QFrame(this);
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    previewTextLayout->addWidget(line1);

    QLabel *previewTitle = new QLabel(tr("Preview"), this);
    previewTitle->setAlignment(Qt::AlignCenter);
    previewTextLayout->addWidget(previewTitle);

    QFrame *line2 = new QFrame(this);
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    previewTextLayout->addWidget(line2);

    leftColumn->addLayout(previewTextLayout);
    mainHorizontalLayout->addLayout(leftColumn);

    // Right Column (Controls)
    QVBoxLayout *rightColumn = new QVBoxLayout();
    rightColumn->setContentsMargins(0, 0, 0, 0);

    // Printer List Group
    QHBoxLayout *printerListLayout = new QHBoxLayout();
    printerListLayout->setSpacing(10);
    printerListLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *printerLabel = new QLabel(tr("Printer:"), this);
    printerListLayout->addWidget(printerLabel);

    printerListPlaceholder = new QLabel(tr("<No printers found>"), this);
    printerListLayout->addWidget(printerListPlaceholder);

    printerListComboBox = new QComboBox(this);
    printerListLayout->addWidget(printerListComboBox);

    printerListLayout->addStretch(1);
    rightColumn->addLayout(printerListLayout);

    // Orientation and Color settings row
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    optionsLayout->setContentsMargins(0, 0, 0, 0);

    // Orientation Group
    QVBoxLayout *orientationLayout = new QVBoxLayout();
    orientationLayout->setSpacing(0);
    orientationLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *orientationTitle = new QLabel(tr("Page orientation:"), this);
    orientationTitle->setMinimumWidth(130);
    orientationLayout->addWidget(orientationTitle);
    orientationLayout->addSpacing(6);

    portrait = new QRadioButton(tr("Portrait"), this);
    portrait->setChecked(true);
    orientationLayout->addWidget(portrait);

    landscape = new QRadioButton(tr("Landscape"), this);
    orientationLayout->addWidget(landscape);
    optionsLayout->addLayout(orientationLayout);

    // Vertical Separator
    QFrame *line3 = new QFrame(this);
    line3->setFrameShape(QFrame::VLine);
    line3->setFrameShadow(QFrame::Sunken);
    optionsLayout->addWidget(line3);

    // Color Mode Group
    QVBoxLayout *colorModeLayout = new QVBoxLayout();
    colorModeLayout->setSpacing(0);
    colorModeLayout->setContentsMargins(6, 0, 0, 0);

    QLabel *colorModeTitle = new QLabel(tr("Color mode:"), this);
    colorModeTitle->setMinimumWidth(130);
    colorModeLayout->addWidget(colorModeTitle);
    colorModeLayout->addSpacing(6);

    grayscale = new QRadioButton(tr("Grayscale"), this);
    grayscale->setChecked(true);
    colorModeLayout->addWidget(grayscale);

    color = new QRadioButton(tr("Color"), this);
    colorModeLayout->addWidget(color);

    colorModeLayout->addStretch(1);
    optionsLayout->addLayout(colorModeLayout);
    rightColumn->addLayout(optionsLayout);

    // Fit to page checkbox
    fitToPageCheckBox = new QCheckBox(tr("Fit to page"), this);
    fitToPageCheckBox->setChecked(true);
    rightColumn->addWidget(fitToPageCheckBox);

    rightColumn->addStretch(1);

    // Button Row
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);

    exportPdfButton = new QPushButton(tr("Export PDF"), this);
    buttonLayout->addWidget(exportPdfButton);

    buttonLayout->addStretch(1);

    printButton = new QPushButton(tr("Print"), this);
    printButton->setDefault(true);
    buttonLayout->addWidget(printButton);

    cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addWidget(cancelButton);

    rightColumn->addLayout(buttonLayout);
    mainHorizontalLayout->addLayout(rightColumn);
}

void PrintDialog::setImage(std::shared_ptr<const QImage> _img) {
    img = _img;
    updatePreview();
}

void PrintDialog::setOutputPath(QString path) {
    printSetup.setPdfOutputPath(path);
}

QString PrintDialog::pdfPathDialog() {
    return QFileDialog::getSaveFileName(this, tr("Choose pdf location"), printSetup.pdfOutputPath(), "*.pdf");
}

PrintOptions PrintDialog::options() const {
    return {.landscape = landscape->isChecked(),
            .color = color->isChecked(),
            .fitToPage = fitToPageCheckBox->isChecked()};
}

void PrintDialog::updatePreview() {
    if(!img)
        return;
    const qreal dpr = devicePixelRatioF();
    const PrintPreviewStyle style{
        .area = previewLabel->size(),
        .devicePixelRatio = dpr,
        .pageFrame = QPalette().window().color().valueF() > kPageFrameWindowValue};
    previewLabel->setPixmap(QPixmap::fromImage(printSetup.renderPreview(*img, options(), style)));
}

void PrintDialog::setLandscape(bool mode) {
    landscape->blockSignals(true);
    landscape->setChecked(mode);
    landscape->blockSignals(false);
    printSetup.setLandscape(mode);
    updatePreview();
}

void PrintDialog::onPrinterSelected(QString name) {
    printSetup.selectPrinter(name);
    updatePreview();
}

void PrintDialog::showPrintFailure(const QString &message) {
    qWarning() << "Print dialog:" << message;
    QMessageBox::warning(this, windowTitle(), message);
}

void PrintDialog::print() {
    if(!img || !printSetup.hasPrinter()) {
        close();
        return;
    }
    if(!printSetup.print(*img, options())) {
        showPrintFailure(tr("Could not print the image."));
        return;
    }
    printPdfDefault = false;
    close();
}

void PrintDialog::exportPdf() {
    if(!img) {
        close();
        return;
    }
    auto path = pdfPathDialog();
    if(path.isEmpty())
        return;
    if(!printSetup.exportPdf(path, *img, options())) {
        showPrintFailure(tr("Could not export the PDF."));
        return;
    }
    printPdfDefault = true;
    close();
}
