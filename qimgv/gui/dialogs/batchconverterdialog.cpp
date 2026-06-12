#include "batchconverterdialog.h"
#include <QCoreApplication>
#include <QPainter>
#include <QPainterPath>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QMessageBox>
#include <QScreen>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSpacerItem>
#include <cmath>

#ifdef USE_UPSCAYL
#include "realesrgan.h"
#endif

// ==================== BatchItemWidget ====================

BatchItemWidget::BatchItemWidget(const QString &filePath, QWidget *parent)
    : QWidget(parent), path(filePath) {
    QFileInfo fi(filePath);
    size = fi.size();

    auto colors = settings->colorScheme();

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(10);

    checkBox = new QCheckBox(this);
    checkBox->setChecked(true);
    mainLayout->addWidget(checkBox);

    thumbLabel = new QLabel(this);
    thumbLabel->setFixedSize(48, 48);
    thumbLabel->setAlignment(Qt::AlignCenter);
    thumbLabel->setStyleSheet(
        QString("border: 1px solid %1; background-color: %2; border-radius: 4px;")
            .arg(colors.widget_border.name())
            .arg(colors.widget.name()));
    mainLayout->addWidget(thumbLabel);

    QVBoxLayout *leftInfo = new QVBoxLayout();
    leftInfo->setSpacing(2);

    nameLabel = new QLabel(fi.fileName(), this);
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    nameLabel->setStyleSheet(
        QString("font-weight: bold; color: %1; font-size: 12px;")
            .arg(colors.text_hc.name()));
    leftInfo->addWidget(nameLabel);

    srcInfoLabel = new QLabel(this);
    srcInfoLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    srcInfoLabel->setStyleSheet(
        QString("color: %1; font-size: 10px;").arg(colors.text_lc.name()));
    leftInfo->addWidget(srcInfoLabel);

    mainLayout->addLayout(leftInfo, 1);

    QVBoxLayout *rightInfo = new QVBoxLayout();
    rightInfo->setSpacing(2);
    rightInfo->setAlignment(Qt::AlignRight);

    statusLabel = new QLabel(tr("Pending"), this);
    statusLabel->setMinimumWidth(80);
    statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    statusLabel->setAlignment(Qt::AlignRight);
    statusLabel->setStyleSheet("font-weight: bold; color: #ffaa00; font-size: 12px;");
    rightInfo->addWidget(statusLabel);

    destInfoLabel = new QLabel(this);
    destInfoLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    destInfoLabel->setAlignment(Qt::AlignRight);
    destInfoLabel->setStyleSheet(
        QString("color: %1; font-size: 10px;").arg(colors.text_lc.name()));
    rightInfo->addWidget(destInfoLabel);

    mainLayout->addLayout(rightInfo, 0);

    QString sizeStr;
    if (size > 1024 * 1024)
        sizeStr = QString::number(size / (1024.0 * 1024.0), 'f', 1) + " MB";
    else
        sizeStr = QString::number(size / 1024.0, 'f', 1) + " KB";

    QImageReader reader(filePath);
    imgSize = reader.size();
    QString origFormat = reader.format().toUpper();
    srcInfoLabel->setText(QString("%1 \xe2\x80\xa2 %2x%3 \xe2\x80\xa2 %4")
                          .arg(origFormat)
                          .arg(imgSize.width())
                          .arg(imgSize.height())
                          .arg(sizeStr));

    connect(checkBox, &QCheckBox::toggled, this, &BatchItemWidget::checkedStateChanged);
}

void BatchItemWidget::setThumbnail(std::shared_ptr<Thumbnail> thumb) {
    if (thumb && thumb->pixmap() && !thumb->pixmap()->isNull()) {
        QPixmap scaledThumb = thumb->pixmap()->scaled(
            thumbLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Clip the thumbnail with a rounded rectangle (4px)
        QPixmap roundedThumb(scaledThumb.size());
        roundedThumb.fill(Qt::transparent);
        QPainter painter(&roundedThumb);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath path;
        path.addRoundedRect(QRectF(scaledThumb.rect()), 4.0, 4.0);
        painter.setClipPath(path);
        painter.drawPixmap(0, 0, scaledThumb);
        painter.end();

        thumbLabel->setPixmap(roundedThumb);
    }
}

void BatchItemWidget::setStatus(const QString &statusText, const QString &details, bool success) {
    statusLabel->setText(statusText);
    if (!success) {
        statusLabel->setStyleSheet("font-weight: bold; color: #ff3333; font-size: 12px;");
    } else if (statusText == tr("Processing...")) {
        statusLabel->setStyleSheet("font-weight: bold; color: #33aaff; font-size: 12px;");
    } else if (statusText == tr("Done")) {
        statusLabel->setStyleSheet("font-weight: bold; color: #33cc33; font-size: 12px;");
    } else {
        statusLabel->setStyleSheet("font-weight: bold; color: #ffaa00; font-size: 12px;");
    }
    destInfoLabel->setText(details);
}

// ==================== BatchConverterDialog UI Setup ====================

void BatchConverterDialog::setupUi() {
    resize(1048, 972);
    setWindowTitle(tr("Batch Converter"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);

    QHBoxLayout *splitLayout = new QHBoxLayout();
    splitLayout->setSpacing(6);
    mainLayout->addLayout(splitLayout, 1);

    setupLeftPanel(splitLayout);
    setupRightPanel(splitLayout);

    progressBar = new QProgressBar(this);
    progressBar->setValue(0);
    mainLayout->addWidget(progressBar);

    setupBottomPanel(mainLayout);
}

void BatchConverterDialog::setupLeftPanel(QBoxLayout *mainLayout) {
    QVBoxLayout *leftLayout = new QVBoxLayout();

    QHBoxLayout *headerLayout = new QHBoxLayout();
    selectAllBtn = new QPushButton(tr("Select all"), this);
    deselectAllBtn = new QPushButton(tr("Deselect all"), this);
    headerLayout->addWidget(selectAllBtn);
    headerLayout->addWidget(deselectAllBtn);
    headerLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    leftLayout->addLayout(headerLayout);

    selectedCountLabel = new QLabel(tr("0 files selected (0.0 MB)"), this);
    QFont f = selectedCountLabel->font();
    f.setBold(true);
    selectedCountLabel->setFont(f);
    leftLayout->addWidget(selectedCountLabel);

    fileListWidget = new QListWidget(this);
    fileListWidget->setMinimumSize(400, 0);
    fileListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    leftLayout->addWidget(fileListWidget);

    mainLayout->addLayout(leftLayout, 1);
}

void BatchConverterDialog::setupRightPanel(QBoxLayout *mainLayout) {
    scrollArea = new QScrollArea(this);
    scrollArea->setMinimumSize(600, 0);
    scrollArea->setMaximumSize(600, 16777215);
    scrollArea->setWidgetResizable(true);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);

    setupFormatSection(scrollLayout);
    setupResizeSection(scrollLayout);
    scrollLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));
    setupColorSection(scrollLayout);
    scrollLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));
    setupRenameSection(scrollLayout);

    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea, 0);
}

void BatchConverterDialog::setupFormatSection(QVBoxLayout *scrollLayout) {
    QHBoxLayout *fmtLayout = new QHBoxLayout();
    QLabel *fmtLabel = new QLabel(tr("Save as type:"), this);
    fmtLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formatComboBox = new QComboBox(this);
    fmtLayout->addWidget(fmtLabel);
    fmtLayout->addWidget(formatComboBox);
    scrollLayout->addLayout(fmtLayout);

    QHBoxLayout *qLayout = new QHBoxLayout();
    qualitySlider = new QSlider(Qt::Horizontal, this);
    qualitySlider->setRange(1, 100);
    qualitySlider->setValue(90);
    qualitySpinBox = new QSpinBox(this);
    qualitySpinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    qualitySpinBox->setAlignment(Qt::AlignCenter);
    qualitySpinBox->setRange(1, 100);
    qualitySpinBox->setValue(90);
    qLayout->addWidget(qualitySlider);
    qLayout->addWidget(qualitySpinBox);
    scrollLayout->addLayout(qLayout);
}

void BatchConverterDialog::setupResizeSection(QVBoxLayout *scrollLayout) {
    resizeContainer = new QWidget(this);
    QVBoxLayout *rcLayout = new QVBoxLayout(resizeContainer);
    rcLayout->setContentsMargins(0, 0, 0, 0);

    resizeEnableCheckBox = new QCheckBox(tr("Resize"), this);
    rcLayout->addWidget(resizeEnableCheckBox);

    QHBoxLayout *splitLayout = new QHBoxLayout();
    splitLayout->setContentsMargins(0, 0, 0, 0);

    // Left Column
    QVBoxLayout *lCol = new QVBoxLayout();
    byPercentage = new QRadioButton(tr("By Percent:"), this);
    lCol->addWidget(byPercentage);

    QHBoxLayout *percLayout = new QHBoxLayout();
    percLayout->setContentsMargins(20, 0, 0, 0);
    QLabel *lPerc = new QLabel(tr("Percent:"), this);
    lPerc->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    percent = new QDoubleSpinBox(this);
    percent->setMinimumSize(0, 30);
    percent->setAlignment(Qt::AlignCenter);
    percent->setButtonSymbols(QAbstractSpinBox::NoButtons);
    percent->setRange(1.0, 1600.0);
    percent->setValue(100.0);
    percent->setDecimals(1);
    percent->setEnabled(false);
    percLayout->addWidget(lPerc);
    percLayout->addWidget(percent);
    lCol->addLayout(percLayout);

    byAbsoluteSize = new QRadioButton(tr("By Absolute Size:"), this);
    byAbsoluteSize->setChecked(true);
    lCol->addWidget(byAbsoluteSize);

    QVBoxLayout *absLayout = new QVBoxLayout();
    absLayout->setContentsMargins(20, 0, 0, 0);
    QHBoxLayout *wLayout = new QHBoxLayout();
    QLabel *lWidth = new QLabel(tr("Width:"), this);
    lWidth->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    width = new QSpinBox(this);
    width->setMinimumSize(0, 30);
    width->setAlignment(Qt::AlignCenter);
    width->setButtonSymbols(QAbstractSpinBox::NoButtons);
    width->setRange(1, 65535);
    width->setEnabled(false);
    wLayout->addWidget(lWidth);
    wLayout->addWidget(width);
    absLayout->addLayout(wLayout);

    QHBoxLayout *hLayout = new QHBoxLayout();
    QLabel *lHeight = new QLabel(tr("Height:"), this);
    lHeight->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    height = new QSpinBox(this);
    height->setMinimumSize(0, 30);
    height->setAlignment(Qt::AlignCenter);
    height->setButtonSymbols(QAbstractSpinBox::NoButtons);
    height->setRange(1, 65535);
    height->setEnabled(false);
    hLayout->addWidget(lHeight);
    hLayout->addWidget(height);
    absLayout->addLayout(hLayout);
    lCol->addLayout(absLayout);

    QHBoxLayout *chkLayout = new QHBoxLayout();
    keepAspectRatio = new QCheckBox(tr("Keep aspect ratio"), this);
    keepAspectRatio->setChecked(true);
    keepAspectRatio->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    useUpscaylCheckBox = new QCheckBox(tr("Use Upscayl"), this);
    useUpscaylCheckBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    chkLayout->addWidget(keepAspectRatio);
    chkLayout->addWidget(useUpscaylCheckBox);
    lCol->addLayout(chkLayout);

    QHBoxLayout *cbLayout = new QHBoxLayout();
    QVBoxLayout *fLayout = new QVBoxLayout();
    fLayout->addWidget(new QLabel(tr("Filter:"), this));
    filterComboBox = new QComboBox(this);
    filterComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    fLayout->addWidget(filterComboBox);
    cbLayout->addLayout(fLayout);

    QVBoxLayout *mLayout = new QVBoxLayout();
    mLayout->addWidget(new QLabel(tr("Model:"), this));
    upscaylModelComboBox = new QComboBox(this);
    upscaylModelComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    mLayout->addWidget(upscaylModelComboBox);
    cbLayout->addLayout(mLayout);
    lCol->addLayout(cbLayout);

    splitLayout->addLayout(lCol);

    // Right Column
    QVBoxLayout *rCol = new QVBoxLayout();
    rCol->setSpacing(6);
    QLabel *lComSizes = new QLabel(tr("Common sizes:"), this);
    lComSizes->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rCol->addWidget(lComSizes);
    
    resComboBox = new QComboBox(this);
    resComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    rCol->addWidget(resComboBox);
    
    fitDesktopButton = new QPushButton(tr("Fit to desktop"), this);
    fitDesktopButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    fillDesktopButton = new QPushButton(tr("Fill desktop (expanding)"), this);
    fillDesktopButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    rCol->addWidget(fitDesktopButton);
    rCol->addWidget(fillDesktopButton);
    rCol->addWidget(resetButton);

    splitLayout->addLayout(rCol);
    rcLayout->addLayout(splitLayout);
    scrollLayout->addWidget(resizeContainer);
}

void BatchConverterDialog::setupColorSection(QVBoxLayout *scrollLayout) {
    colorContainer = new QWidget(this);
    QVBoxLayout *ccLayout = new QVBoxLayout(colorContainer);
    ccLayout->setContentsMargins(0, 0, 0, 0);

    colorEnableCheckBox = new QCheckBox(tr("Color adjustments"), this);
    ccLayout->addWidget(colorEnableCheckBox);

    QVBoxLayout *vColorLayout = new QVBoxLayout();

    auto addColorRow = [&](const QString &text, int minV, int maxV, int defV, QSlider*& s, QDoubleSpinBox*& sb, double sbMin, double sbMax, double sbStep) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *l = new QLabel(text, this);
        l->setMinimumWidth(80);
        s = new QSlider(Qt::Horizontal, this);
        s->setRange(minV, maxV);
        s->setValue(defV);
        sb = new QDoubleSpinBox(this);
        sb->setFixedSize(80, 24); // approximated height
        sb->setAlignment(Qt::AlignCenter);
        sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sb->setRange(sbMin, sbMax);
        sb->setSingleStep(sbStep);
        row->addWidget(l);
        row->addWidget(s);
        row->addWidget(sb);
        vColorLayout->addLayout(row);
    };

    addColorRow(tr("Exposure:"), -200, 200, 0, exposureSlider, exposureSpinBox, -2.0, 2.0, 0.1);
    addColorRow(tr("Contrast:"), 0, 300, 100, contrastSlider, contrastSpinBox, 0.0, 3.0, 0.1);
    contrastSpinBox->setValue(1.0);
    addColorRow(tr("Brightness:"), -100, 100, 0, brightnessSlider, brightnessSpinBox, -1.0, 1.0, 0.1);
    addColorRow(tr("Saturation:"), 0, 300, 100, saturationSlider, saturationSpinBox, 0.0, 3.0, 0.1);
    saturationSpinBox->setValue(1.0);
    addColorRow(tr("Hue:"), -180, 180, 0, hueSlider, hueSpinBox, -180.0, 180.0, 1.0);
    addColorRow(tr("Temperature:"), -100, 100, 0, tempSlider, tempSpinBox, -1.0, 1.0, 0.05);
    addColorRow(tr("Tint:"), -100, 100, 0, tintSlider, tintSpinBox, -1.0, 1.0, 0.05);

    ccLayout->addLayout(vColorLayout);
    // Note: The UI file has an empty verticalLayoutColor inside colorContainer, 
    // the code creates resetColorButton dynamically and adds it to verticalLayoutColor.
    // So we should expose this layout or recreate the button later.
    // I will expose vColorLayout by re-parenting things properly later or just letting setupUi finish it.

    scrollLayout->addWidget(colorContainer);
}

void BatchConverterDialog::setupRenameSection(QVBoxLayout *scrollLayout) {
    outputContainer = new QWidget(this);
    QVBoxLayout *ocLayout = new QVBoxLayout(outputContainer);
    ocLayout->setContentsMargins(0, 0, 0, 0);

    ocLayout->addWidget(new QLabel(tr("Output folder:"), this));
    
    QHBoxLayout *dirLayout = new QHBoxLayout();
    outDirEdit = new QLineEdit(this);
    outDirBrowseBtn = new QPushButton(tr("..."), this);
    dirLayout->addWidget(outDirEdit);
    dirLayout->addWidget(outDirBrowseBtn);
    ocLayout->addLayout(dirLayout);

    subfolderCheckBox = new QCheckBox(tr("Create subfolder for batch"), this);
    ocLayout->addWidget(subfolderCheckBox);

    ocLayout->addWidget(new QLabel(tr("Filename pattern:"), this));
    patternEdit = new QLineEdit(tr("{name}_converted"), this);
    ocLayout->addWidget(patternEdit);

    QLabel *helpL = new QLabel(tr("Available: {name}, {ext}, {date}, {index}"), this);
    QFont f = helpL->font(); f.setItalic(true); helpL->setFont(f);
    ocLayout->addWidget(helpL);

    overwriteCheckBox = new QCheckBox(tr("Overwrite existing files"), this);
    ocLayout->addWidget(overwriteCheckBox);

    scrollLayout->addWidget(outputContainer);
}

void BatchConverterDialog::setupBottomPanel(QVBoxLayout *mainLayout) {
    QHBoxLayout *bLayout = new QHBoxLayout();
    statusLabel = new QLabel(tr("Ready to convert."), this);
    bLayout->addWidget(statusLabel);
    bLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    convertButton = new QPushButton(tr("Convert"), this);
    convertButton->setDefault(true);
    cancelButton = new QPushButton(tr("Cancel"), this);

    bLayout->addWidget(convertButton);
    bLayout->addWidget(cancelButton);
    mainLayout->addLayout(bLayout);
}

// ==================== BatchConverterDialog ====================

BatchConverterDialog::BatchConverterDialog(const QList<QString> &filePaths, QWidget *parent)
    : QDialog(parent), inputPaths(filePaths) {
    setupUi();
    setWindowModality(Qt::ApplicationModal);

    // Add Reset Color Adjustments button
    QPushButton *resetColorButton = new QPushButton(tr("Reset Color Adjustments"), this);
    colorContainer->layout()->addWidget(resetColorButton);
    connect(resetColorButton, &QPushButton::clicked, this, [this]() {
        exposureSlider->setValue(0);
        contrastSlider->setValue(100);
        brightnessSlider->setValue(0);
        saturationSlider->setValue(100);
        hueSlider->setValue(0);
        tempSlider->setValue(0);
        tintSlider->setValue(0);
    });

    exposureSlider->installEventFilter(this);
    contrastSlider->installEventFilter(this);
    brightnessSlider->installEventFilter(this);
    saturationSlider->installEventFilter(this);
    hueSlider->installEventFilter(this);
    tempSlider->installEventFilter(this);
    tintSlider->installEventFilter(this);

    collectResizeWidgets();
    collectColorWidgets();

    setResizeWidgetsEnabled(resizeEnableCheckBox->isChecked());
    setColorWidgetsEnabled(colorEnableCheckBox->isChecked());

    connect(resizeEnableCheckBox, &QCheckBox::toggled, this, &BatchConverterDialog::onResizeEnabledChanged);
    connect(colorEnableCheckBox, &QCheckBox::toggled, this, &BatchConverterDialog::onColorEnabledChanged);

    // ----- Configure spinboxes with proper units -----
    exposureSpinBox->setRange(-2.0, 2.0);
    exposureSpinBox->setSingleStep(0.1);
    exposureSpinBox->setSuffix("");
    exposureSpinBox->setDecimals(2);
    exposureSlider->setRange(-200, 200);
    exposureSlider->setValue(0);

    contrastSpinBox->setRange(0.0, 300.0);
    contrastSpinBox->setValue(100.0);
    contrastSpinBox->setSingleStep(1.0);
    contrastSpinBox->setSuffix("%");
    contrastSpinBox->setDecimals(0);
    contrastSlider->setRange(0, 300);
    contrastSlider->setValue(100);

    brightnessSpinBox->setRange(-100.0, 100.0);
    brightnessSpinBox->setValue(0.0);
    brightnessSpinBox->setSingleStep(1.0);
    brightnessSpinBox->setSuffix("%");
    brightnessSlider->setRange(-100, 100);
    brightnessSlider->setValue(0);

    saturationSpinBox->setRange(0.0, 200.0);
    saturationSpinBox->setValue(100.0);
    saturationSpinBox->setSingleStep(1.0);
    saturationSpinBox->setSuffix("%");
    saturationSlider->setRange(0, 200);
    saturationSlider->setValue(100);

    hueSpinBox->setRange(-180.0, 180.0);
    hueSpinBox->setValue(0.0);
    hueSpinBox->setSingleStep(1.0);
    hueSpinBox->setSuffix("\xc2\xb0");
    hueSpinBox->setDecimals(0);
    hueSlider->setRange(-180, 180);
    hueSlider->setValue(0);

    tempSpinBox->setRange(-50.0, 50.0);
    tempSpinBox->setValue(0.0);
    tempSpinBox->setSingleStep(1.0);
    tempSpinBox->setDecimals(0);
    tempSpinBox->setSuffix("");
    tempSlider->setRange(-50, 50);
    tempSlider->setValue(0);

    tintSpinBox->setRange(-50.0, 50.0);
    tintSpinBox->setValue(0.0);
    tintSpinBox->setSingleStep(1.0);
    tintSpinBox->setDecimals(0);
    tintSpinBox->setSuffix("");
    tintSlider->setRange(-50, 50);
    tintSlider->setValue(0);

    auto colors = settings->colorScheme();
    QString dialogStyle =
        QString("QDialog { background-color: %1; color: %2; }"
                "QScrollArea, QScrollArea > QWidget > QWidget { background-color: %1; border: none; }"
                "QGroupBox { background-color: %1; color: %3; border: 1px solid %4; border-radius: 4px; margin-top: 10px; padding-top: 12px; }"
                "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 0 3px; color: %3; }"
                "QLabel { color: %3; }"
                "QLabel:disabled { color: %2; }"
                "QCheckBox, QRadioButton { color: %3; }"
                "QCheckBox:disabled, QRadioButton:disabled { color: %2; }"
                "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background-color: %5; color: %3; border: 1px solid %4; border-radius: 3px; padding: 3px; }"
                "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled { background-color: %1; color: %2; border-color: %4; }"
                "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover,"
                "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: %6; }"
                "QListWidget { background-color: %8; border: 1px solid %4; color: %3; }"
                "QScrollBar:vertical { width: 13px; background-color: transparent; }"
                "QScrollBar::handle:vertical { background-color: %9; min-height: 30px; border-radius: 2px; }"
                "QScrollBar::handle:vertical:hover { background-color: %10; }"
                "QScrollBar::sub-page, QScrollBar::add-page { background: none; }"
                "QProgressBar { border: 1px solid %4; border-radius: 3px; text-align: center; color: %7; background-color: %8; }"
                "QProgressBar::chunk { background-color: %6; }"
                "QSlider { height: 18px; background: transparent; }"
                "QSlider::groove:horizontal { height: 4px; background: %5; border-radius: 2px; }"
                "QSlider::sub-page:horizontal { background: %6; border-radius: 2px; }"
                "QSlider::sub-page:horizontal:disabled { background: %4; }"
                "QSlider::handle:horizontal { background: %3; width: 12px; height: 12px; margin: -4px 0px; border-radius: 6px; }"
                "QSlider::handle:horizontal:disabled { background: %2; }")
        .arg(colors.widget.name(), colors.text.name(), colors.text_hc.name(),
             colors.widget_border.name(), colors.button.name(), colors.accent.name(),
             colors.text_hc2.name(), colors.folderview.name(), colors.scrollbar.name(),
             colors.scrollbar_hover.name());
    setStyleSheet(dialogStyle);

    threadPool.setMaxThreadCount(QThread::idealThreadCount());

    formatComboBox->addItem("JPEG (*.jpg *.jpeg *.jpe *.jfif)", "jpg");
    formatComboBox->addItem("PNG (*.png)", "png");
    formatComboBox->addItem("WebP (*.webp)", "webp");
    formatComboBox->addItem("JPEG-XL (*.jxl)", "jxl");
    formatComboBox->addItem("AVIF (*.avif *.avifs)", "avif");
    formatComboBox->addItem("QOI (*.qoi)", "qoi");
    formatComboBox->addItem("BMP (*.bmp)", "bmp");
    formatComboBox->addItem("TIFF (*.tif *.tiff)", "tif");

    thumbnailer = new Thumbnailer();
    connect(thumbnailer, &Thumbnailer::thumbnailReady, this,
            [this](std::shared_ptr<Thumbnail> thumb, QString filePath) {
                for (int i = 0; i < fileListWidget->count(); ++i) {
                    QListWidgetItem *item = fileListWidget->item(i);
                    auto *w = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
                    if (w && w->filePath() == filePath) {
                        w->setThumbnail(thumb);
                        break;
                    }
                }
            });

    fileListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fileListWidget->setResizeMode(QListView::Adjust);
    for (const QString &path : filePaths) {
        QListWidgetItem *item = new QListWidgetItem(fileListWidget);
        BatchItemWidget *widget = new BatchItemWidget(path, this);
        item->setSizeHint(widget->sizeHint());
        fileListWidget->addItem(item);
        fileListWidget->setItemWidget(item, widget);
        connect(widget, &BatchItemWidget::checkedStateChanged, this, &BatchConverterDialog::onCheckedStateChanged);
        thumbnailer->getThumbnailAsync(path, 48, true, false);
    }
    totalFiles = filePaths.size();
    updateSelectedCount();

    if (!filePaths.isEmpty())
        outDirEdit->setText(QFileInfo(filePaths[0]).absolutePath());

    filterComboBox->addItem(tr("Nearest"), QI_FILTER_NEAREST);
    filterComboBox->addItem(tr("Bilinear"), QI_FILTER_BILINEAR);
    filterComboBox->addItem(tr("Smart sharpen"), QI_FILTER_SMART);
    int smartIndex = filterComboBox->findData(QI_FILTER_SMART);
    filterComboBox->setCurrentIndex(smartIndex != -1 ? smartIndex : 1);

    desktopSize = qApp->primaryScreen()->size();
    resComboBox->addItem(tr("Original size"));
    resComboBox->addItem("1366 x 768");
    resComboBox->addItem("1440 x 900");
    resComboBox->addItem("1440 x 1050");
    resComboBox->addItem("1600 x 1200");
    resComboBox->addItem("1920 x 1080");
    resComboBox->addItem("1920 x 1200");
    resComboBox->addItem("2560 x 1080");
    resComboBox->addItem("2560 x 1440");
    resComboBox->addItem("2560 x 1600");
    resComboBox->addItem("3840 x 1600");
    resComboBox->addItem("3840 x 2160");

    if (!filePaths.isEmpty()) {
        QImageReader r(filePaths[0]);
        originalSize = r.size();
    } else {
        originalSize = QSize(2560, 1440);
    }
    targetSize = originalSize;
    width->setValue(originalSize.width());
    height->setValue(originalSize.height());
    resetButton->setText(tr("Reset: %1 x %2").arg(originalSize.width()).arg(originalSize.height()));

    percent->setEnabled(false);

#ifdef USE_UPSCAYL
    QDir modelsDir(QCoreApplication::applicationDirPath() + "/models");
    QStringList filters; filters << "*.param";
    QStringList files = modelsDir.entryList(filters, QDir::Files);
    QStringList modelNames;
    for (const QString &file : files) {
        QFileInfo fi(file);
        QString modelName = fi.baseName();
        if (modelsDir.exists(modelName + ".bin")) modelNames.append(modelName);
    }
    if (modelNames.isEmpty()) modelNames.append("4xLSDIRCompactC3");
    upscaylModelComboBox->addItems(modelNames);

    int modelIdx = upscaylModelComboBox->findText(settings->upscaylModel());
    upscaylModelComboBox->setCurrentIndex(modelIdx != -1 ? modelIdx : 0);
    useUpscaylCheckBox->setChecked(settings->resizeUseUpscayl());
    bool resizeEnabled = resizeEnableCheckBox->isChecked();
    useUpscaylCheckBox->setEnabled(resizeEnabled);
    upscaylModelComboBox->setEnabled(resizeEnabled && useUpscaylCheckBox->isChecked());
#else
    useUpscaylCheckBox->setEnabled(false);
    useUpscaylCheckBox->setToolTip(tr("AI Upscaling is disabled in this build."));
    upscaylModelComboBox->setEnabled(false);
#endif

    connect(qualitySlider, &QSlider::valueChanged, this, &BatchConverterDialog::onQualitySliderChanged);
    connect(qualitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onQualitySpinBoxChanged);

    // ----- Color adjustment connections -----
    // For exposureSpinBox (double -> double) as declared in header
    connect(exposureSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onExposureSliderChanged);
    connect(exposureSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &BatchConverterDialog::onExposureSpinBoxChanged);

    // For others, spinBox gives double, but slots expect int => use lambda
    connect(contrastSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onContrastSliderChanged);
    connect(contrastSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double val) { onContrastSpinBoxChanged(static_cast<int>(val)); });

    connect(brightnessSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onBrightnessSliderChanged);
    connect(brightnessSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double val) { onBrightnessSpinBoxChanged(static_cast<int>(val)); });

    connect(saturationSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onSaturationSliderChanged);
    connect(saturationSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double val) { onSaturationSpinBoxChanged(static_cast<int>(val)); });

    connect(hueSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onHueSliderChanged);
    connect(hueSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double val) { onHueSpinBoxChanged(static_cast<int>(val)); });

    connect(tempSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onTempSliderChanged);
    connect(tempSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double val) { onTempSpinBoxChanged(static_cast<int>(val)); });

    connect(tintSlider, &QSlider::valueChanged, this, &BatchConverterDialog::onTintSliderChanged);
    connect(tintSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double val) { onTintSpinBoxChanged(static_cast<int>(val)); });

    connect(byPercentage, &QRadioButton::toggled, this, &BatchConverterDialog::onResizeRadioToggled);
    connect(byAbsoluteSize, &QRadioButton::toggled, this, &BatchConverterDialog::onResizeRadioToggled);
    connect(percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &BatchConverterDialog::onPercentChanged);
    connect(width, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onWidthChanged);
    connect(height, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onHeightChanged);
    connect(resComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &BatchConverterDialog::onCommonResolutionChanged);
    connect(fitDesktopButton, &QPushButton::clicked, this, &BatchConverterDialog::onFitDesktop);
    connect(fillDesktopButton, &QPushButton::clicked, this, &BatchConverterDialog::onFillDesktop);
    connect(resetButton, &QPushButton::clicked, this, &BatchConverterDialog::onResetSizes);
    connect(useUpscaylCheckBox, &QCheckBox::toggled, this, &BatchConverterDialog::onUseUpscaylToggled);

    connect(selectAllBtn, &QPushButton::clicked, this, &BatchConverterDialog::onSelectAll);
    connect(deselectAllBtn, &QPushButton::clicked, this, &BatchConverterDialog::onDeselectAll);
    connect(outDirBrowseBtn, &QPushButton::clicked, this, &BatchConverterDialog::onBrowseClicked);
    connect(formatComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &BatchConverterDialog::onFormatChanged);
    connect(convertButton, &QPushButton::clicked, this, &BatchConverterDialog::onConvertClicked);
    connect(cancelButton, &QPushButton::clicked, this, &BatchConverterDialog::onCancelClicked);

    onFormatChanged(0);
}

BatchConverterDialog::~BatchConverterDialog() {
    isCancelled = true;
    threadPool.clear();
    threadPool.waitForDone();
    cleanupSharedUpscayl();
    thumbnailer->clearTasks();
    thumbnailer->waitForDone();
    delete thumbnailer;
}

void BatchConverterDialog::onQualitySliderChanged(int value) {
    qualitySpinBox->blockSignals(true);
    qualitySpinBox->setValue(value);
    qualitySpinBox->blockSignals(false);
}

void BatchConverterDialog::onQualitySpinBoxChanged(int value) {
    qualitySlider->blockSignals(true);
    qualitySlider->setValue(value);
    qualitySlider->blockSignals(false);
}

// ----- Color adjustment slots (matching header signatures) -----
void BatchConverterDialog::onExposureSliderChanged(int value) {
    double val = value / 100.0;
    exposureSpinBox->blockSignals(true);
    exposureSpinBox->setValue(val);
    exposureSpinBox->blockSignals(false);
}
void BatchConverterDialog::onExposureSpinBoxChanged(double value) {
    exposureSlider->blockSignals(true);
    exposureSlider->setValue(static_cast<int>(value * 100));
    exposureSlider->blockSignals(false);
}

void BatchConverterDialog::onContrastSliderChanged(int value) {
    contrastSpinBox->blockSignals(true);
    contrastSpinBox->setValue(static_cast<double>(value));
    contrastSpinBox->blockSignals(false);
}
void BatchConverterDialog::onContrastSpinBoxChanged(int value) {
    contrastSlider->blockSignals(true);
    contrastSlider->setValue(value);
    contrastSlider->blockSignals(false);
}

void BatchConverterDialog::onBrightnessSliderChanged(int value) {
    brightnessSpinBox->blockSignals(true);
    brightnessSpinBox->setValue(static_cast<double>(value));
    brightnessSpinBox->blockSignals(false);
}
void BatchConverterDialog::onBrightnessSpinBoxChanged(int value) {
    brightnessSlider->blockSignals(true);
    brightnessSlider->setValue(value);
    brightnessSlider->blockSignals(false);
}

void BatchConverterDialog::onSaturationSliderChanged(int value) {
    saturationSpinBox->blockSignals(true);
    saturationSpinBox->setValue(static_cast<double>(value));
    saturationSpinBox->blockSignals(false);
}
void BatchConverterDialog::onSaturationSpinBoxChanged(int value) {
    saturationSlider->blockSignals(true);
    saturationSlider->setValue(value);
    saturationSlider->blockSignals(false);
}

void BatchConverterDialog::onHueSliderChanged(int value) {
    hueSpinBox->blockSignals(true);
    hueSpinBox->setValue(static_cast<double>(value));
    hueSpinBox->blockSignals(false);
}
void BatchConverterDialog::onHueSpinBoxChanged(int value) {
    hueSlider->blockSignals(true);
    hueSlider->setValue(value);
    hueSlider->blockSignals(false);
}

void BatchConverterDialog::onTempSliderChanged(int value) {
    tempSpinBox->blockSignals(true);
    tempSpinBox->setValue(static_cast<double>(value));
    tempSpinBox->blockSignals(false);
}
void BatchConverterDialog::onTempSpinBoxChanged(int value) {
    tempSlider->blockSignals(true);
    tempSlider->setValue(value);
    tempSlider->blockSignals(false);
}

void BatchConverterDialog::onTintSliderChanged(int value) {
    tintSpinBox->blockSignals(true);
    tintSpinBox->setValue(static_cast<double>(value));
    tintSpinBox->blockSignals(false);
}
void BatchConverterDialog::onTintSpinBoxChanged(int value) {
    tintSlider->blockSignals(true);
    tintSlider->setValue(value);
    tintSlider->blockSignals(false);
}

// ----- Resize slots -----
void BatchConverterDialog::onResizeRadioToggled() {
    bool isPercent = byPercentage->isChecked();
    percent->setEnabled(isPercent);
    width->setEnabled(!isPercent);
    height->setEnabled(!isPercent);
    keepAspectRatio->setEnabled(!isPercent);

    if (isPercent) {
        keepAspectRatio->blockSignals(true);
        keepAspectRatio->setChecked(true);
        keepAspectRatio->blockSignals(false);
        onPercentChanged(percent->value());
    } else {
        onWidthChanged(width->value());
    }
}

void BatchConverterDialog::onPercentChanged(double val) {
    double scale = val / 100.0;
    targetSize.setWidth(originalSize.width() * scale);
    targetSize.setHeight(originalSize.height() * scale);
    updateToTargetValues();
}

void BatchConverterDialog::onWidthChanged(int val) {
    lastEdited = 0;
    float factor = static_cast<float>(val) / originalSize.width();
    targetSize.setWidth(val);
    if (keepAspectRatio->isChecked()) {
        targetSize.setHeight(static_cast<int>(originalSize.height() * factor));
    }
    updateToTargetValues();
}

void BatchConverterDialog::onHeightChanged(int val) {
    lastEdited = 1;
    float factor = static_cast<float>(val) / originalSize.height();
    targetSize.setHeight(val);
    if (keepAspectRatio->isChecked()) {
        targetSize.setWidth(static_cast<int>(originalSize.width() * factor));
    }
    updateToTargetValues();
}

void BatchConverterDialog::updateToTargetValues() {
    width->blockSignals(true);
    height->blockSignals(true);
    width->setValue(targetSize.width());
    height->setValue(targetSize.height());
    width->blockSignals(false);
    height->blockSignals(false);
}

void BatchConverterDialog::onCommonResolutionChanged(int index) {
    QSize res;
    switch (index) {
    case 1: res = QSize(1366, 768); break;
    case 2: res = QSize(1440, 900); break;
    case 3: res = QSize(1440, 1050); break;
    case 4: res = QSize(1600, 1200); break;
    case 5: res = QSize(1920, 1080); break;
    case 6: res = QSize(1920, 1200); break;
    case 7: res = QSize(2560, 1080); break;
    case 8: res = QSize(2560, 1440); break;
    case 9: res = QSize(2560, 1600); break;
    case 10: res = QSize(3840, 1600); break;
    case 11: res = QSize(3840, 2160); break;
    default: res = originalSize; break;
    }
    if (keepAspectRatio->isChecked())
        targetSize = originalSize.scaled(res, Qt::KeepAspectRatio);
    else
        targetSize = originalSize.scaled(res, Qt::IgnoreAspectRatio);
    updateToTargetValues();
}

void BatchConverterDialog::onFitDesktop() {
    targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatio);
    updateToTargetValues();
}

void BatchConverterDialog::onFillDesktop() {
    targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatioByExpanding);
    updateToTargetValues();
}

void BatchConverterDialog::onResetSizes() {
    resComboBox->blockSignals(true);
    resComboBox->setCurrentIndex(0);
    resComboBox->blockSignals(false);
    targetSize = originalSize;
    updateToTargetValues();
}

void BatchConverterDialog::onUseUpscaylToggled(bool checked) {
    upscaylModelComboBox->setEnabled(checked);
}

void BatchConverterDialog::onSelectAll() {
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
        if (widget) widget->setChecked(true);
    }
}

void BatchConverterDialog::onDeselectAll() {
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
        if (widget) widget->setChecked(false);
    }
}

void BatchConverterDialog::onCheckedStateChanged() {
    updateSelectedCount();
}

void BatchConverterDialog::updateSelectedCount() {
    int checkedCount = 0;
    qint64 totalSizeBytes = 0;
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
        if (widget && widget->isChecked()) {
            checkedCount++;
            totalSizeBytes += widget->fileSize();
        }
    }
    double totalSizeMB = totalSizeBytes / (1024.0 * 1024.0);
    selectedCountLabel->setText(tr("%1 files selected (%2 MB)").arg(checkedCount).arg(QString::number(totalSizeMB, 'f', 1)));
}

void BatchConverterDialog::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"), outDirEdit->text());
    if (!dir.isEmpty()) outDirEdit->setText(dir);
}

void BatchConverterDialog::onFormatChanged(int index) {
    QString ext = formatComboBox->itemData(index).toString();
    if (ext == "png") {
        qualitySlider->setEnabled(true);
        qualitySpinBox->setEnabled(true);
        qualitySlider->blockSignals(true);
        qualitySpinBox->blockSignals(true);
        qualitySlider->setRange(0, 9);
        qualitySpinBox->setRange(0, 9);
        qualitySlider->setValue(settings->pngSaveQuality());
        qualitySpinBox->setValue(settings->pngSaveQuality());
        qualitySlider->blockSignals(false);
        qualitySpinBox->blockSignals(false);
        qualitySlider->setToolTip(tr("PNG Compression level (0 - none, 9 - max)"));
        qualitySpinBox->setToolTip(tr("PNG Compression level (0 - none, 9 - max)"));
    } else if (ext == "jpg" || ext == "webp" || ext == "jxl" || ext == "avif") {
        qualitySlider->setEnabled(true);
        qualitySpinBox->setEnabled(true);
        qualitySlider->blockSignals(true);
        qualitySpinBox->blockSignals(true);
        qualitySlider->setRange(1, 100);
        qualitySpinBox->setRange(1, 100);
        int val = 90;
        if (ext == "jpg") val = settings->JPEGSaveQuality();
        else if (ext == "webp" || ext == "jxl" || ext == "avif") val = settings->modernSaveQuality();
        qualitySlider->setValue(val);
        qualitySpinBox->setValue(val);
        qualitySlider->blockSignals(false);
        qualitySpinBox->blockSignals(false);
        qualitySlider->setToolTip(tr("Quality (1 - lowest, 100 - highest)"));
        qualitySpinBox->setToolTip(tr("Quality (1 - lowest, 100 - highest)"));
    } else {
        qualitySlider->setEnabled(false);
        qualitySpinBox->setEnabled(false);
        qualitySlider->setToolTip("");
        qualitySpinBox->setToolTip("");
    }
}

void BatchConverterDialog::updateUiState() {
    scrollArea->setEnabled(!isConverting);
    convertButton->setEnabled(!isConverting);
    selectAllBtn->setEnabled(!isConverting);
    deselectAllBtn->setEnabled(!isConverting);
    cancelButton->setText(isConverting ? tr("Stop") : tr("Cancel"));
}

void BatchConverterDialog::onConvertClicked() {
    if (isConverting) return;

    QString outDir = outDirEdit->text().trimmed();
    if (outDir.isEmpty() || !QDir(outDir).exists()) {
        QMessageBox::warning(this, tr("Invalid Directory"), tr("Please select a valid output directory."));
        return;
    }

    int checkedCount = 0;
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
        if (widget && widget->isChecked()) checkedCount++;
    }

    if (checkedCount == 0) {
        QMessageBox::warning(this, tr("No files"), tr("No files selected in the queue. Please check at least one file."));
        return;
    }

    isConverting = true;
    isCancelled = false;
    processedFiles = 0;
    successCount = 0;
    failedCount = 0;

    progressBar->setMaximum(checkedCount);
    progressBar->setValue(0);
    statusLabel->setText(tr("Processing..."));
    updateUiState();
    startConversion();
}

void BatchConverterDialog::startConversion() {
    QString baseOutDir = outDirEdit->text().trimmed();
    QString finalOutDir = baseOutDir;

    if (subfolderCheckBox->isChecked()) {
        QString subfolderName = "Batch_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QDir baseDir(baseOutDir);
        if (baseDir.mkdir(subfolderName)) finalOutDir = baseOutDir + "/" + subfolderName;
    }

    QString formatExt = formatComboBox->currentData().toString();
    int quality = qualitySlider->value();
    bool doResize = resizeEnableCheckBox->isChecked();
    QSize resizeTarget = targetSize;
    bool keepAspect = keepAspectRatio->isChecked();
    bool useUpscayl = doResize && useUpscaylCheckBox->isChecked();
    QString upscaylModel = upscaylModelComboBox->currentText();

#ifdef USE_UPSCAYL
    settings->setResizeUseUpscayl(useUpscaylCheckBox->isChecked());
    settings->setUpscaylModel(upscaylModel);
    settings->sync();
#endif

    int filter = filterComboBox->currentData().toInt();
    bool doColor = colorEnableCheckBox->isChecked();

    // Convert UI values to coefficients expected by ImageLib::applyColorAdjustments
    float exposure = doColor ? static_cast<float>(exposureSpinBox->value()) : 0.0f;
    float contrast = doColor ? static_cast<float>(contrastSpinBox->value() / 100.0) : 1.0f;
    float brightness = doColor ? static_cast<float>(brightnessSpinBox->value() / 100.0) : 0.0f;
    float saturation = doColor ? static_cast<float>(saturationSpinBox->value() / 100.0) : 1.0f;
    float hue = doColor ? static_cast<float>(hueSpinBox->value()) : 0.0f;
    float temp = doColor ? static_cast<float>(tempSpinBox->value() / 100.0) : 0.0f;
    float tint = doColor ? static_cast<float>(tintSpinBox->value() / 100.0) : 0.0f;

    QString pattern = patternEdit->text();
    bool overwrite = overwriteCheckBox->isChecked();

    if (useUpscayl && doResize) {
        threadPool.setMaxThreadCount(1);
    } else {
        threadPool.setMaxThreadCount(QThread::idealThreadCount());
    }

#ifdef USE_UPSCAYL
    if (useUpscayl && doResize) {
        statusLabel->setText(tr("Loading AI Model..."));
        qApp->processEvents();

        sharedResrgan = new RealESRGAN(-1, false);
        sharedResrgan->scale = 4;
        sharedResrgan->prepadding = 10;
        sharedResrgan->tilesize = sharedResrgan->autoTilesize();

        QString appDir = QCoreApplication::applicationDirPath();
        QString paramQStr = appDir + "/models/" + upscaylModel + ".param";
        QString binQStr = appDir + "/models/" + upscaylModel + ".bin";

        int loadRes = sharedResrgan->load(paramQStr.toStdWString(), binQStr.toStdWString());
        if (loadRes != 0) {
            delete sharedResrgan;
            sharedResrgan = nullptr;
            statusLabel->setText(tr("Failed to load AI model."));
            QMessageBox::warning(this, tr("AI Error"), tr("Failed to load AI upscaling model: %1").arg(upscaylModel));
            return;
        }
    }
#endif

    int activeIndex = 0;
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
        if (!widget || !widget->isChecked()) continue;

        widget->setStatus(tr("Pending"), "", true);
        QString srcPath = widget->filePath();
        QFileInfo srcFi(srcPath);

        QString targetName = pattern;
        targetName.replace("{name}", srcFi.baseName());
        targetName.replace("{ext}", formatExt);
        targetName.replace("{date}", QDate::currentDate().toString("yyyy-MM-dd"));
        targetName.replace("{index}", QString::number(activeIndex + 1));

        if (!targetName.contains(".")) targetName += "." + formatExt;

        QString destPath = finalOutDir + "/" + targetName;

        if (!overwrite && QFileInfo::exists(destPath)) {
            processedFiles++;
            successCount++;
            progressBar->setValue(processedFiles);
            widget->setStatus(tr("Done"), tr("Skipped (Exists)"), true);
            continue;
        }

        widget->setStatus(tr("Processing..."), "", true);
        BatchWorkerTask *task = new BatchWorkerTask(
            this, i, srcPath, destPath, formatExt, quality, doResize, resizeTarget,
            keepAspect, useUpscayl, upscaylModel, filter, exposure, contrast,
            brightness, temp, tint, saturation, hue);
        threadPool.start(task);
        activeIndex++;
    }

    if (processedFiles >= progressBar->maximum()) finalizeConversion();
}

void BatchConverterDialog::onProgressUpdated(int index, QString status, QString details, bool success) {
    QListWidgetItem *item = fileListWidget->item(index);
    BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
    if (widget) widget->setStatus(status, details, success);

    processedFiles++;
    if (success) successCount++;
    else failedCount++;

    progressBar->setValue(processedFiles);
    statusLabel->setText(tr("Processed %1 / %2 files.").arg(processedFiles).arg(progressBar->maximum()));

    if (processedFiles >= progressBar->maximum() || isCancelled) finalizeConversion();
}

void BatchConverterDialog::finalizeConversion() {
    isConverting = false;
    cleanupSharedUpscayl();
    updateUiState();
    statusLabel->setText(tr("Finished. Success: %1, Failed: %2").arg(successCount).arg(failedCount));
    QMessageBox::information(this, tr("Batch Conversion Complete"),
                             tr("Batch process complete.\n\nSuccessfully converted: %1\nFailed: %2\nTotal files: %3")
                             .arg(successCount).arg(failedCount).arg(processedFiles));
}

void BatchConverterDialog::onCancelClicked() {
    if (isConverting) {
        isCancelled = true;
        threadPool.clear();
        threadPool.waitForDone();
        cleanupSharedUpscayl();
        isConverting = false;
        updateUiState();
        statusLabel->setText(tr("Stopped by user."));
    } else {
        reject();
    }
}

void BatchConverterDialog::cleanupSharedUpscayl() {
#ifdef USE_UPSCAYL
    if (sharedResrgan) {
        delete sharedResrgan;
        sharedResrgan = nullptr;
    }
#endif
}

void BatchConverterDialog::collectResizeWidgets() {
    const QList<QWidget*> children = resizeContainer->findChildren<QWidget*>();
    for (QWidget *w : children) {
        if (w != resizeEnableCheckBox) m_resizeWidgets.append(w);
    }
}

void BatchConverterDialog::collectColorWidgets() {
    const QList<QWidget*> children = colorContainer->findChildren<QWidget*>();
    for (QWidget *w : children) {
        if (w != colorEnableCheckBox) m_colorWidgets.append(w);
    }
}

void BatchConverterDialog::setResizeWidgetsEnabled(bool enabled) {
    for (QWidget *w : m_resizeWidgets) if (w) w->setEnabled(enabled);
}

void BatchConverterDialog::setColorWidgetsEnabled(bool enabled) {
    for (QWidget *w : m_colorWidgets) if (w) w->setEnabled(enabled);
}

void BatchConverterDialog::onResizeEnabledChanged(bool enabled) {
    setResizeWidgetsEnabled(enabled);
    if (enabled) {
        onResizeRadioToggled();
        upscaylModelComboBox->setEnabled(useUpscaylCheckBox->isChecked());
    }
}

void BatchConverterDialog::onColorEnabledChanged(bool enabled) {
    setColorWidgetsEnabled(enabled);
}

// ==================== BatchWorkerTask ====================
void BatchWorkerTask::run() {
    if (dialog->isCancelled) return;
    QImage srcImg(srcPath);
    if (srcImg.isNull()) {
        if (dialog->isCancelled) return;
        QMetaObject::invokeMethod(
            dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
            Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Failed")),
            Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Load Error")),
            Q_ARG(bool, false));
        return;
    }

    QImage processedImg = srcImg;

    bool colorModified = (brightness != 0.0f || std::abs(contrast - 1.0f) > 0.001f ||
                          std::abs(saturation - 1.0f) > 0.001f || temp != 0.0f ||
                          tint != 0.0f || exposure != 0.0f || hue != 0.0f);
    if (colorModified) {
        if (dialog->isCancelled) return;
        std::shared_ptr<const QImage> srcPtr = std::make_shared<const QImage>(processedImg);
        QImage *adj = ImageLib::applyColorAdjustments(
            srcPtr, exposure, contrast, brightness, temp, tint, saturation, hue);
        if (adj) {
            processedImg = *adj;
            delete adj;
        }
    }

    if (useUpscayl) {
        if (dialog->isCancelled) return;
#ifdef USE_UPSCAYL
        if (dialog->sharedResrgan) {
            QImage imgRgba = processedImg.convertToFormat(QImage::Format_ARGB32);
            int inW = imgRgba.width(), inH = imgRgba.height();
            int scale = dialog->sharedResrgan->scale;
            if (scale <= 0) scale = 4;
            int outW = inW * scale;
            int outH = inH * scale;
            QImage outImg(outW, outH, QImage::Format_ARGB32);
            if (outImg.isNull()) {
                if (dialog->isCancelled) return;
                QMetaObject::invokeMethod(
                    dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Failed")),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Out of memory")),
                    Q_ARG(bool, false));
                return;
            }
            if (dialog->sharedResrgan->processPixels(
                    imgRgba.constBits(), inW, inH, outImg.bits(), outW, outH) == 0) {
                processedImg = outImg;
            }
        }
#endif
        if (doResize && processedImg.size() != targetSize) {
            if (dialog->isCancelled) return;
            QSize finalSize = targetSize;
            if (keepAspectRatio) finalSize = processedImg.size().scaled(targetSize, Qt::KeepAspectRatio);
            std::shared_ptr<const QImage> upscaledPtr = std::make_shared<const QImage>(processedImg);
            QImage *finalImg = ImageLib::scaled(upscaledPtr, finalSize, static_cast<ScalingFilter>(scalingFilter));
            if (finalImg) {
                processedImg = *finalImg;
                delete finalImg;
            }
        }
    } else if (doResize) {
        if (dialog->isCancelled) return;
        QSize finalSize = targetSize;
        if (keepAspectRatio) finalSize = processedImg.size().scaled(targetSize, Qt::KeepAspectRatio);
        std::shared_ptr<const QImage> imgPtr = std::make_shared<const QImage>(processedImg);
        QImage *scaledImg = ImageLib::scaled(imgPtr, finalSize, static_cast<ScalingFilter>(scalingFilter));
        if (scaledImg) {
            processedImg = *scaledImg;
            delete scaledImg;
        }
    }

    if (dialog->isCancelled) return;
    QByteArray formatBa = format.toLatin1();
    bool saved = processedImg.save(destPath, formatBa.constData(), quality);
    QString detailsStr = QString("%1 \xe2\x80\xa2 %2x%3")
                            .arg(format.toUpper())
                            .arg(processedImg.width())
                            .arg(processedImg.height());

    if (saved) {
        QMetaObject::invokeMethod(
            dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
            Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Done")),
            Q_ARG(QString, detailsStr), Q_ARG(bool, true));
    } else {
        QMetaObject::invokeMethod(
            dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
            Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Failed")),
            Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Save Error")),
            Q_ARG(bool, false));
    }
}

bool BatchConverterDialog::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == exposureSlider) {
            exposureSlider->setValue(0);
            return true;
        } else if (watched == contrastSlider) {
            contrastSlider->setValue(100);
            return true;
        } else if (watched == brightnessSlider) {
            brightnessSlider->setValue(0);
            return true;
        } else if (watched == saturationSlider) {
            saturationSlider->setValue(100);
            return true;
        } else if (watched == hueSlider) {
            hueSlider->setValue(0);
            return true;
        } else if (watched == tempSlider) {
            tempSlider->setValue(0);
            return true;
        } else if (watched == tintSlider) {
            tintSlider->setValue(0);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}