#include "batchconverterdialog.h"
#include "settings.h"
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

namespace {
constexpr int kBatchThumbnailExtent = 48;
constexpr int kBatchThumbnailCornerRadius = 6;
constexpr int kBatchDialogWidth = 1048;
constexpr int kBatchDialogHeight = 816;
constexpr qreal kMinimumDevicePixelRatio = 1.0;
}

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
    thumbLabel->setFixedSize(kBatchThumbnailExtent, kBatchThumbnailExtent);
    thumbLabel->setAlignment(Qt::AlignCenter);
    thumbLabel->setStyleSheet(
        QString("border: 1px solid %1; background-color: %2; border-radius: %3px;")
            .arg(colors.widget_border.name())
            .arg(colors.widget.name())
            .arg(kBatchThumbnailCornerRadius));
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

    statusLabel = new QLabel(BatchJobRules::itemStateText(BatchItemState::Pending), this);
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
    destInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    destInfoLabel->setWordWrap(true);
    destInfoLabel->setStyleSheet(
        QString("color: %1; font-size: 10px;").arg(colors.text_lc.name()));
    rightInfo->addWidget(destInfoLabel);

    mainLayout->addLayout(rightInfo, 0);

    QImageReader reader(filePath);
    imgSize = reader.size();
    srcInfoLabel->setText(BatchJobRules::sourceInfoText(QString::fromLatin1(reader.format()),
                                                        imgSize, size));

    connect(checkBox, &QCheckBox::toggled, this, &BatchItemWidget::checkedStateChanged);
}

void BatchItemWidget::setThumbnail(std::shared_ptr<Thumbnail> thumb) {
    if (thumb && thumb->pixmap() && !thumb->pixmap()->isNull()) {
        const qreal dpr =
            qMax(thumbLabel->devicePixelRatioF(), kMinimumDevicePixelRatio);
        const QSize physicalTargetSize(
            qRound(thumbLabel->width() * dpr),
            qRound(thumbLabel->height() * dpr));
        QPixmap scaledThumb = thumb->pixmap()->scaled(
            physicalTargetSize, Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        scaledThumb.setDevicePixelRatio(dpr);

        // Clip the thumbnail with a rounded rectangle
        QPixmap roundedThumb(scaledThumb.size());
        roundedThumb.setDevicePixelRatio(dpr);
        roundedThumb.fill(Qt::transparent);
        QPainter painter(&roundedThumb);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF logicalThumbRect(
            QPointF(), scaledThumb.deviceIndependentSize());
        QPainterPath path;
        path.addRoundedRect(logicalThumbRect, kBatchThumbnailCornerRadius,
                            kBatchThumbnailCornerRadius);
        painter.setClipPath(path);
        painter.drawPixmap(logicalThumbRect.topLeft(), scaledThumb);
        painter.end();

        thumbLabel->setPixmap(roundedThumb);
    }
}

void BatchItemWidget::setStatus(BatchItemState state, const QString &details) {
    statusLabel->setText(BatchJobRules::itemStateText(state));
    const auto colors = settings->colorScheme();
    QColor statusColor = colors.status_pending;
    switch (state) {
    case BatchItemState::Failed:
    case BatchItemState::Stopped:
        statusColor = colors.status_error;
        break;
    case BatchItemState::Processing:
        statusColor = colors.status_processing;
        break;
    case BatchItemState::Done:
        statusColor = colors.status_success;
        break;
    case BatchItemState::Pending:
        break;
    }
    statusLabel->setStyleSheet(
        QString("font-weight: bold; color: %1; font-size: 12px;").arg(statusColor.name()));
    destInfoLabel->setText(details);
}

// ==================== LinkedSliderSpin ====================

LinkedSliderSpin::LinkedSliderSpin(const BatchJobRules::ColorSliderSpec &spec, QWidget *parent)
    : QWidget(parent), m_factor(spec.step), m_defaultValue(spec.defaultValue) {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    label = new QLabel(spec.label, this);
    label->setMinimumWidth(80);
    layout->addWidget(label);

    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(static_cast<int>(std::round(spec.minimum / spec.step)),
                     static_cast<int>(std::round(spec.maximum / spec.step)));
    layout->addWidget(slider);

    spinBox = new QDoubleSpinBox(this);
    spinBox->setFixedSize(80, 24);
    spinBox->setAlignment(Qt::AlignCenter);
    spinBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    spinBox->setRange(spec.minimum, spec.maximum);
    spinBox->setSingleStep(spec.decimals > 0 ? 0.1 : 1.0);
    spinBox->setDecimals(spec.decimals);
    spinBox->setSuffix(spec.suffix);
    layout->addWidget(spinBox);

    setValue(spec.defaultValue);

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
    resize(kBatchDialogWidth, kBatchDialogHeight);
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
    scrollArea->setMinimumSize(500, 0);
    scrollArea->setMaximumSize(500, 16777215);
    scrollArea->setWidgetResizable(true);

    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(14, 0, 14, 0);

    setupFormatSection(scrollLayout);
    setupResizeSection(scrollLayout);
    setupTransformSection(scrollLayout);
    scrollLayout->addSpacing(6);
    setupColorSection(scrollLayout);
    scrollLayout->addSpacing(6);
    setupRenameSection(scrollLayout);
    scrollLayout->addStretch(1);

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
    qualitySpinBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
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
    auto *resizeModeGroup = new QButtonGroup(this);
    byPercentage = new QRadioButton(tr("By Percent:"), this);
    resizeModeGroup->addButton(byPercentage);
    lCol->addWidget(byPercentage);

    QHBoxLayout *percLayout = new QHBoxLayout();
    percLayout->setContentsMargins(20, 0, 0, 0);
    QLabel *lPerc = new QLabel(tr("Percent:"), this);
    lPerc->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    percent = new QDoubleSpinBox(this);
    percent->setMinimumSize(0, 30);
    percent->setAlignment(Qt::AlignCenter);
    percent->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    percent->setRange(BatchJobRules::kMinimumPercent, BatchJobRules::kMaximumPercent);
    percent->setValue(BatchJobRules::kDefaultPercent);
    percent->setDecimals(BatchJobRules::kPercentDecimals);
    percent->setEnabled(false);
    percLayout->addWidget(lPerc);
    percLayout->addWidget(percent);
    lCol->addLayout(percLayout);

    byAbsoluteSize = new QRadioButton(tr("By Absolute Size:"), this);
    resizeModeGroup->addButton(byAbsoluteSize);
    byAbsoluteSize->setChecked(true);
    lCol->addWidget(byAbsoluteSize);

    QVBoxLayout *absLayout = new QVBoxLayout();
    absLayout->setContentsMargins(20, 0, 0, 0);
    QHBoxLayout *wLayout = new QHBoxLayout();
    QLabel *lWidth = new QLabel(tr("Max Width:"), this);
    lWidth->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    width = new QSpinBox(this);
    width->setMinimumSize(0, 30);
    width->setAlignment(Qt::AlignCenter);
    width->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    width->setRange(BatchJobRules::kMinimumSide, BatchJobRules::kMaximumSide);
    width->setEnabled(false);
    wLayout->addWidget(lWidth);
    wLayout->addWidget(width);
    absLayout->addLayout(wLayout);

    QHBoxLayout *hLayout = new QHBoxLayout();
    QLabel *lHeight = new QLabel(tr("Max Height:"), this);
    lHeight->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    height = new QSpinBox(this);
    height->setMinimumSize(0, 30);
    height->setAlignment(Qt::AlignCenter);
    height->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    height->setRange(BatchJobRules::kMinimumSide, BatchJobRules::kMaximumSide);
    height->setEnabled(false);
    hLayout->addWidget(lHeight);
    hLayout->addWidget(height);
    absLayout->addLayout(hLayout);

    QHBoxLayout *resSizeLayout = new QHBoxLayout();
    QLabel *lComSizes = new QLabel(tr("Common sizes:"), this);
    lComSizes->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resComboBox = new QComboBox(this);
    resComboBox->setMinimumSize(0, 30);
    resSizeLayout->addWidget(lComSizes);
    resSizeLayout->addWidget(resComboBox);
    absLayout->addLayout(resSizeLayout);

    lCol->addLayout(absLayout);

    resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    lCol->addWidget(resetButton);

    QHBoxLayout *chkLayout = new QHBoxLayout();

    keepAspectRatio = new QCheckBox(tr("Keep aspect ratio"), this);
    keepAspectRatio->setChecked(true);
    chkLayout->addWidget(keepAspectRatio);

    aspectFitModeGroup = new QButtonGroup(this);
    aspectFitAutoRadio = new QRadioButton(tr("Auto"), this);
    aspectFitAutoRadio->setChecked(true);
    aspectFitWidthRadio = new QRadioButton(tr("Width"), this);
    aspectFitHeightRadio = new QRadioButton(tr("Height"), this);
    aspectFitModeGroup->addButton(aspectFitAutoRadio, static_cast<int>(AspectFitMode::Auto));
    aspectFitModeGroup->addButton(aspectFitWidthRadio, static_cast<int>(AspectFitMode::Width));
    aspectFitModeGroup->addButton(aspectFitHeightRadio, static_cast<int>(AspectFitMode::Height));
    chkLayout->addWidget(aspectFitAutoRadio);
    chkLayout->addWidget(aspectFitWidthRadio);
    chkLayout->addWidget(aspectFitHeightRadio);

    chkLayout->addStretch(1);

    useUpscaylCheckBox = new QCheckBox(tr("Upscayl"), this);
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
    rcLayout->addLayout(splitLayout);
    scrollLayout->addWidget(resizeContainer);
}

void BatchConverterDialog::setupTransformSection(QVBoxLayout *scrollLayout) {
    QHBoxLayout *rotationLayout = new QHBoxLayout();
    rotationGroup = new QButtonGroup(this);
    rotate0Radio = new QRadioButton(BatchJobRules::rotationLabel(RotationAngle::Rotate0), this);
    rotate0Radio->setChecked(true);
    rotate90Radio = new QRadioButton(BatchJobRules::rotationLabel(RotationAngle::Rotate90), this);
    rotate180Radio = new QRadioButton(BatchJobRules::rotationLabel(RotationAngle::Rotate180), this);
    rotate270Radio = new QRadioButton(BatchJobRules::rotationLabel(RotationAngle::Rotate270), this);
    rotationGroup->addButton(rotate0Radio, static_cast<int>(RotationAngle::Rotate0));
    rotationGroup->addButton(rotate90Radio, static_cast<int>(RotationAngle::Rotate90));
    rotationGroup->addButton(rotate180Radio, static_cast<int>(RotationAngle::Rotate180));
    rotationGroup->addButton(rotate270Radio, static_cast<int>(RotationAngle::Rotate270));
    rotationLayout->addWidget(rotate0Radio, 1);
    rotationLayout->addWidget(rotate90Radio, 1);
    rotationLayout->addWidget(rotate180Radio, 1);
    rotationLayout->addWidget(rotate270Radio, 1);
    scrollLayout->addLayout(rotationLayout);

    QHBoxLayout *flipLayout = new QHBoxLayout();
    flipHorizontalCheckBox = new QCheckBox(tr("Flip horizontal"), this);
    flipVerticalCheckBox = new QCheckBox(tr("Flip vertical"), this);
    flipLayout->addWidget(flipHorizontalCheckBox, 1);
    flipLayout->addWidget(flipVerticalCheckBox, 1);
    scrollLayout->addLayout(flipLayout);
}

void BatchConverterDialog::setupColorSection(QVBoxLayout *scrollLayout) {
    colorContainer = new QWidget(this);
    QVBoxLayout *ccLayout = new QVBoxLayout(colorContainer);
    ccLayout->setContentsMargins(0, 0, 0, 0);

    colorEnableCheckBox = new QCheckBox(tr("Color adjustments"), this);
    ccLayout->addWidget(colorEnableCheckBox);

    // Everything below the checkbox lives in its own widget so the whole
    // block can be collapsed (hidden) instead of just grayed out.
    colorAdjustmentsContent = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(colorAdjustmentsContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    vColorLayout = new QVBoxLayout();

    const QList<BatchJobRules::ColorSliderSpec> sliderSpecs = BatchJobRules::colorSliders();
    for (std::size_t i = 0; i < colorSliderWidgets.size(); ++i) {
        colorSliderWidgets[i] =
            new LinkedSliderSpin(sliderSpecs[static_cast<qsizetype>(i)], this);
        vColorLayout->addWidget(colorSliderWidgets[i]);
    }

    contentLayout->addLayout(vColorLayout);

    // Add Reset Color Adjustments button
    QPushButton *resetColorButton = new QPushButton(tr("Reset Color Adjustments"), this);
    contentLayout->addWidget(resetColorButton);
    connect(resetColorButton, &QPushButton::clicked, this, [this]() {
        const BatchJobRules::ColorSliderValues defaults = BatchJobRules::defaultColorValues();
        for (std::size_t i = 0; i < colorSliderWidgets.size(); ++i)
            colorSliderWidgets[i]->setValue(defaults[i]);
    });

    ccLayout->addWidget(colorAdjustmentsContent);

    // Collapsed by default; expands once the checkbox is enabled.
    colorAdjustmentsContent->setVisible(colorEnableCheckBox->isChecked());

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
    patternEdit = new QLineEdit(BatchJobRules::defaultPattern(), this);
    ocLayout->addWidget(patternEdit);

    QLabel *helpL = new QLabel(BatchJobRules::patternHelp(), this);
    QFont f = helpL->font(); f.setItalic(true); helpL->setFont(f);
    ocLayout->addWidget(helpL);

    overwriteCheckBox = new QCheckBox(tr("Overwrite existing files"), this);
    ocLayout->addWidget(overwriteCheckBox);

    scrollLayout->addWidget(outputContainer);
}

void BatchConverterDialog::setupBottomPanel(QVBoxLayout *mainLayout) {
    QHBoxLayout *bLayout = new QHBoxLayout();
    statusLabel = new QLabel(BatchJobRules::readyText(), this);
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

BatchConverterDialog::BatchConverterDialog(const QList<QString> &filePaths, QWidget *parent,
                                             const QString &defaultOutputDir)
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
                "QLineEdit, QSpinBox, QDoubleSpinBox { background-color: %5; color: %3; border: 1px solid %4; border-radius: 3px; padding: 3px; }"
                "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { background-color: %1; color: %2; border-color: %4; }"
                "QSpinBox::up-button, QDoubleSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::down-button { max-width: 0px; max-height: 0px; width: 0px; }"
                "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover,"
                "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: %6; }"
                "QListWidget { background-color: %8; border: 1px solid %4; color: %3; }"
                "QScrollBar:vertical { width: 13px; background-color: transparent; }"
                "QScrollBar::handle:vertical { background-color: %9; min-height: 30px; border-radius: 2px; }"
                "QScrollBar::handle:vertical:hover { background-color: %10; }"
                "QScrollBar::sub-page, QScrollBar::add-page { background: none; }"
                "QProgressBar { border: 1px solid %4; border-radius: 3px; text-align: center; color: %7; background-color: %8; }"
                "QProgressBar::chunk { background-color: %6; }")
        .arg(colors.widget.name(), colors.text.name(), colors.text_hc.name(),
             colors.widget_border.name(), colors.button.name(), colors.accent.name(),
             colors.text_hc2.name(), colors.folderview.name(), colors.scrollbar.name(),
             colors.scrollbar_hover.name());
    setStyleSheet(dialogStyle);

    m_converter = new BatchConverter(this);
    connect(m_converter, &BatchConverter::progressUpdated, this, &BatchConverterDialog::onProgressUpdated);
    connect(m_converter, &BatchConverter::finished, this, &BatchConverterDialog::onFinished);
    connect(m_converter, &BatchConverter::cancelled, this, &BatchConverterDialog::onCancelled);
    connect(m_converter, &BatchConverter::startFailed, this, &BatchConverterDialog::onStartFailed);

    for (const BatchJobRules::OutputFormat &format : BatchJobRules::outputFormats())
        formatComboBox->addItem(format.label, format.extension);

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
    m_itemWidgets.reserve(filePaths.size());
    for (const QString &path : filePaths) {
        QListWidgetItem *item = new QListWidgetItem(fileListWidget);
        BatchItemWidget *widget = new BatchItemWidget(path, this);
        item->setSizeHint(widget->sizeHint());
        fileListWidget->addItem(item);
        fileListWidget->setItemWidget(item, widget);
        m_itemWidgets.append(widget);
        connect(widget, &BatchItemWidget::checkedStateChanged, this, &BatchConverterDialog::onCheckedStateChanged);
        const qreal thumbnailDpr =
            qMax(widget->devicePixelRatioF(), kMinimumDevicePixelRatio);
        const int thumbnailExtent =
            qRound(kBatchThumbnailExtent * thumbnailDpr);
        thumbnailer->getThumbnailAsync(path, thumbnailExtent, true, false);
    }
    totalFiles = filePaths.size();
    updateSelectedCount();

    const QString initialOutputDir = BatchJobRules::initialOutputDirectory(
        defaultOutputDir, filePaths.isEmpty() ? QString() : filePaths.first());
    if (!initialOutputDir.isEmpty()) {
        outDirEdit->setText(initialOutputDir);
    }

    for (const BatchJobRules::ScalingFilterOption &filter : BatchJobRules::scalingFilters())
        filterComboBox->addItem(filter.label, filter.filter);
    filterComboBox->setCurrentIndex(BatchJobRules::defaultScalingFilterIndex());

    for (const BatchJobRules::CommonSize &commonSize : BatchJobRules::commonSizes())
        resComboBox->addItem(commonSize.label,
                             commonSize.size.isValid() ? QVariant(commonSize.size) : QVariant());

    if (!filePaths.isEmpty()) {
        QImageReader r(filePaths[0]);
        originalSize = r.size();
    } else {
        originalSize = BatchJobRules::fallbackOriginalSize();
    }
    targetSize = originalSize;
    width->setValue(originalSize.width());
    height->setValue(originalSize.height());
    resetButton->setText(tr("Reset: %1 x %2").arg(originalSize.width()).arg(originalSize.height()));

    percent->setEnabled(false);

    if (settings->hasUpscaylModels()) {
        upscaylModelComboBox->addItems(settings->availableUpscaylModels());
        int modelIdx = upscaylModelComboBox->findText(settings->batchUpscaylModel());
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
    updateUpscaylAvailability();

    connect(qualitySlider, &QSlider::valueChanged, this, &BatchConverterDialog::onQualitySliderChanged);
    connect(qualitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onQualitySpinBoxChanged);

    connect(byPercentage, &QRadioButton::toggled, this, &BatchConverterDialog::onResizeRadioToggled);
    connect(byAbsoluteSize, &QRadioButton::toggled, this, &BatchConverterDialog::onResizeRadioToggled);
    connect(percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &BatchConverterDialog::onPercentChanged);
    connect(width, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onWidthChanged);
    connect(height, qOverload<int>(&QSpinBox::valueChanged), this, &BatchConverterDialog::onHeightChanged);
    connect(resComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &BatchConverterDialog::onCommonResolutionChanged);
    connect(resetButton, &QPushButton::clicked, this, &BatchConverterDialog::onResetSizes);
    connect(keepAspectRatio, &QCheckBox::toggled, this, &BatchConverterDialog::onKeepAspectRatioToggled);
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
    disconnect(m_converter, nullptr, this, nullptr);
    m_converter->cancel();
    m_converter->setParent(nullptr);
    m_converter->enableSelfDestruct();

    disconnect(thumbnailer, nullptr, this, nullptr);
    thumbnailer->clearTasks();
    thumbnailer->setParent(nullptr);
    thumbnailer->enableSelfDestruct();
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
        // Percent mode applies the same percentage to both dimensions per
        // file, so Auto/W/H are equivalent - keep the group disabled.
        aspectFitAutoRadio->setEnabled(false);
        aspectFitWidthRadio->setEnabled(false);
        aspectFitHeightRadio->setEnabled(false);
        onPercentChanged(percent->value());
    } else {
        onKeepAspectRatioToggled(keepAspectRatio->isChecked());
        onWidthChanged(width->value());
    }
}

void BatchConverterDialog::onKeepAspectRatioToggled(bool checked) {
    bool enable = checked && !byPercentage->isChecked();
    aspectFitAutoRadio->setEnabled(enable);
    aspectFitWidthRadio->setEnabled(enable);
    aspectFitHeightRadio->setEnabled(enable);
}

void BatchConverterDialog::onPercentChanged(double val) {
    targetSize = BatchJobRules::percentTarget(originalSize, val);
    updateToTargetValues();
    updateUpscaylAvailability();
}

void BatchConverterDialog::onWidthChanged(int val) {
    lastEdited = 0;
    targetSize = BatchJobRules::widthEdited(originalSize, targetSize, val,
                                            keepAspectRatio->isChecked());
    updateToTargetValues();
    updateUpscaylAvailability();
}

void BatchConverterDialog::onHeightChanged(int val) {
    lastEdited = 1;
    targetSize = BatchJobRules::heightEdited(originalSize, targetSize, val,
                                             keepAspectRatio->isChecked());
    updateToTargetValues();
    updateUpscaylAvailability();
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
    // Set the bounding box directly; keepAspectRatio is applied per-file during conversion
    targetSize = data.isValid() ? data.toSize() : originalSize;
    updateToTargetValues();
    updateUpscaylAvailability();
}

void BatchConverterDialog::onResetSizes() {
    resComboBox->blockSignals(true);
    resComboBox->setCurrentIndex(0);
    resComboBox->blockSignals(false);
    percent->blockSignals(true);
    percent->setValue(BatchJobRules::kDefaultPercent);
    percent->blockSignals(false);
    targetSize = originalSize;
    updateToTargetValues();
    updateUpscaylAvailability();
}

void BatchConverterDialog::onUseUpscaylToggled(bool checked) {
    upscaylModelComboBox->setEnabled(settings->hasUpscaylModels() && resizeEnableCheckBox->isChecked() && checked);
}

void BatchConverterDialog::updateUpscaylAvailability() {
    if (!settings->hasUpscaylModels())
        return; // already disabled with its own tooltip

    bool available = resizeEnableCheckBox->isChecked();
    useUpscaylCheckBox->setEnabled(available);
    useUpscaylCheckBox->setToolTip(QString());
    upscaylModelComboBox->setEnabled(available && useUpscaylCheckBox->isChecked());
}

void BatchConverterDialog::onSelectAll() {
    for (BatchItemWidget *widget : m_itemWidgets) {
        if (widget) widget->setChecked(true);
    }
}

void BatchConverterDialog::onDeselectAll() {
    for (BatchItemWidget *widget : m_itemWidgets) {
        if (widget) widget->setChecked(false);
    }
}

void BatchConverterDialog::onCheckedStateChanged() {
    updateSelectedCount();
}

void BatchConverterDialog::updateSelectedCount() {
    int checkedCount = 0;
    qint64 totalSizeBytes = 0;
    for (BatchItemWidget *widget : m_itemWidgets) {
        if (widget && widget->isChecked()) {
            checkedCount++;
            totalSizeBytes += widget->fileSize();
        }
    }
    selectedCountLabel->setText(BatchJobRules::selectionText(checkedCount, totalSizeBytes));
}

void BatchConverterDialog::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"), outDirEdit->text());
    if (!dir.isEmpty()) outDirEdit->setText(dir);
}

void BatchConverterDialog::onFormatChanged(int index) {
    const QString ext = formatComboBox->itemData(index).toString();
    const BatchJobRules::QualityScale scale = BatchJobRules::qualityScaleFor(ext);
    const QString toolTip = BatchJobRules::qualityToolTip(scale.kind);
    const bool hasQuality = scale.kind != BatchJobRules::QualityKind::None;
    qualitySlider->setEnabled(hasQuality);
    qualitySpinBox->setEnabled(hasQuality);
    qualitySlider->setToolTip(toolTip);
    qualitySpinBox->setToolTip(toolTip);
    if (!hasQuality)
        return;

    const BatchJobRules::SaveQualityDefaults defaults{.jpeg = settings->JPEGSaveQuality(),
                                                      .png = settings->pngSaveQuality(),
                                                      .modern = settings->modernSaveQuality()};
    const int quality = BatchJobRules::defaultQualityFor(ext, defaults);
    qualitySlider->blockSignals(true);
    qualitySpinBox->blockSignals(true);
    qualitySlider->setRange(scale.minimum, scale.maximum);
    qualitySpinBox->setRange(scale.minimum, scale.maximum);
    qualitySlider->setValue(quality);
    qualitySpinBox->setValue(quality);
    qualitySlider->blockSignals(false);
    qualitySpinBox->blockSignals(false);
}

void BatchConverterDialog::updateUiState() {
    scrollArea->setEnabled(!isConverting && !isCancelling);
    convertButton->setEnabled(!isConverting && !isCancelling);
    selectAllBtn->setEnabled(!isConverting && !isCancelling);
    deselectAllBtn->setEnabled(!isConverting && !isCancelling);
    if (isCancelling) {
        cancelButton->setText(BatchJobRules::stoppingText());
        cancelButton->setEnabled(false);
    } else {
        cancelButton->setText(isConverting ? tr("Stop") : tr("Cancel"));
        cancelButton->setEnabled(true);
    }
}

void BatchConverterDialog::onConvertClicked() {
    if (isConverting) return;

    int checkedCount = 0;
    for (BatchItemWidget *widget : m_itemWidgets) {
        if (widget && widget->isChecked()) checkedCount++;
    }

    const BatchJobRules::StartCheck check{.outputDirectory = outDirEdit->text(),
                                          .pattern = patternEdit->text(),
                                          .selectedCount = checkedCount,
                                          .resize = resizeEnableCheckBox->isChecked(),
                                          .useUpscayl = useUpscaylCheckBox->isChecked(),
                                          .targetSize = targetSize};
    const BatchJobRules::StartProblem problem = BatchJobRules::checkStart(check);
    if (problem != BatchJobRules::StartProblem::None) {
        const BatchJobRules::Message message = BatchJobRules::startProblemMessage(problem, check);
        QMessageBox::warning(this, message.title, message.text);
        return;
    }

    isConverting = true;
    m_conversionStarted = true;
    processedFiles = 0;

    progressBar->setMaximum(checkedCount);
    progressBar->setValue(0);
    statusLabel->setText(BatchJobRules::processingText());
    updateUiState();
    startConversion();
}

void BatchConverterDialog::startConversion() {
    BatchJobRules::BatchJobDraft draft;
    BatchJob &job = draft.job;
    job.format = formatComboBox->currentData().toString();
    job.quality = qualitySlider->value();
    job.doResize = resizeEnableCheckBox->isChecked();
    job.resizeByPercent = byPercentage->isChecked();
    job.resizePercent = percent->value();
    job.targetSize = targetSize;
    job.keepAspectRatio = keepAspectRatio->isChecked();
    job.aspectFitMode = static_cast<AspectFitMode>(aspectFitModeGroup->checkedId());
    // Per-file upscaling check for mixed resolution batches lives inside BatchConverter itself.
    job.useUpscayl = useUpscaylCheckBox->isChecked();
    job.upscaylModel = upscaylModelComboBox->currentText();
    job.rotation = static_cast<RotationAngle>(rotationGroup->checkedId());
    job.flipHorizontal = flipHorizontalCheckBox->isChecked();
    job.flipVertical = flipVerticalCheckBox->isChecked();

    settings->setResizeUseUpscayl(useUpscaylCheckBox->isChecked());
    settings->setBatchUpscaylModel(job.upscaylModel);
    settings->sync();

    job.scalingFilter = filterComboBox->currentData().toInt();
    draft.colorEnabled = colorEnableCheckBox->isChecked();
    for (std::size_t i = 0; i < colorSliderWidgets.size(); ++i)
        draft.colorValues[i] = colorSliderWidgets[i]->value();

    job.pattern = patternEdit->text();
    job.overwrite = overwriteCheckBox->isChecked();
    job.outputDir = outDirEdit->text();
    job.createSubfolder = subfolderCheckBox->isChecked();

    QList<int> selectedIndices;
    for (int i = 0; i < m_itemWidgets.size(); ++i) {
        BatchItemWidget *widget = m_itemWidgets[i];
        if (widget && widget->isChecked()) {
            selectedIndices.append(i);
            widget->setStatus(BatchItemState::Pending);
        }
    }

    m_converter->start(inputPaths, selectedIndices, BatchJobRules::finalJob(draft));
}

void BatchConverterDialog::onProgressUpdated(int index, BatchItemState state, QString details) {
    if (index >= 0 && index < m_itemWidgets.size()) {
        BatchItemWidget *widget = m_itemWidgets[index];
        if (widget) widget->setStatus(state, details);
    }

    if (state != BatchItemState::Processing) {
        processedFiles++;
        progressBar->setValue(processedFiles);
        statusLabel->setText(BatchJobRules::processedText(processedFiles, progressBar->maximum()));
    }
}

void BatchConverterDialog::onFinished(int successCount, int failedCount, int totalCount) {
    isConverting = false;
    updateUiState();
    statusLabel->setText(BatchJobRules::finishedText(successCount, failedCount));
    const BatchJobRules::Message message =
        BatchJobRules::completedMessage(successCount, failedCount, totalCount);
    QMessageBox::information(this, message.title, message.text);
}

void BatchConverterDialog::onCancelClicked() {
    if (isConverting) {
        if (!isCancelling) {
            isCancelling = true;
            m_converter->cancel();
            updateUiState();
            statusLabel->setText(BatchJobRules::stoppingText());
        }
    } else if (isCancelling) {
        // Do nothing, wait for cancellation to finish
    } else {
        reject();
    }
}

void BatchConverterDialog::onCancelled(int successCount, int failedCount, int totalCount) {
    isConverting = false;
    isCancelling = false;
    updateUiState();
    statusLabel->setText(BatchJobRules::stoppedText(successCount, failedCount));
}

void BatchConverterDialog::onStartFailed(const QString &reason) {
    isConverting = false;
    updateUiState();
    statusLabel->setText(BatchJobRules::abortedText());
    const BatchJobRules::Message message = BatchJobRules::startFailedMessage(reason);
    QMessageBox::warning(this, message.title, message.text);
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
    // Collapse/expand the slider block itself, not just gray it out.
    if (colorAdjustmentsContent) colorAdjustmentsContent->setVisible(enabled);
}

void BatchConverterDialog::onResizeEnabledChanged(bool enabled) {
    setResizeWidgetsEnabled(enabled);
    if (enabled) {
        onResizeRadioToggled();
    }
    updateUpscaylAvailability();
}

void BatchConverterDialog::onColorEnabledChanged(bool enabled) {
    setColorWidgetsEnabled(enabled);
}
