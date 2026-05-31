#include "batchconverterdialog.h"
#include "ui_batchconverterdialog.h"
#include <QCoreApplication>
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
#include <QSpacerItem>
#include <QTime>
#include <QVBoxLayout>

#ifdef USE_UPSCAYL
#include "realesrgan.h"
#endif

// ==================== BatchItemWidget Implementation ====================

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
      QString("border: 1px solid %1; background-color: %2;")
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

  mainLayout->addLayout(leftInfo,
                        1); // takes all available space but can shrink

  QVBoxLayout *rightInfo = new QVBoxLayout();
  rightInfo->setSpacing(2);
  rightInfo->setAlignment(Qt::AlignRight);

  statusLabel = new QLabel(tr("Pending"), this);
  statusLabel->setMinimumWidth(80);
  statusLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  statusLabel->setAlignment(Qt::AlignRight);
  statusLabel->setStyleSheet(
      "font-weight: bold; color: #ffaa00; font-size: 12px;");
  rightInfo->addWidget(statusLabel);

  destInfoLabel = new QLabel(this);
  destInfoLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  destInfoLabel->setAlignment(Qt::AlignRight);
  destInfoLabel->setStyleSheet(
      QString("color: %1; font-size: 10px;").arg(colors.text_lc.name()));
  rightInfo->addWidget(destInfoLabel);

  mainLayout->addLayout(rightInfo, 0); // fixed size, always fully visible

  // Read metadata and load downscaled thumbnail
  QString sizeStr;
  if (size > 1024 * 1024) {
    sizeStr = QString::number(size / (1024.0 * 1024.0), 'f', 1) + " MB";
  } else {
    sizeStr = QString::number(size / 1024.0, 'f', 1) + " KB";
  }

  QImageReader reader(filePath);
  imgSize = reader.size();
  QString origFormat = reader.format().toUpper();
  srcInfoLabel->setText(QString("%1 • %2x%3 • %4")
                            .arg(origFormat)
                            .arg(imgSize.width())
                            .arg(imgSize.height())
                            .arg(sizeStr));

  connect(checkBox, &QCheckBox::toggled, this,
          &BatchItemWidget::checkedStateChanged);
}

void BatchItemWidget::setThumbnail(std::shared_ptr<Thumbnail> thumb) {
  if (thumb && thumb->pixmap() && !thumb->pixmap()->isNull()) {
    thumbLabel->setPixmap(thumb->pixmap()->scaled(
        thumbLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }
}

void BatchItemWidget::setStatus(const QString &statusText,
                                const QString &details, bool success) {
  statusLabel->setText(statusText);
  if (!success) {
    statusLabel->setStyleSheet(
        "font-weight: bold; color: #ff3333; font-size: 12px;");
  } else if (statusText == tr("Processing...")) {
    statusLabel->setStyleSheet(
        "font-weight: bold; color: #33aaff; font-size: 12px;");
  } else if (statusText == tr("Done")) {
    statusLabel->setStyleSheet(
        "font-weight: bold; color: #33cc33; font-size: 12px;");
  } else {
    statusLabel->setStyleSheet(
        "font-weight: bold; color: #ffaa00; font-size: 12px;");
  }
  destInfoLabel->setText(details);
}

// ==================== BatchConverterDialog Implementation ====================

BatchConverterDialog::BatchConverterDialog(const QList<QString> &filePaths,
                                           QWidget *parent)
    : QDialog(parent), ui(new Ui::BatchConverterDialog), inputPaths(filePaths) {
  ui->setupUi(this);
  setWindowModality(Qt::ApplicationModal);

  // Collect widgets for Resize block (excluding the enable checkbox itself)
  collectResizeWidgets();
  // Collect widgets for Color adjustments block
  collectColorWidgets();

  // Set initial enabled state based on checkboxes
  setResizeWidgetsEnabled(ui->resizeEnableCheckBox->isChecked());
  setColorWidgetsEnabled(ui->colorEnableCheckBox->isChecked());

  // Connect toggled signals
  connect(ui->resizeEnableCheckBox, &QCheckBox::toggled,
          this, &BatchConverterDialog::onResizeEnabledChanged);
  connect(ui->colorEnableCheckBox, &QCheckBox::toggled,
          this, &BatchConverterDialog::onColorEnabledChanged);

  // Style dialog with active theme colors
  auto colors = settings->colorScheme();
  QString dialogStyle =
      QString("QDialog {"
              "  background-color: %1;"
              "  color: %2;"
              "}"
              "QScrollArea, QScrollArea > QWidget > QWidget {"
              "  background-color: %1;"
              "  border: none;"
              "}"
              "QGroupBox {"
              "  background-color: %1;"
              "  color: %3;"
              "  border: 1px solid %4;"
              "  border-radius: 4px;"
              "  margin-top: 10px;"
              "  padding-top: 12px;"
              "}"
              "QGroupBox::title {"
              "  subcontrol-origin: margin;"
              "  subcontrol-position: top left;"
              "  left: 8px;"
              "  padding: 0 3px;"
              "  color: %3;"
              "}"
              "QLabel {"
              "  color: %3;"
              "}"
              "QCheckBox, QRadioButton {"
              "  color: %3;"
              "}"
              "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
              "  background-color: %5;"
              "  color: %3;"
              "  border: 1px solid %4;"
              "  border-radius: 3px;"
              "  padding: 3px;"
              "}"
              "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, "
              "QComboBox:hover,"
              "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, "
              "QComboBox:focus {"
              "  border-color: %6;"
              "}"
              "QPushButton {"
              "  background-color: %5;"
              "  color: %3;"
              "  border: 1px solid %4;"
              "  border-radius: 3px;"
              "  padding: 5px 10px;"
              "}"
              "QPushButton:hover {"
              "  background-color: %7;"
              "  color: %8;"
              "}"
              "QPushButton:pressed {"
              "  background-color: %9;"
              "}"
              "QListWidget {"
              "  background-color: %10;"
              "  border: 1px solid %4;"
              "  color: %3;"
              "}"
              "QScrollBar:vertical {"
              "  width: 13px;"
              "  background-color: transparent;"
              "}"
              "QScrollBar::handle:vertical {"
              "  background-color: %11;"
              "  min-height: 30px;"
              "  border-radius: 2px;"
              "}"
              "QScrollBar::handle:vertical:hover {"
              "  background-color: %12;"
              "}"
              "QScrollBar::sub-page, QScrollBar::add-page {"
              "  background: none;"
              "}"
              "QProgressBar {"
              "  border: 1px solid %4;"
              "  border-radius: 3px;"
              "  text-align: center;"
              "  color: %8;"
              "  background-color: %10;"
              "}"
              "QProgressBar::chunk {"
              "  background-color: %6;"
              "}")
          .arg(colors.widget.name())           // %1
          .arg(colors.text.name())             // %2
          .arg(colors.text_hc.name())          // %3
          .arg(colors.widget_border.name())    // %4
          .arg(colors.button.name())           // %5
          .arg(colors.accent.name())           // %6
          .arg(colors.button_hover.name())     // %7
          .arg(colors.text_hc2.name())         // %8
          .arg(colors.button_pressed.name())   // %9
          .arg(colors.folderview.name())       // %10
          .arg(colors.scrollbar.name())        // %11
          .arg(colors.scrollbar_hover.name()); // %12

  setStyleSheet(dialogStyle);

  // Initialize ThreadPool
  threadPool.setMaxThreadCount(QThread::idealThreadCount());

  // 1. Format combobox setup
  ui->formatComboBox->addItem("JPEG (*.jpg *.jpeg *.jpe *.jfif)", "jpg");
  ui->formatComboBox->addItem("PNG (*.png)", "png");
  ui->formatComboBox->addItem("WebP (*.webp)", "webp");
  ui->formatComboBox->addItem("JPEG-XL (*.jxl)", "jxl");
  ui->formatComboBox->addItem("AVIF (*.avif *.avifs)", "avif");
  ui->formatComboBox->addItem("BMP (*.bmp)", "bmp");
  ui->formatComboBox->addItem("TIFF (*.tif *.tiff)", "tif");

  // 2. Populate File List Queue
  thumbnailer = new Thumbnailer();
  connect(thumbnailer, &Thumbnailer::thumbnailReady, this,
          [this](std::shared_ptr<Thumbnail> thumb, QString filePath) {
            // Find the widget for this path and set thumbnail
            for (int i = 0; i < ui->fileListWidget->count(); ++i) {
              QListWidgetItem *item = ui->fileListWidget->item(i);
              auto *w = qobject_cast<BatchItemWidget *>(
                  ui->fileListWidget->itemWidget(item));
              if (w && w->filePath() == filePath) {
                w->setThumbnail(thumb);
                break;
              }
            }
          });

  ui->fileListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->fileListWidget->setResizeMode(QListView::Adjust);
  for (const QString &path : filePaths) {
    QListWidgetItem *item = new QListWidgetItem(ui->fileListWidget);
    BatchItemWidget *widget = new BatchItemWidget(path, this);
    item->setSizeHint(widget->sizeHint());
    ui->fileListWidget->addItem(item);
    ui->fileListWidget->setItemWidget(item, widget);
    connect(widget, &BatchItemWidget::checkedStateChanged, this,
            &BatchConverterDialog::onCheckedStateChanged);
    // Request thumbnail from cache/thumbnailer asynchronously
    thumbnailer->getThumbnailAsync(path, 48, true, false);
  }
  totalFiles = filePaths.size();
  updateSelectedCount();

  // 3. Set Default Output Folder
  if (!filePaths.isEmpty()) {
    ui->outDirEdit->setText(QFileInfo(filePaths[0]).absolutePath());
  }

  // 4. Populate filters
  ui->filterComboBox->addItem("Nearest", QI_FILTER_NEAREST);
  ui->filterComboBox->addItem("Bilinear", QI_FILTER_BILINEAR);
#ifdef USE_OPENCV
  ui->filterComboBox->addItem("Bilinear+sharpen (OpenCV)",
                              QI_FILTER_CV_BILINEAR_SHARPEN);
  ui->filterComboBox->addItem("Bicubic (OpenCV)", QI_FILTER_CV_CUBIC);
  ui->filterComboBox->addItem("Bicubic+sharpen (OpenCV)",
                              QI_FILTER_CV_CUBIC_SHARPEN);
  ui->filterComboBox->addItem("Lanczos (OpenCV)", QI_FILTER_CV_LANCZOS);
  ui->filterComboBox->addItem("Area (OpenCV)", QI_FILTER_CV_AREA);
  ui->filterComboBox->addItem("Smart sharpen (OpenCV)", QI_FILTER_CV_SMART);
#endif
#ifdef USE_OPENCV
  int smartIndex = ui->filterComboBox->findData(QI_FILTER_CV_SMART);
  if (smartIndex != -1) {
    ui->filterComboBox->setCurrentIndex(smartIndex);
  } else {
    ui->filterComboBox->setCurrentIndex(1); // Bilinear default
  }
#else
  ui->filterComboBox->setCurrentIndex(1); // Bilinear default
#endif

  // 5. Populate resolutions & setup ResizeDialog behavior
  desktopSize = qApp->primaryScreen()->size();
  ui->resComboBox->addItem("Original size");
  ui->resComboBox->addItem("1366 x 768");
  ui->resComboBox->addItem("1440 x 900");
  ui->resComboBox->addItem("1440 x 1050");
  ui->resComboBox->addItem("1600 x 1200");
  ui->resComboBox->addItem("1920 x 1080");
  ui->resComboBox->addItem("1920 x 1200");
  ui->resComboBox->addItem("2560 x 1080");
  ui->resComboBox->addItem("2560 x 1440");
  ui->resComboBox->addItem("2560 x 1600");
  ui->resComboBox->addItem("3840 x 1600");
  ui->resComboBox->addItem("3840 x 2160");

  if (!filePaths.isEmpty()) {
    QImageReader r(filePaths[0]);
    originalSize = r.size();
  } else {
    originalSize = QSize(2560, 1440);
  }
  targetSize = originalSize;
  ui->width->setValue(originalSize.width());
  ui->height->setValue(originalSize.height());
  ui->resetButton->setText(tr("Reset: %1 x %2")
                               .arg(originalSize.width())
                               .arg(originalSize.height()));

  // Disable percent or width/height initially based on radio selection
  ui->percent->setEnabled(false);

  // 6. Populate Upscayl Models
#ifdef USE_UPSCAYL
  QDir modelsDir(QCoreApplication::applicationDirPath() + "/models");
  QStringList filters;
  filters << "*.param";
  QStringList files = modelsDir.entryList(filters, QDir::Files);
  QStringList modelNames;
  for (const QString &file : files) {
    QFileInfo fi(file);
    QString modelName = fi.baseName();
    if (modelsDir.exists(modelName + ".bin")) {
      modelNames.append(modelName);
    }
  }
  if (modelNames.isEmpty()) {
    modelNames.append("4xLSDIRCompactC3");
  }
  ui->upscaylModelComboBox->addItems(modelNames);

  // Enable AI upscaling controls and restore from settings
  int modelIdx = ui->upscaylModelComboBox->findText(settings->upscaylModel());
  if (modelIdx != -1) {
    ui->upscaylModelComboBox->setCurrentIndex(modelIdx);
  } else {
    ui->upscaylModelComboBox->setCurrentIndex(0);
  }

  ui->useUpscaylCheckBox->setChecked(settings->resizeUseUpscayl());
  ui->useUpscaylCheckBox->setEnabled(true);
  ui->upscaylModelComboBox->setEnabled(ui->useUpscaylCheckBox->isChecked());
#else
  ui->useUpscaylCheckBox->setEnabled(false);
  ui->useUpscaylCheckBox->setToolTip(
      tr("AI Upscaling is disabled in this build."));
  ui->upscaylModelComboBox->setEnabled(false);
#endif

  // 7. Connect Quality Slider & Spinbox
  connect(ui->qualitySlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onQualitySliderChanged);
  connect(ui->qualitySpinBox, qOverload<int>(&QSpinBox::valueChanged), this,
          &BatchConverterDialog::onQualitySpinBoxChanged);

  // 8. Connect Color Adjustment sliders & spinboxes
  connect(ui->exposureSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onExposureSliderChanged);
  connect(ui->exposureSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, &BatchConverterDialog::onExposureSpinBoxChanged);
  connect(ui->contrastSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onContrastSliderChanged);
  connect(ui->contrastSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, &BatchConverterDialog::onContrastSpinBoxChanged);
  connect(ui->saturationSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onSaturationSliderChanged);
  connect(ui->saturationSpinBox,
          qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &BatchConverterDialog::onSaturationSpinBoxChanged);
  connect(ui->tempSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onTempSliderChanged);
  connect(ui->tempSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, &BatchConverterDialog::onTempSpinBoxChanged);
  connect(ui->tintSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onTintSliderChanged);
  connect(ui->tintSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, &BatchConverterDialog::onTintSpinBoxChanged);
  connect(ui->brightnessSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onBrightnessSliderChanged);
  connect(ui->brightnessSpinBox,
          qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &BatchConverterDialog::onBrightnessSpinBoxChanged);
  connect(ui->hueSlider, &QSlider::valueChanged, this,
          &BatchConverterDialog::onHueSliderChanged);
  connect(ui->hueSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, &BatchConverterDialog::onHueSpinBoxChanged);

  // Initial slider values
  ui->exposureSpinBox->setValue(0.0);
  ui->contrastSpinBox->setValue(1.0);
  ui->saturationSpinBox->setValue(1.0);
  ui->tempSpinBox->setValue(0.0);
  ui->tintSpinBox->setValue(0.0);
  ui->brightnessSpinBox->setValue(0.0);
  ui->hueSpinBox->setValue(0.0);

  // 9. Connect Resize Dialog events
  connect(ui->byPercentage, &QRadioButton::toggled, this,
          &BatchConverterDialog::onResizeRadioToggled);
  connect(ui->byAbsoluteSize, &QRadioButton::toggled, this,
          &BatchConverterDialog::onResizeRadioToggled);
  connect(ui->percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &BatchConverterDialog::onPercentChanged);
  connect(ui->width, qOverload<int>(&QSpinBox::valueChanged), this,
          &BatchConverterDialog::onWidthChanged);
  connect(ui->height, qOverload<int>(&QSpinBox::valueChanged), this,
          &BatchConverterDialog::onHeightChanged);
  connect(ui->resComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &BatchConverterDialog::onCommonResolutionChanged);
  connect(ui->fitDesktopButton, &QPushButton::clicked, this,
          &BatchConverterDialog::onFitDesktop);
  connect(ui->fillDesktopButton, &QPushButton::clicked, this,
          &BatchConverterDialog::onFillDesktop);
  connect(ui->resetButton, &QPushButton::clicked, this,
          &BatchConverterDialog::onResetSizes);
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, this,
          &BatchConverterDialog::onUseUpscaylToggled);

  // 10. Connect Dialog Action buttons and headers
  connect(ui->selectAllBtn, &QPushButton::clicked, this,
          &BatchConverterDialog::onSelectAll);
  connect(ui->deselectAllBtn, &QPushButton::clicked, this,
          &BatchConverterDialog::onDeselectAll);
  connect(ui->outDirBrowseBtn, &QPushButton::clicked, this,
          &BatchConverterDialog::onBrowseClicked);
  connect(ui->formatComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &BatchConverterDialog::onFormatChanged);
  connect(ui->convertButton, &QPushButton::clicked, this,
          &BatchConverterDialog::onConvertClicked);
  connect(ui->cancelButton, &QPushButton::clicked, this,
          &BatchConverterDialog::onCancelClicked);

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
  delete ui;
}

void BatchConverterDialog::onQualitySliderChanged(int value) {
  ui->qualitySpinBox->blockSignals(true);
  ui->qualitySpinBox->setValue(value);
  ui->qualitySpinBox->blockSignals(false);
}

void BatchConverterDialog::onQualitySpinBoxChanged(int value) {
  ui->qualitySlider->blockSignals(true);
  ui->qualitySlider->setValue(value);
  ui->qualitySlider->blockSignals(false);
}

// Sliders and spinbox connections

void BatchConverterDialog::onExposureSliderChanged(int value) {
  ui->exposureSpinBox->blockSignals(true);
  ui->exposureSpinBox->setValue(value / 100.0);
  ui->exposureSpinBox->blockSignals(false);
}
void BatchConverterDialog::onExposureSpinBoxChanged(double value) {
  ui->exposureSlider->blockSignals(true);
  ui->exposureSlider->setValue(static_cast<int>(value * 100));
  ui->exposureSlider->blockSignals(false);
}

void BatchConverterDialog::onContrastSliderChanged(int value) {
  ui->contrastSpinBox->blockSignals(true);
  ui->contrastSpinBox->setValue(value / 100.0);
  ui->contrastSpinBox->blockSignals(false);
}
void BatchConverterDialog::onContrastSpinBoxChanged(double value) {
  ui->contrastSlider->blockSignals(true);
  ui->contrastSlider->setValue(static_cast<int>(value * 100));
  ui->contrastSlider->blockSignals(false);
}

void BatchConverterDialog::onSaturationSliderChanged(int value) {
  ui->saturationSpinBox->blockSignals(true);
  ui->saturationSpinBox->setValue(value / 100.0);
  ui->saturationSpinBox->blockSignals(false);
}
void BatchConverterDialog::onSaturationSpinBoxChanged(double value) {
  ui->saturationSlider->blockSignals(true);
  ui->saturationSlider->setValue(static_cast<int>(value * 100));
  ui->saturationSlider->blockSignals(false);
}

void BatchConverterDialog::onTempSliderChanged(int value) {
  ui->tempSpinBox->blockSignals(true);
  ui->tempSpinBox->setValue(value / 100.0);
  ui->tempSpinBox->blockSignals(false);
}
void BatchConverterDialog::onTempSpinBoxChanged(double value) {
  ui->tempSlider->blockSignals(true);
  ui->tempSlider->setValue(static_cast<int>(value * 100));
  ui->tempSlider->blockSignals(false);
}

void BatchConverterDialog::onTintSliderChanged(int value) {
  ui->tintSpinBox->blockSignals(true);
  ui->tintSpinBox->setValue(value / 100.0);
  ui->tintSpinBox->blockSignals(false);
}
void BatchConverterDialog::onTintSpinBoxChanged(double value) {
  ui->tintSlider->blockSignals(true);
  ui->tintSlider->setValue(static_cast<int>(value * 100));
  ui->tintSlider->blockSignals(false);
}

void BatchConverterDialog::onBrightnessSliderChanged(int value) {
  ui->brightnessSpinBox->blockSignals(true);
  ui->brightnessSpinBox->setValue(value / 100.0);
  ui->brightnessSpinBox->blockSignals(false);
}
void BatchConverterDialog::onBrightnessSpinBoxChanged(double value) {
  ui->brightnessSlider->blockSignals(true);
  ui->brightnessSlider->setValue(static_cast<int>(value * 100));
  ui->brightnessSlider->blockSignals(false);
}

void BatchConverterDialog::onHueSliderChanged(int value) {
  ui->hueSpinBox->blockSignals(true);
  ui->hueSpinBox->setValue(static_cast<double>(value));
  ui->hueSpinBox->blockSignals(false);
}
void BatchConverterDialog::onHueSpinBoxChanged(double value) {
  ui->hueSlider->blockSignals(true);
  ui->hueSlider->setValue(static_cast<int>(value));
  ui->hueSlider->blockSignals(false);
}

// ----------------- Resize Dialog Sync Implementation -----------------

void BatchConverterDialog::onResizeRadioToggled() {
  bool isPercent = ui->byPercentage->isChecked();
  ui->percent->setEnabled(isPercent);
  ui->width->setEnabled(!isPercent);
  ui->height->setEnabled(!isPercent);
  ui->keepAspectRatio->setEnabled(!isPercent);

  if (isPercent) {
    ui->keepAspectRatio->blockSignals(true);
    ui->keepAspectRatio->setChecked(true);
    ui->keepAspectRatio->blockSignals(false);
    onPercentChanged(ui->percent->value());
  } else {
    onWidthChanged(ui->width->value());
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
  if (ui->keepAspectRatio->isChecked()) {
    targetSize.setHeight(static_cast<int>(originalSize.height() * factor));
  }
  updateToTargetValues();
}

void BatchConverterDialog::onHeightChanged(int val) {
  lastEdited = 1;
  float factor = static_cast<float>(val) / originalSize.height();
  targetSize.setHeight(val);
  if (ui->keepAspectRatio->isChecked()) {
    targetSize.setWidth(static_cast<int>(originalSize.width() * factor));
  }
  updateToTargetValues();
}

void BatchConverterDialog::updateToTargetValues() {
  ui->width->blockSignals(true);
  ui->height->blockSignals(true);
  ui->width->setValue(targetSize.width());
  ui->height->setValue(targetSize.height());
  ui->width->blockSignals(false);
  ui->height->blockSignals(false);
}

void BatchConverterDialog::onCommonResolutionChanged(int index) {
  QSize res;
  switch (index) {
  case 1:
    res = QSize(1366, 768);
    break;
  case 2:
    res = QSize(1440, 900);
    break;
  case 3:
    res = QSize(1440, 1050);
    break;
  case 4:
    res = QSize(1600, 1200);
    break;
  case 5:
    res = QSize(1920, 1080);
    break;
  case 6:
    res = QSize(1920, 1200);
    break;
  case 7:
    res = QSize(2560, 1080);
    break;
  case 8:
    res = QSize(2560, 1440);
    break;
  case 9:
    res = QSize(2560, 1600);
    break;
  case 10:
    res = QSize(3840, 1600);
    break;
  case 11:
    res = QSize(3840, 2160);
    break;
  default:
    res = originalSize;
    break;
  }
  if (ui->keepAspectRatio->isChecked())
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
  ui->resComboBox->blockSignals(true);
  ui->resComboBox->setCurrentIndex(0);
  ui->resComboBox->blockSignals(false);
  targetSize = originalSize;
  updateToTargetValues();
}

void BatchConverterDialog::onUseUpscaylToggled(bool checked) {
  ui->upscaylModelComboBox->setEnabled(checked);
}

// ----------------- Mass Check/Uncheck Toggles -----------------

void BatchConverterDialog::onSelectAll() {
  for (int i = 0; i < ui->fileListWidget->count(); ++i) {
    QListWidgetItem *item = ui->fileListWidget->item(i);
    BatchItemWidget *widget =
        qobject_cast<BatchItemWidget *>(ui->fileListWidget->itemWidget(item));
    if (widget) {
      widget->setChecked(true);
    }
  }
}

void BatchConverterDialog::onDeselectAll() {
  for (int i = 0; i < ui->fileListWidget->count(); ++i) {
    QListWidgetItem *item = ui->fileListWidget->item(i);
    BatchItemWidget *widget =
        qobject_cast<BatchItemWidget *>(ui->fileListWidget->itemWidget(item));
    if (widget) {
      widget->setChecked(false);
    }
  }
}

void BatchConverterDialog::onCheckedStateChanged() { updateSelectedCount(); }

void BatchConverterDialog::updateSelectedCount() {
  int checkedCount = 0;
  qint64 totalSizeBytes = 0;
  for (int i = 0; i < ui->fileListWidget->count(); ++i) {
    QListWidgetItem *item = ui->fileListWidget->item(i);
    BatchItemWidget *widget =
        qobject_cast<BatchItemWidget *>(ui->fileListWidget->itemWidget(item));
    if (widget && widget->isChecked()) {
      checkedCount++;
      totalSizeBytes += widget->fileSize();
    }
  }
  double totalSizeMB = totalSizeBytes / (1024.0 * 1024.0);
  ui->selectedCountLabel->setText(
      tr("%1 files selected (%2 MB)")
          .arg(checkedCount)
          .arg(QString::number(totalSizeMB, 'f', 1)));
}

void BatchConverterDialog::onBrowseClicked() {
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Select Output Directory"), ui->outDirEdit->text());
  if (!dir.isEmpty()) {
    ui->outDirEdit->setText(dir);
  }
}

void BatchConverterDialog::onFormatChanged(int index) {
  QString ext = ui->formatComboBox->itemData(index).toString();
  if (ext == "png") {
    ui->qualitySlider->setEnabled(true);
    ui->qualitySpinBox->setEnabled(true);

    ui->qualitySlider->blockSignals(true);
    ui->qualitySpinBox->blockSignals(true);

    ui->qualitySlider->setRange(0, 9);
    ui->qualitySpinBox->setRange(0, 9);
    ui->qualitySlider->setValue(settings->pngSaveQuality());
    ui->qualitySpinBox->setValue(settings->pngSaveQuality());

    ui->qualitySlider->blockSignals(false);
    ui->qualitySpinBox->blockSignals(false);

    QString tooltip = tr("PNG Compression level (0 - none, 9 - max)");
    ui->qualitySlider->setToolTip(tooltip);
    ui->qualitySpinBox->setToolTip(tooltip);
  } else if (ext == "jpg" || ext == "webp" || ext == "jxl" || ext == "avif") {
    ui->qualitySlider->setEnabled(true);
    ui->qualitySpinBox->setEnabled(true);

    ui->qualitySlider->blockSignals(true);
    ui->qualitySpinBox->blockSignals(true);

    ui->qualitySlider->setRange(1, 100);
    ui->qualitySpinBox->setRange(1, 100);

    int val = 90;
    if (ext == "jpg") {
      val = settings->JPEGSaveQuality();
    } else if (ext == "webp" || ext == "jxl" || ext == "avif") {
      val = settings->modernSaveQuality();
    }

    ui->qualitySlider->setValue(val);
    ui->qualitySpinBox->setValue(val);

    ui->qualitySlider->blockSignals(false);
    ui->qualitySpinBox->blockSignals(false);

    QString tooltip = tr("Quality (1 - lowest, 100 - highest)");
    ui->qualitySlider->setToolTip(tooltip);
    ui->qualitySpinBox->setToolTip(tooltip);
  } else {
    ui->qualitySlider->setEnabled(false);
    ui->qualitySpinBox->setEnabled(false);
    ui->qualitySlider->setToolTip("");
    ui->qualitySpinBox->setToolTip("");
  }
}

void BatchConverterDialog::updateUiState() {
  ui->scrollArea->setEnabled(!isConverting);
  ui->convertButton->setEnabled(!isConverting);
  ui->selectAllBtn->setEnabled(!isConverting);
  ui->deselectAllBtn->setEnabled(!isConverting);
  if (isConverting) {
    ui->cancelButton->setText(tr("Stop"));
  } else {
    ui->cancelButton->setText(tr("Cancel"));
  }
}

void BatchConverterDialog::onConvertClicked() {
  if (isConverting)
    return;

  QString outDir = ui->outDirEdit->text().trimmed();
  if (outDir.isEmpty() || !QDir(outDir).exists()) {
    QMessageBox::warning(this, tr("Invalid Directory"),
                         tr("Please select a valid output directory."));
    return;
  }

  int checkedCount = 0;
  for (int i = 0; i < ui->fileListWidget->count(); ++i) {
    QListWidgetItem *item = ui->fileListWidget->item(i);
    BatchItemWidget *widget =
        qobject_cast<BatchItemWidget *>(ui->fileListWidget->itemWidget(item));
    if (widget && widget->isChecked()) {
      checkedCount++;
    }
  }

  if (checkedCount == 0) {
    QMessageBox::warning(
        this, tr("No files"),
        tr("No files selected in the queue. Please check at least one file."));
    return;
  }

  isConverting = true;
  isCancelled = false;
  processedFiles = 0;
  successCount = 0;
  failedCount = 0;

  ui->progressBar->setMaximum(checkedCount);
  ui->progressBar->setValue(0);
  ui->statusLabel->setText(tr("Processing..."));

  updateUiState();
  startConversion();
}

void BatchConverterDialog::startConversion() {
  QString baseOutDir = ui->outDirEdit->text().trimmed();
  QString finalOutDir = baseOutDir;

  // Create subfolder for batch if selected
  if (ui->subfolderCheckBox->isChecked()) {
    QString subfolderName =
        "Batch_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QDir baseDir(baseOutDir);
    if (baseDir.mkdir(subfolderName)) {
      finalOutDir = baseOutDir + "/" + subfolderName;
    }
  }

  QString formatExt = ui->formatComboBox->currentData().toString();
  int quality = ui->qualitySlider->value();
  bool doResize = ui->resizeEnableCheckBox->isChecked();
  QSize resizeTarget = targetSize;
  bool keepAspect = ui->keepAspectRatio->isChecked();
  bool useUpscayl = doResize && ui->useUpscaylCheckBox->isChecked();
  QString upscaylModel = ui->upscaylModelComboBox->currentText();

#ifdef USE_UPSCAYL
  // Save current Upscayl preferences to settings
  settings->setResizeUseUpscayl(ui->useUpscaylCheckBox->isChecked());
  settings->setUpscaylModel(upscaylModel);
  settings->sync();
#endif
  int filter = ui->filterComboBox->currentData().toInt();

  bool doColor = ui->colorEnableCheckBox->isChecked();
  float exposure = doColor ? ui->exposureSpinBox->value() : 0.0f;
  float contrast = doColor ? ui->contrastSpinBox->value() : 1.0f;
  float saturation = doColor ? ui->saturationSpinBox->value() : 1.0f;
  float temp = doColor ? ui->tempSpinBox->value() : 0.0f;
  float tint = doColor ? ui->tintSpinBox->value() : 0.0f;
  float brightness = doColor ? ui->brightnessSpinBox->value() : 0.0f;
  float hue = doColor ? ui->hueSpinBox->value() : 0.0f;

  QString pattern = ui->patternEdit->text();
  bool overwrite = ui->overwriteCheckBox->isChecked();

  if (useUpscayl && doResize) {
    threadPool.setMaxThreadCount(1); // GPU VRAM Sequential protection
  } else {
    threadPool.setMaxThreadCount(
        QThread::idealThreadCount()); // CPU Multi-threading
  }

#ifdef USE_UPSCAYL
  if (useUpscayl && doResize) {
    ui->statusLabel->setText(tr("Loading AI Model..."));
    qApp->processEvents();

    sharedResrgan = new RealESRGAN(-1, false);
    sharedResrgan->scale = 4;
    sharedResrgan->prepadding = 10;
    sharedResrgan->tilesize = sharedResrgan->autoTilesize();

    QString appDir = QCoreApplication::applicationDirPath();
    QString paramQStr = appDir + "/models/" + upscaylModel + ".param";
    QString binQStr = appDir + "/models/" + upscaylModel + ".bin";

    int loadRes =
        sharedResrgan->load(paramQStr.toStdWString(), binQStr.toStdWString());
    if (loadRes != 0) {
      delete sharedResrgan;
      sharedResrgan = nullptr;
      ui->statusLabel->setText(tr("Failed to load AI model."));
      QMessageBox::warning(
          this, tr("AI Error"),
          tr("Failed to load AI upscaling model: %1").arg(upscaylModel));
      return;
    }
  }
#endif

  int activeIndex = 0;
  for (int i = 0; i < ui->fileListWidget->count(); ++i) {
    QListWidgetItem *item = ui->fileListWidget->item(i);
    BatchItemWidget *widget =
        qobject_cast<BatchItemWidget *>(ui->fileListWidget->itemWidget(item));
    if (!widget || !widget->isChecked()) {
      continue;
    }

    widget->setStatus(tr("Pending"), "", true);

    QString srcPath = widget->filePath();
    QFileInfo srcFi(srcPath);

    // Format name based on pattern
    QString targetName = pattern;
    targetName.replace("{name}", srcFi.baseName());
    targetName.replace("{ext}", formatExt);
    targetName.replace("{date}", QDate::currentDate().toString("yyyy-MM-dd"));
    targetName.replace("{index}", QString::number(activeIndex + 1));

    if (!targetName.contains(".")) {
      targetName += "." + formatExt;
    }

    QString destPath = finalOutDir + "/" + targetName;

    // Skip if already exists and no overwrite
    if (!overwrite && QFileInfo::exists(destPath)) {
      processedFiles++;
      ui->progressBar->setValue(processedFiles);
      widget->setStatus(tr("Done"), tr("Skipped (Exists)"), true);
      continue;
    }

    widget->setStatus(tr("Processing..."), "", true);
    BatchWorkerTask *task = new BatchWorkerTask(
        this, i, srcPath, destPath, formatExt, quality, doResize, resizeTarget,
        keepAspect, useUpscayl, upscaylModel, filter, brightness, contrast,
        saturation, temp, tint, exposure, hue);
    threadPool.start(task);
    activeIndex++;
  }

  if (processedFiles >= ui->progressBar->maximum()) {
    finalizeConversion();
  }
}

void BatchConverterDialog::onProgressUpdated(int index, QString status,
                                             QString details, bool success) {
  QMutexLocker locker(&listMutex);
  QListWidgetItem *item = ui->fileListWidget->item(index);
  BatchItemWidget *widget =
      qobject_cast<BatchItemWidget *>(ui->fileListWidget->itemWidget(item));
  if (widget) {
    widget->setStatus(status, details, success);
  }

  processedFiles++;
  if (success) {
    successCount++;
  } else {
    failedCount++;
  }

  ui->progressBar->setValue(processedFiles);
  ui->statusLabel->setText(tr("Processed %1 / %2 files.")
                               .arg(processedFiles)
                               .arg(ui->progressBar->maximum()));

  if (processedFiles >= ui->progressBar->maximum() || isCancelled) {
    finalizeConversion();
  }
}

void BatchConverterDialog::finalizeConversion() {
  isConverting = false;
  cleanupSharedUpscayl();
  updateUiState();
  ui->statusLabel->setText(tr("Finished. Success: %1, Failed: %2")
                               .arg(successCount)
                               .arg(failedCount));

  QMessageBox::information(this, tr("Batch Conversion Complete"),
                           tr("Batch process complete.\n\nSuccessfully "
                              "converted: %1\nFailed: %2\nTotal files: %3")
                               .arg(successCount)
                               .arg(failedCount)
                               .arg(processedFiles));
}

void BatchConverterDialog::onCancelClicked() {
  if (isConverting) {
    isCancelled = true;
    threadPool.clear();
    threadPool.waitForDone();
    cleanupSharedUpscayl();
    isConverting = false;
    updateUiState();
    ui->statusLabel->setText(tr("Stopped by user."));
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

// ========== Enable/Disable blocks based on checkboxes ==========

void BatchConverterDialog::collectResizeWidgets()
{
    const QList<QWidget*> children = ui->resizeContainer->findChildren<QWidget*>();
    for (QWidget *w : children) {
        if (w != ui->resizeEnableCheckBox) {
            m_resizeWidgets.append(w);
        }
    }
}

void BatchConverterDialog::collectColorWidgets()
{
    const QList<QWidget*> children = ui->colorContainer->findChildren<QWidget*>();
    for (QWidget *w : children) {
        if (w != ui->colorEnableCheckBox) {
            m_colorWidgets.append(w);
        }
    }
}

void BatchConverterDialog::setResizeWidgetsEnabled(bool enabled)
{
    for (QWidget *w : m_resizeWidgets) {
        if (w) w->setEnabled(enabled);
    }
}

void BatchConverterDialog::setColorWidgetsEnabled(bool enabled)
{
    for (QWidget *w : m_colorWidgets) {
        if (w) w->setEnabled(enabled);
    }
}

void BatchConverterDialog::onResizeEnabledChanged(bool enabled)
{
    setResizeWidgetsEnabled(enabled);
}

void BatchConverterDialog::onColorEnabledChanged(bool enabled)
{
    setColorWidgetsEnabled(enabled);
}

// ==================== BatchWorkerTask Implementation ====================

void BatchWorkerTask::run() {
  QImage srcImg(srcPath);
  if (srcImg.isNull()) {
    QMetaObject::invokeMethod(
        dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
        Q_ARG(QString,
              QCoreApplication::translate("BatchConverterDialog", "Failed")),
        Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog",
                                                   "Load Error")),
        Q_ARG(bool, false));
    return;
  }

  QImage processedImg = srcImg;

  // 1. Color adjustments
  bool colorModified =
      (brightness != 0.0f || std::abs(contrast - 1.0f) > 0.001f ||
       std::abs(saturation - 1.0f) > 0.001f || temp != 0.0f || tint != 0.0f ||
       exposure != 0.0f || hue != 0.0f);
  if (colorModified) {
    std::shared_ptr<const QImage> srcPtr =
        std::make_shared<const QImage>(processedImg);
    QImage *adj = ImageLib::applyColorAdjustments(
        srcPtr, brightness, contrast, saturation, hue, exposure, temp, tint);
    if (adj) {
      processedImg = *adj;
      delete adj;
    }
  }

  // 2. AI Upscayl / Resizing
  if (useUpscayl) {
#ifdef USE_UPSCAYL
    if (dialog->sharedResrgan) {
      QImage imgRgba = processedImg.convertToFormat(QImage::Format_ARGB32);
      int inW = imgRgba.width(), inH = imgRgba.height();
      int outW = inW * 4, outH = inH * 4;
      QImage outImg(outW, outH, QImage::Format_ARGB32);
      if (dialog->sharedResrgan->processPixels(
              imgRgba.constBits(), inW, inH, outImg.bits(), outW, outH) == 0) {
        processedImg = outImg;
      }
    }
#endif
    if (doResize && processedImg.size() != targetSize) {
      QSize finalSize = targetSize;
      if (keepAspectRatio) {
        finalSize = processedImg.size().scaled(targetSize, Qt::KeepAspectRatio);
      }
      std::shared_ptr<const QImage> upscaledPtr =
          std::make_shared<const QImage>(processedImg);
      QImage *finalImg = ImageLib::scaled(
          upscaledPtr, finalSize, static_cast<ScalingFilter>(scalingFilter));
      if (finalImg) {
        processedImg = *finalImg;
        delete finalImg;
      }
    }
  } else if (doResize) {
    QSize finalSize = targetSize;
    if (keepAspectRatio) {
      finalSize = processedImg.size().scaled(targetSize, Qt::KeepAspectRatio);
    }
    std::shared_ptr<const QImage> imgPtr =
        std::make_shared<const QImage>(processedImg);
    QImage *scaledImg = ImageLib::scaled(
        imgPtr, finalSize, static_cast<ScalingFilter>(scalingFilter));
    if (scaledImg) {
      processedImg = *scaledImg;
      delete scaledImg;
    }
  }

  // 3. Save
  QByteArray formatBa = format.toLatin1();
  bool saved = processedImg.save(destPath, formatBa.constData(), quality);

  // Get resolution of processed image for status details
  QString detailsStr = QString("%1 • %2x%3")
                           .arg(format.toUpper())
                           .arg(processedImg.width())
                           .arg(processedImg.height());

  if (saved) {
    QMetaObject::invokeMethod(
        dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
        Q_ARG(QString,
              QCoreApplication::translate("BatchConverterDialog", "Done")),
        Q_ARG(QString, detailsStr), Q_ARG(bool, true));
  } else {
    QMetaObject::invokeMethod(
        dialog, "onProgressUpdated", Qt::QueuedConnection, Q_ARG(int, index),
        Q_ARG(QString,
              QCoreApplication::translate("BatchConverterDialog", "Failed")),
        Q_ARG(QString, QCoreApplication::translate("BatchConverterDialog",
                                                   "Save Error")),
        Q_ARG(bool, false));
  }
}