#include "printdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QFrame>

PrintDialog::PrintDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    previewLabel->setContentsMargins(0,0,0,0);
    pdfPrinter.setOutputFormat(QPrinter::PdfFormat);
    pdfPrinter.setPageSize(QPageSize(QPageSize::A4));
    pdfPrinter.setOutputFileName(" ");
    QStringList printerList = QPrinterInfo::availablePrinterNames();
    if(printerList.isEmpty()) {
        printerListComboBox->hide();
        printButton->setEnabled(false);
        exportPdfButton->setFocus();
    } else {
        printerListPlaceholder->hide();
        printerListComboBox->addItems(printerList);
        printerListComboBox->setCurrentText(QPrinterInfo::defaultPrinterName());
        if(printerList.contains(settings->lastPrinter()))
            onPrinterSelected(settings->lastPrinter());
        else
            onPrinterSelected(QPrinterInfo::defaultPrinterName());
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
    if(printer)
        delete printer;
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
    previewLabel->setFixedSize(160, 160);
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
    if(path.isEmpty())
        path = " ";
    pdfPrinter.setOutputFileName(path);
}

QString PrintDialog::pdfPathDialog() {
    return QFileDialog::getSaveFileName(this, tr("Choose pdf location"), pdfPrinter.outputFileName(), "*.pdf");
}

void PrintDialog::updatePreview() {
    if(!img)
        return;
    QPrinter *targetPrinter = printer;
    if(!targetPrinter)
        targetPrinter = &pdfPrinter;
    auto imgRect = getImagePrintRect(targetPrinter);
    QRectF fullRect = targetPrinter->pageLayout().fullRectPixels(targetPrinter->resolution());
    // margins
    QMarginsF margins(targetPrinter->pageLayout().marginsPixels(targetPrinter->resolution()));
    // scaled page with margins
    QRect fullRectScaled( QRectF(QPointF(0,0), fullRect.size().scaled(previewLabel->size(), Qt::KeepAspectRatio)).toRect() );
    qreal scale = fullRectScaled.width() / fullRect.width();
    // scaled image rect with margins (not accurate, but good enough for a preview)
    QRect imgRectScaled(QRectF((imgRect.left() + margins.left()) * scale, (imgRect.top() + margins.top()) * scale,
                               imgRect.width() * scale, imgRect.height() * scale).toRect());
    QPixmap pagePixmap(fullRectScaled.size() * qApp->devicePixelRatio());
    pagePixmap.setDevicePixelRatio(qApp->devicePixelRatio());
    auto scaledImg = img->scaled(imgRectScaled.size() * qApp->devicePixelRatio(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if(grayscale->isChecked())
        scaledImg = scaledImg.convertToFormat(QImage::Format_Grayscale8);
    scaledImg.setDevicePixelRatio(qApp->devicePixelRatio());
    QPainter p(&pagePixmap);
    p.fillRect(pagePixmap.rect(), QColor(255,255,255));
    p.drawImage(imgRectScaled.left(), imgRectScaled.top(), scaledImg);
    // page border for white window bg
    QPalette palette;
    QColor sys_window = palette.window().color();
    if(sys_window.valueF() > 0.45f) {
        p.setOpacity(0.25f);
        p.setPen(Qt::black);
        p.drawRect(QRectF(QPointF(0.5f, 0.5f), QSizeF(pagePixmap.size() / qApp->devicePixelRatio() - QSizeF(1.0f, 1.0f))));
    }
    previewLabel->setPixmap(pagePixmap);
}

QRectF PrintDialog::getImagePrintRect(QPrinter *pr) {
    QRectF imgRect;
    if(!pr || !img)
        return QRect();
    QRectF pageRect = QRectF(QPoint(0,0), pr->pageRect(QPrinter::DevicePixel).size());
    imgRect = img->rect();
    // downscale / upscale
    if(fitToPageCheckBox->isChecked() || imgRect.width() > pageRect.width() || imgRect.height() > pageRect.height())
        imgRect.setSize(imgRect.size().scaled(pageRect.size(), Qt::KeepAspectRatio));
    // align top center
    imgRect.moveCenter(pageRect.center());
    imgRect.moveTop(pageRect.top());
    return imgRect;
}

void PrintDialog::setLandscape(bool mode) {
    landscape->blockSignals(true);
    landscape->setChecked(mode);
    landscape->blockSignals(false);
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    if(mode)
        orientation = QPageLayout::Landscape;
    if(printer)
        printer->setPageOrientation(orientation);
    pdfPrinter.setPageOrientation(orientation);
    updatePreview();
}

void PrintDialog::onPrinterSelected(QString name) {
    if(printer)
        delete printer;
    printer = new QPrinter(QPrinterInfo::printerInfo(name));
    updatePreview();
}

void PrintDialog::print() {
    if(!img || !printer) {
        close();
        return;
    }
    if(color->isChecked())
        printer->setColorMode(QPrinter::Color);
    else
        printer->setColorMode(QPrinter::GrayScale);
    QPainter p(printer);
    p.drawImage(getImagePrintRect(printer), *img);
    printPdfDefault = false;
    close();
}

void PrintDialog::exportPdf() {
    if(!img || pdfPrinter.outputFileName().isEmpty()) {
        close();
        return;
    }
    auto path = pdfPathDialog();
    if(path.isEmpty())
        return;
    pdfPrinter.setOutputFileName(path);
    if(color->isChecked())
        pdfPrinter.setColorMode(QPrinter::Color);
    else
        pdfPrinter.setColorMode(QPrinter::GrayScale);
    QPainter p(&pdfPrinter);
    p.drawImage(getImagePrintRect(&pdfPrinter), *img);
    printPdfDefault = true;
    close();
}
