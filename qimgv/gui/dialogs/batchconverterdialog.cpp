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
    statusLabel->setStyleSheet(
        QString("font-weight: bold; color: %1; font-size: 12px;")
            .arg(colors.status_pending.name()));
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
    auto colors = settings->colorScheme();
    if (!success) {
        statusLabel->setStyleSheet(
            QString("font-weight: bold; color: %1; font-size: 12px;")
                .arg(colors.status_error.name()));
    } else if (statusText == tr("Processing...")) {
        statusLabel->setStyleSheet(
            QString("font-weight: bold; color: %1; font-size: 12px;")
                .arg(colors.status_processing.name()));
    } else if (statusText == tr("Done")) {
        statusLabel->setStyleSheet(
            QString("font-weight: bold; color: %1; font-size: 12px;")
                .arg(colors.status_success.name()));
    } else {
        statusLabel->setStyleSheet(
            QString("font-weight: bold; color: %1; font-size: 12px;")
                .arg(colors.status_pending.name()));
    }
    destInfoLabel->setText(details);
}

// ==================== LinkedSliderSpin ====================

LinkedSliderSpin::LinkedSliderSpin(const QString &labelText, double minVal, double maxVal, double defaultVal,
                                   double factor, int decimals, const QString &suffix, QWidget *parent)
    : QWidget(parent), m_factor(factor), m_defaultValue(defaultVal) {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    label = new QLabel(labelText, this);
    label->setMinimumWidth(80);
    layout->addWidget(label);

    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(static_cast<int>(std::round(minVal / factor)), static_cast<int>(std::round(maxVal / factor)));
    layout->addWidget(slider);

    spinBox = new QDoubleSpinBox(this);
    spinBox->setFixedSize(80, 24);
    spinBox->setAlignment(Qt::AlignCenter);
    spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spinBox->setRange(minVal, maxVal);
    spinBox->setSingleStep(decimals > 0 ? 0.1 : 1.0);
    spinBox->setDecimals(decimals);
    spinBox->setSuffix(suffix);
    layout->addWidget(spinBox);

    setValue(defaultVal);

    slider->installEventFilter(this);

    connect(slider, &QSlider::valueChanged, this, &LinkedSliderSpin::updateSpinBox);
    connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &LinkedSliderSpin::updateSlider);
}

double LinkedSliderSpin::value() const {
    return spinBox->value();
}

void LinkedSliderSpin::setValue(double val) {
    int sliderVal = static_cast<int>(std::round(val / m_factor));
    slider->blockSignals(true);
    slider->setValue(sliderVal);
    slider->blockSignals(false);

    spinBox->blockSignals(true);
    spinBox->setValue(val);
    spinBox->blockSignals(false);

    emit valueChanged(val);
}

void LinkedSliderSpin::updateSpinBox(int val) {
    double realVal = val * m_factor;
    if (std::abs(spinBox->value() - realVal) > 1e-7) {
        spinBox->blockSignals(true);
        spinBox->setValue(realVal);
        spinBox->blockSignals(false);
        emit valueChanged(realVal);
    }
}

void LinkedSliderSpin::updateSlider(double val) {
    int sliderVal = static_cast<int>(std::round(val / m_factor));
    if (slider->value() != sliderVal) {
        slider->blockSignals(true);
        slider->setValue(sliderVal);
        slider->blockSignals(false);
        emit valueChanged(val);
    }
}

bool LinkedSliderSpin::eventFilter(QObject *watched, QEvent *event) {
    if (watched == slider && event->type() == QEvent::MouseButtonDblClick) {
        setValue(m_defaultValue);
        return true;
    }
    return QWidget::eventFilter(watched, event);
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

    vColorLayout = new QVBoxLayout();

    exposureWidget = new LinkedSliderSpin(tr("Exposure:"), -2.0, 2.0, 0.0, 0.01, 2, "", this);
    vColorLayout->addWidget(exposureWidget);

    contrastWidget = new LinkedSliderSpin(tr("Contrast:"), 0.0, 300.0, 100.0, 1.0, 0, "%", this);
    vColorLayout->addWidget(contrastWidget);

    brightnessWidget = new LinkedSliderSpin(tr("Brightness:"), -100.0, 100.0, 0.0, 1.0, 0, "%", this);
    vColorLayout->addWidget(brightnessWidget);

    saturationWidget = new LinkedSliderSpin(tr("Saturation:"), 0.0, 200.0, 100.0, 1.0, 0, "%", this);
    vColorLayout->addWidget(saturationWidget);

    hueWidget = new LinkedSliderSpin(tr("Hue:"), -180.0, 180.0, 0.0, 1.0, 0, QString::fromUtf8("\xc2\xb0"), this);
    vColorLayout->addWidget(hueWidget);

    tempWidget = new LinkedSliderSpin(tr("Temperature:"), -50.0, 50.0, 0.0, 1.0, 0, "", this);
    vColorLayout->addWidget(tempWidget);

    tintWidget = new LinkedSliderSpin(tr("Tint:"), -50.0, 50.0, 0.0, 1.0, 0, "", this);
    vColorLayout->addWidget(tintWidget);

    ccLayout->addLayout(vColorLayout);

    // Add Reset Color Adjustments button
    QPushButton *resetColorButton = new QPushButton(tr("Reset Color Adjustments"), this);
    ccLayout->addWidget(resetColorButton);
    connect(resetColorButton, &QPushButton::clicked, this, [this]() {
        exposureWidget->setValue(0.0);
        contrastWidget->setValue(100.0);
        brightnessWidget->setValue(0.0);
        saturationWidget->setValue(100.0);
        hueWidget->setValue(0.0);
        tempWidget->setValue(0.0);
        tintWidget->setValue(0.0);
    });

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

    collectResizeWidgets();
    collectColorWidgets();

    setResizeWidgetsEnabled(resizeEnableCheckBox->isChecked());
    setColorWidgetsEnabled(colorEnableCheckBox->isChecked());

    connect(resizeEnableCheckBox, &QCheckBox::toggled, this, &BatchConverterDialog::onResizeEnabledChanged);
    connect(colorEnableCheckBox, &QCheckBox::toggled, this, &BatchConverterDialog::onColorEnabledChanged);

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
    resComboBox->addItem(tr("Original size"), QVariant());
    resComboBox->addItem("1280 x 720", QSize(1280, 720));
    resComboBox->addItem("1366 x 768", QSize(1366, 768));
    resComboBox->addItem("1440 x 900", QSize(1440, 900));
    resComboBox->addItem("1440 x 1050", QSize(1440, 1050));
    resComboBox->addItem("1600 x 1200", QSize(1600, 1200));
    resComboBox->addItem("1920 x 1080", QSize(1920, 1080));
    resComboBox->addItem("1920 x 1200", QSize(1920, 1200));
    resComboBox->addItem("2560 x 1080", QSize(2560, 1080));
    resComboBox->addItem("2560 x 1440", QSize(2560, 1440));
    resComboBox->addItem("2560 x 1600", QSize(2560, 1600));
    resComboBox->addItem("3840 x 1600", QSize(3840, 1600));
    resComboBox->addItem("3840 x 2160", QSize(3840, 2160));

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
    if (settings->hasUpscaylModels()) {
        QDir modelsDir(QCoreApplication::applicationDirPath() + "/models");
        QStringList filters; filters << "*.param";
        QStringList files = modelsDir.entryList(filters, QDir::Files);
        QStringList modelNames;
        for (const QString &file : files) {
            QFileInfo fi(file);
            QString modelName = fi.baseName();
            if (modelsDir.exists(modelName + ".bin")) modelNames.append(modelName);
        }
        upscaylModelComboBox->addItems(modelNames);
        int modelIdx = upscaylModelComboBox->findText(settings->upscaylModel());
        upscaylModelComboBox->setCurrentIndex(modelIdx != -1 ? modelIdx : 0);
        useUpscaylCheckBox->setChecked(settings->resizeUseUpscayl());
        bool resizeEnabled = resizeEnableCheckBox->isChecked();
        useUpscaylCheckBox->setEnabled(resizeEnabled);
        upscaylModelComboBox->setEnabled(resizeEnabled && useUpscaylCheckBox->isChecked());
    } else {
        useUpscaylCheckBox->setChecked(false);
        useUpscaylCheckBox->setEnabled(false);
        useUpscaylCheckBox->setToolTip(tr("No AI models found in models/ directory."));
        upscaylModelComboBox->setEnabled(false);
    }
#else
    useUpscaylCheckBox->setEnabled(false);
    useUpscaylCheckBox->setToolTip(tr("AI Upscaling is disabled in this build."));
    upscaylModelComboBox->setEnabled(false);
#endif

    connect(qualitySlider, &QSlider::valueChanged, this, &BatchConverterDialog::onQualitySliderChanged);
    connect(qualitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onQualitySpinBoxChanged);

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
    if (index > 0) {
        byAbsoluteSize->setChecked(true);
    }
    QVariant data = resComboBox->itemData(index);
    QSize res = data.isValid() ? data.toSize() : originalSize;
    if (keepAspectRatio->isChecked())
        targetSize = originalSize.scaled(res, Qt::KeepAspectRatio);
    else
        targetSize = originalSize.scaled(res, Qt::IgnoreAspectRatio);
    updateToTargetValues();
}

void BatchConverterDialog::onFitDesktop() {
    byAbsoluteSize->setChecked(true);
    targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatio);
    updateToTargetValues();
}

void BatchConverterDialog::onFillDesktop() {
    byAbsoluteSize->setChecked(true);
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
    upscaylModelComboBox->setEnabled(settings->hasUpscaylModels() && checked);
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

    QString pattern = patternEdit->text().trimmed();
    if (pattern.contains("..") || pattern.startsWith('/') || pattern.startsWith('\\') || (pattern.size() >= 2 && pattern[1] == ':')) {
        QMessageBox::warning(this, tr("Invalid Pattern"), tr("Filename pattern cannot contain path traversal sequences (..) or absolute paths."));
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

    if (resizeEnableCheckBox->isChecked()) {
        int maxDim = 12288;
        qint64 maxPixels = 100000000;
#ifdef USE_UPSCAYL
        if (settings->useUpscayl() || useUpscaylCheckBox->isChecked()) {
            maxDim = 16384;
            maxPixels = 268435456;
        }
#endif
        if (targetSize.width() > maxDim || targetSize.height() > maxDim ||
            (qint64)targetSize.width() * targetSize.height() > maxPixels) {
            double mpLimit = maxPixels / 1000000.0;
            QMessageBox::warning(this, tr("Resolution Limit Exceeded"),
                                 tr("Target resolution (%1x%2) exceeds safety limits.\n\n"
                                    "Maximum allowed dimension: %3 px\n"
                                    "Maximum allowed pixel count: %4 MP\n\n"
                                    "Please reduce the percentage or absolute size.")
                                 .arg(targetSize.width()).arg(targetSize.height())
                                 .arg(maxDim).arg(mpLimit, 0, 'f', 0));
            return;
        }
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
    float exposure = doColor ? static_cast<float>(exposureWidget->value()) : 0.0f;
    float contrast = doColor ? static_cast<float>(contrastWidget->value() / 100.0) : 1.0f;
    float brightness = doColor ? static_cast<float>(brightnessWidget->value() / 100.0) : 0.0f;
    float saturation = doColor ? static_cast<float>(saturationWidget->value() / 100.0) : 1.0f;
    float hue = doColor ? static_cast<float>(hueWidget->value()) : 0.0f;
    float temp = doColor ? static_cast<float>(tempWidget->value() / 100.0) : 0.0f;
    float tint = doColor ? static_cast<float>(tintWidget->value() / 100.0) : 0.0f;

    QString pattern = patternEdit->text();
    bool overwrite = overwriteCheckBox->isChecked();

    if (useUpscayl) {
        threadPool.setMaxThreadCount(1);
        if (!loadUpscaylModel(upscaylModel)) {
            return;
        }
    } else {
        threadPool.setMaxThreadCount(QThread::idealThreadCount());
    }

    int activeIndex = 0;
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        BatchItemWidget *widget = qobject_cast<BatchItemWidget*>(fileListWidget->itemWidget(item));
        if (!widget || !widget->isChecked()) continue;

        widget->setStatus(tr("Pending"), "", true);
        QString srcPath = widget->filePath();
        QFileInfo srcFi(srcPath);

        QString destPath = buildDestPath(srcFi, pattern, activeIndex + 1, formatExt, finalOutDir);

        if (destPath.isEmpty()) {
            processedFiles++;
            failedCount++;
            progressBar->setValue(processedFiles);
            widget->setStatus(tr("Failed"), tr("Invalid destination path"), false);
            continue;
        }

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

QString BatchConverterDialog::buildDestPath(const QFileInfo &srcFi, const QString &pattern, int index, const QString &formatExt, const QString &finalOutDir) const {
    QString safeBaseName = srcFi.baseName();
    safeBaseName.replace("/", "_");
    safeBaseName.replace("\\", "_");

    QString targetName = pattern;
    targetName.replace("{name}", safeBaseName);
    targetName.replace("{ext}", formatExt);
    targetName.replace("{date}", QDate::currentDate().toString("yyyy-MM-dd"));
    targetName.replace("{index}", QString::number(index));

    if (!targetName.contains(".")) {
        targetName += "." + formatExt;
    }

    QString canonicalOut = QDir(finalOutDir).canonicalPath();
    if (canonicalOut.isEmpty()) {
        canonicalOut = QDir(finalOutDir).absolutePath();
    }
    QString full = QDir::cleanPath(canonicalOut + "/" + targetName);
    if (!full.startsWith(canonicalOut + "/")) {
        return QString();
    }

    return full;
}

bool BatchConverterDialog::loadUpscaylModel(const QString &upscaylModel) {
#ifdef USE_UPSCAYL
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
        return false;
    }
    return true;
#else
    Q_UNUSED(upscaylModel);
    return false;
#endif
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
    for (QWidget *w : m_resizeWidgets) {
        if (w) {
            if (w == useUpscaylCheckBox && !settings->hasUpscaylModels()) {
                w->setEnabled(false);
            } else {
                w->setEnabled(enabled);
            }
        }
    }
}

void BatchConverterDialog::setColorWidgetsEnabled(bool enabled) {
    for (QWidget *w : m_colorWidgets) if (w) w->setEnabled(enabled);
}

void BatchConverterDialog::onResizeEnabledChanged(bool enabled) {
    setResizeWidgetsEnabled(enabled);
    if (enabled) {
        onResizeRadioToggled();
        upscaylModelComboBox->setEnabled(settings->hasUpscaylModels() && useUpscaylCheckBox->isChecked());
    }
}

void BatchConverterDialog::onColorEnabledChanged(bool enabled) {
    setColorWidgetsEnabled(enabled);
}

// ==================== BatchWorkerTask ====================
QImage BatchWorkerTask::applyResize(const QImage &img, const QSize &targetSize, bool keepAspect, int filter) {
    if (img.isNull()) return img;
    QSize finalSize = targetSize;
    if (keepAspect) {
        finalSize = img.size().scaled(targetSize, Qt::KeepAspectRatio);
    }
    std::shared_ptr<const QImage> imgPtr = std::make_shared<const QImage>(img);
    QImage scaledImg = ImageLib::scaled(imgPtr, finalSize, static_cast<ScalingFilter>(filter));
    return scaledImg.isNull() ? img : scaledImg;
}

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

    bool colorModified = (std::abs(brightness) > ImageLib::kAdjustEpsilon ||
                          std::abs(contrast - 1.0f) > ImageLib::kAdjustEpsilon ||
                          std::abs(saturation - 1.0f) > ImageLib::kAdjustEpsilon ||
                          std::abs(temp) > ImageLib::kAdjustEpsilon ||
                          std::abs(tint) > ImageLib::kAdjustEpsilon ||
                          std::abs(exposure) > ImageLib::kAdjustEpsilon ||
                          std::abs(hue) > ImageLib::kAdjustEpsilon);
    if (colorModified) {
        if (dialog->isCancelled) return;
        std::shared_ptr<const QImage> srcPtr = std::make_shared<const QImage>(processedImg);
        QImage adj = ImageLib::applyColorAdjustments(
            srcPtr, exposure, contrast, brightness, temp, tint, saturation, hue);
        if (!adj.isNull()) {
            processedImg = adj;
        }
    }

    if (useUpscayl) {
        if (dialog->isCancelled) return;
#ifdef USE_UPSCAYL
        if (dialog->sharedResrgan) {
            QImage imgRgba = processedImg.convertToFormat(QImage::Format_ARGB32);
            qint64 inW = imgRgba.width();
            qint64 inH = imgRgba.height();

            constexpr qint64 kMaxUpscalePixels = 64LL * 1024 * 1024;
            if (inW <= 0 || inH <= 0 || (inW * inH) > kMaxUpscalePixels) {
                if (dialog->isCancelled) return;
                QMetaObject::invokeMethod(
                    dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Failed")),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Invalid size")),
                    Q_ARG(bool, false));
                return;
            }

            int scale = dialog->sharedResrgan->scale;
            if (scale <= 0) scale = 4;

            qint64 outW = inW * scale;
            qint64 outH = inH * scale;

            constexpr qint64 kMaxIntVal = 2147483647; // std::numeric_limits<int>::max()
            if (outW > kMaxIntVal || outH > kMaxIntVal) {
                if (dialog->isCancelled) return;
                QMetaObject::invokeMethod(
                    dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Failed")),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog", "Output size too large")),
                    Q_ARG(bool, false));
                return;
            }

            QImage outImg(static_cast<int>(outW), static_cast<int>(outH), QImage::Format_ARGB32);
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
                    imgRgba.constBits(), static_cast<int>(inW), static_cast<int>(inH),
                    outImg.bits(), static_cast<int>(outW), static_cast<int>(outH)) == 0) {
                processedImg = outImg;
            }
        }
#endif
        if (doResize && processedImg.size() != targetSize) {
            if (dialog->isCancelled) return;
            processedImg = applyResize(processedImg, targetSize, keepAspectRatio, scalingFilter);
        }
    } else if (doResize) {
        if (dialog->isCancelled) return;
        processedImg = applyResize(processedImg, targetSize, keepAspectRatio, scalingFilter);
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