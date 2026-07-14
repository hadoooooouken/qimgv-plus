#include "resizedialog.h"
#include "settings.h"
#include <QDir>
#include <QFileInfo>

ResizeDialog::ResizeDialog(QSize originalSize, QWidget *parent)
    : QDialog(parent), lastEdited(0) {
  setupUi();
  setWindowModality(Qt::ApplicationModal);

  // Style dialog with active theme colors
  auto colors = settings->colorScheme();
  QString dialogStyle =
      QString("QDialog {"
              "  background-color: %1;"
              "  color: %2;"
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
              "}")
          .arg(colors.widget.name())          // %1
          .arg(colors.text.name())            // %2
          .arg(colors.text_hc.name())         // %3
          .arg(colors.widget_border.name())   // %4
          .arg(colors.button.name())          // %5
          .arg(colors.accent.name())          // %6
          .arg(colors.button_hover.name())    // %7
          .arg(colors.text_hc2.name())        // %8
          .arg(colors.button_pressed.name()); // %9

  setStyleSheet(dialogStyle);

  percent->setFocus();

  this->originalSize = originalSize;
  targetSize = originalSize;

  width->setValue(originalSize.width());
  height->setValue(originalSize.height());

  resetButton->setText(tr("Reset:") + " " +
                       QString::number(originalSize.width()) + " x " +
                       QString::number(originalSize.height()));

  desktopSize = qApp->primaryScreen()->size();
  connect(byPercentage, &QRadioButton::toggled, this,
          &ResizeDialog::onPercentageRadioButton);
  connect(byAbsoluteSize, &QRadioButton::toggled, this,
          &ResizeDialog::onAbsoluteSizeRadioButton);
  connect(percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &ResizeDialog::percentChanged);
  connect(width, qOverload<int>(&QSpinBox::valueChanged), this,
          &ResizeDialog::widthChanged);
  connect(height, qOverload<int>(&QSpinBox::valueChanged), this,
          &ResizeDialog::heightChanged);
  connect(keepAspectRatio, &QCheckBox::toggled, this,
          &ResizeDialog::onAspectRatioCheckbox);
  connect(resComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &ResizeDialog::setCommonResolution);
  connect(fitDesktopButton, &QPushButton::pressed, this,
          &ResizeDialog::fitDesktop);
  connect(fillDesktopButton, &QPushButton::pressed, this,
          &ResizeDialog::fillDesktop);
  connect(resetButton, &QPushButton::pressed, this, &ResizeDialog::reset);
  connect(cancelButton, &QPushButton::pressed, this, &ResizeDialog::reject);
  connect(okButton, &QPushButton::pressed, this, &ResizeDialog::sizeSelect);

  // Enable and populate the filter dropdown
  label_4->setEnabled(true);
  comboBox->setEnabled(true);
  comboBox->clear();
  comboBox->addItem(tr("Nearest"), QI_FILTER_NEAREST);
  comboBox->addItem(tr("Bilinear"), QI_FILTER_BILINEAR);
  comboBox->addItem(tr("Smart sharpen"), QI_FILTER_SMART);
  comboBox->addItem(tr("Magic Kernel Sharp 2021"), QI_FILTER_MKS2021);

  int idx = comboBox->findData(QI_FILTER_SMART);
  if (idx != -1) {
    comboBox->setCurrentIndex(idx);
  } else {
    comboBox->setCurrentIndex(2); // default to Smart sharpen
  }

#ifdef USE_UPSCAYL
  if (useUpscaylCheckBox) {
    if (settings->hasUpscaylModels()) {
      // Auto-scan models directory for compatible models
      QDir modelsDir(qApp->applicationDirPath() + "/models");
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
      upscaylModelComboBox->addItems(modelNames);

      // Pre-select the model from settings
      int modelIdx = upscaylModelComboBox->findText(settings->upscaylModel());
      if (modelIdx != -1) {
        upscaylModelComboBox->setCurrentIndex(modelIdx);
      } else {
        upscaylModelComboBox->setCurrentIndex(0);
      }

      connect(useUpscaylCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        upscaylModelLabel->setEnabled(checked);
        upscaylModelComboBox->setEnabled(checked);
      });

      useUpscaylCheckBox->setChecked(settings->resizeUseUpscayl());
      upscaylModelLabel->setEnabled(useUpscaylCheckBox->isChecked());
      upscaylModelComboBox->setEnabled(useUpscaylCheckBox->isChecked());
    } else {
      useUpscaylCheckBox->setChecked(false);
      useUpscaylCheckBox->setEnabled(false);
      useUpscaylCheckBox->setToolTip(tr("No AI models found in models/ directory."));
      upscaylModelLabel->setEnabled(false);
      upscaylModelComboBox->setEnabled(false);
    }
  }
#endif
}

ResizeDialog::~ResizeDialog() = default;

void ResizeDialog::setupUi() {
  setWindowTitle(tr("Resize"));
  setFixedSize(540, 420);

  QVBoxLayout *verticalLayout = new QVBoxLayout(this);
  verticalLayout->setSpacing(6);

  QHBoxLayout *mainHorizontalLayout = new QHBoxLayout();
  mainHorizontalLayout->setSpacing(6);

  QVBoxLayout *leftColumn = new QVBoxLayout();

  byPercentage = new QRadioButton(tr("By Percent:"), this);
  byPercentage->setChecked(true);
  leftColumn->addWidget(byPercentage);

  QGridLayout *percentGrid = new QGridLayout();
  QLabel *label_3 = new QLabel(tr("Percent:"), this);
  label_3->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  percentGrid->addWidget(label_3, 0, 0);

  percent = new QDoubleSpinBox(this);
  percent->setAlignment(Qt::AlignCenter);
  percent->setButtonSymbols(QAbstractSpinBox::NoButtons);
  percent->setMinimum(1.0);
  percent->setMaximum(1600.0);
  percent->setValue(100.0);
  percentGrid->addWidget(percent, 0, 1);
  leftColumn->addLayout(percentGrid);

  byAbsoluteSize = new QRadioButton(tr("By Absolute Size:"), this);
  leftColumn->addWidget(byAbsoluteSize);

  QGridLayout *sizeGrid = new QGridLayout();
  QLabel *label = new QLabel(tr("Width:"), this);
  label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  sizeGrid->addWidget(label, 0, 0);

  width = new QSpinBox(this);
  width->setAlignment(Qt::AlignCenter);
  width->setButtonSymbols(QAbstractSpinBox::NoButtons);
  width->setMinimum(1);
  width->setMaximum(65535);
  sizeGrid->addWidget(width, 0, 1);

  QLabel *label_2 = new QLabel(tr("Height:"), this);
  label_2->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  sizeGrid->addWidget(label_2, 1, 0);

  height = new QSpinBox(this);
  height->setAlignment(Qt::AlignCenter);
  height->setButtonSymbols(QAbstractSpinBox::NoButtons);
  height->setMinimum(1);
  height->setMaximum(65535);
  sizeGrid->addWidget(height, 1, 1);
  leftColumn->addLayout(sizeGrid);

  keepAspectRatio = new QCheckBox(tr("Keep aspect ratio"), this);
  keepAspectRatio->setChecked(true);
  leftColumn->addWidget(keepAspectRatio);

  QGridLayout *filterGrid = new QGridLayout();
  label_4 = new QLabel(tr("Filter:"), this);
  label_4->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  filterGrid->addWidget(label_4, 0, 0);

  comboBox = new QComboBox(this);
  filterGrid->addWidget(comboBox, 0, 1);
  leftColumn->addLayout(filterGrid);

#ifdef USE_UPSCAYL
  useUpscaylCheckBox = new QCheckBox(tr("Use Upscayl"), this);
  leftColumn->addWidget(useUpscaylCheckBox);

  QGridLayout *modelGrid = new QGridLayout();
  upscaylModelLabel = new QLabel(tr("Model:"), this);
  upscaylModelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  modelGrid->addWidget(upscaylModelLabel, 0, 0);

  upscaylModelComboBox = new QComboBox(this);
  modelGrid->addWidget(upscaylModelComboBox, 0, 1);
  leftColumn->addLayout(modelGrid);
#endif

  mainHorizontalLayout->addLayout(leftColumn);

  QVBoxLayout *rightColumn = new QVBoxLayout();
  rightColumn->setContentsMargins(0, 0, 0, 0);

  QLabel *commonSizesLabel = new QLabel(tr("Common sizes:"), this);
  rightColumn->addWidget(commonSizesLabel);

  resComboBox = new QComboBox(this);
  resComboBox->addItem(tr("Select:"), QVariant());
  resComboBox->addItem("1280 x 720", QSize(1280, 720));
  resComboBox->addItem("1366 x 768", QSize(1366, 768));
  resComboBox->addItem("1440 x 900", QSize(1440, 900));
  resComboBox->addItem("1440 x 1050", QSize(1440, 1050));
  resComboBox->addItem("1600 x 1200", QSize(1600, 1200));
  resComboBox->addItem("1920 x 1080 (FullHD)", QSize(1920, 1080));
  resComboBox->addItem("1920 x 1200 (FullHD)", QSize(1920, 1200));
  resComboBox->addItem("2560 x 1080", QSize(2560, 1080));
  resComboBox->addItem("2560 x 1440", QSize(2560, 1440));
  resComboBox->addItem("2560 x 1600", QSize(2560, 1600));
  resComboBox->addItem("3840 x 1600 (UW 4K)", QSize(3840, 1600));
  resComboBox->addItem("3840 x 2160 (UHD-1)", QSize(3840, 2160));
  rightColumn->addWidget(resComboBox);

  fitDesktopButton = new QPushButton(tr("Fit to desktop"), this);
  rightColumn->addWidget(fitDesktopButton);

  fillDesktopButton = new QPushButton(tr("Fill desktop (expanding)"), this);
  rightColumn->addWidget(fillDesktopButton);

  resetButton = new QPushButton(tr("Reset"), this);
  rightColumn->addWidget(resetButton);

  rightColumn->addStretch(1);

  mainHorizontalLayout->addLayout(rightColumn);
  verticalLayout->addLayout(mainHorizontalLayout);

  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  buttonsLayout->addStretch(1);

  okButton = new QPushButton(tr("OK"), this);
  buttonsLayout->addWidget(okButton);

  cancelButton = new QPushButton(tr("Cancel"), this);
  buttonsLayout->addWidget(cancelButton);

  verticalLayout->addLayout(buttonsLayout);
}

void ResizeDialog::sizeSelect() {
  if (targetSize != originalSize) {
    ScalingFilter selectedFilter =
        static_cast<ScalingFilter>(comboBox->currentData().toInt());
    bool useUpscayl = false;
    QString upscaylModel = "";
#ifdef USE_UPSCAYL
    if (useUpscaylCheckBox) {
      useUpscayl = useUpscaylCheckBox->isChecked();
      upscaylModel = upscaylModelComboBox->currentText();
      settings->setResizeUseUpscayl(useUpscayl);
      settings->setUpscaylModel(upscaylModel);
      settings->sync();
    }
#endif
    emit sizeSelected(targetSize, selectedFilter, useUpscayl, upscaylModel);
  }
  this->accept();
}

void ResizeDialog::setCommonResolution(int index) {
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

QSize ResizeDialog::newSize() { return targetSize; }

void ResizeDialog::widthChanged(int newWidth) {
  lastEdited = 0;
  float factor = static_cast<float>(newWidth) / originalSize.width();
  targetSize.setWidth(newWidth);
  if (keepAspectRatio->isChecked()) {
    targetSize.setHeight(static_cast<int>(originalSize.height() * factor));
  }
  updateToTargetValues();
}

void ResizeDialog::heightChanged(int newHeight) {
  lastEdited = 1;
  float factor = static_cast<float>(newHeight) / originalSize.height();
  targetSize.setHeight(newHeight);
  if (keepAspectRatio->isChecked()) {
    targetSize.setWidth(static_cast<int>(originalSize.width() * factor));
  }
  updateToTargetValues();
}

void ResizeDialog::updateToTargetValues() {
  width->blockSignals(true);
  height->blockSignals(true);
  width->setValue(targetSize.width());
  height->setValue(targetSize.height());
  width->blockSignals(false);
  height->blockSignals(false);
}

void ResizeDialog::fitDesktop() {
  byAbsoluteSize->setChecked(true);
  targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatio);
  updateToTargetValues();
}

void ResizeDialog::fillDesktop() {
  byAbsoluteSize->setChecked(true);
  targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatioByExpanding);
  updateToTargetValues();
}

void ResizeDialog::onAspectRatioCheckbox() {
  resetResCheckBox();
  (lastEdited) ? heightChanged(height->value())
               : widthChanged(width->value());
}

void ResizeDialog::onAbsoluteSizeRadioButton() {
  width->blockSignals(true);
  height->blockSignals(true);
  percent->blockSignals(true);
  keepAspectRatio->blockSignals(true);

  width->setEnabled(true);
  height->setEnabled(true);
  percent->setEnabled(false);
  keepAspectRatio->setEnabled(true);

  width->blockSignals(false);
  height->blockSignals(false);
  percent->blockSignals(false);
  keepAspectRatio->blockSignals(false);
}

void ResizeDialog::onPercentageRadioButton() {
  width->blockSignals(true);
  height->blockSignals(true);
  percent->blockSignals(true);
  keepAspectRatio->blockSignals(true);

  width->setEnabled(false);
  height->setEnabled(false);
  percent->setEnabled(true);
  keepAspectRatio->setChecked(true);
  keepAspectRatio->setEnabled(false);
  percentChanged(percent->value());

  width->blockSignals(false);
  height->blockSignals(false);
  percent->blockSignals(false);
  keepAspectRatio->blockSignals(false);
}

void ResizeDialog::resetResCheckBox() {
  resComboBox->blockSignals(true);
  resComboBox->setCurrentIndex(0);
  resComboBox->blockSignals(false);
}

void ResizeDialog::percentChanged(double newPercent) {
  double scale = newPercent / 100.;
  targetSize.setWidth(originalSize.width() * scale);
  targetSize.setHeight(originalSize.height() * scale);

  updateToTargetValues();
}

void ResizeDialog::keyPressEvent(QKeyEvent *event) {
  if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)) {
    sizeSelect();
  } else if (event->key() == Qt::Key_Escape) {
    reject();
  } else {
    event->ignore();
  }
}

void ResizeDialog::reset() {
  resetResCheckBox();
  targetSize = originalSize;
  updateToTargetValues();
}

int ResizeDialog::exec() {
  resize(sizeHint());
  return QDialog::exec();
}
