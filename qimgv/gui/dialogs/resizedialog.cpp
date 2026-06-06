#include "resizedialog.h"
#include "ui_resizedialog.h"
#include <QDir>
#include <QFileInfo>

ResizeDialog::ResizeDialog(QSize originalSize, QWidget *parent)
    : QDialog(parent), ui(new Ui::ResizeDialog), lastEdited(0) {
  ui->setupUi(this);
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

  ui->percent->setFocus();

  this->originalSize = originalSize;
  targetSize = originalSize;

  ui->width->setValue(originalSize.width());
  ui->height->setValue(originalSize.height());

  ui->resetButton->setText(tr("Reset:") + " " +
                           QString::number(originalSize.width()) + " x " +
                           QString::number(originalSize.height()));

  desktopSize = qApp->primaryScreen()->size();
  connect(ui->byPercentage, &QRadioButton::toggled, this,
          &ResizeDialog::onPercentageRadioButton);
  connect(ui->byAbsoluteSize, &QRadioButton::toggled, this,
          &ResizeDialog::onAbsoluteSizeRadioButton);
  connect(ui->percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &ResizeDialog::percentChanged);
  connect(ui->width, qOverload<int>(&QSpinBox::valueChanged), this,
          &ResizeDialog::widthChanged);
  connect(ui->height, qOverload<int>(&QSpinBox::valueChanged), this,
          &ResizeDialog::heightChanged);
  connect(ui->keepAspectRatio, &QCheckBox::toggled, this,
          &ResizeDialog::onAspectRatioCheckbox);
  connect(ui->resComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
          this, &ResizeDialog::setCommonResolution);
  connect(ui->fitDesktopButton, &QPushButton::pressed, this,
          &ResizeDialog::fitDesktop);
  connect(ui->fillDesktopButton, &QPushButton::pressed, this,
          &ResizeDialog::fillDesktop);
  connect(ui->resetButton, &QPushButton::pressed, this, &ResizeDialog::reset);
  connect(ui->cancelButton, &QPushButton::pressed, this, &ResizeDialog::reject);
  connect(ui->okButton, &QPushButton::pressed, this, &ResizeDialog::sizeSelect);

  // Enable and populate the filter dropdown
  ui->label_4->setEnabled(true);
  ui->comboBox->setEnabled(true);
  ui->comboBox->clear();
  ui->comboBox->addItem(tr("Nearest"), QI_FILTER_NEAREST);
  ui->comboBox->addItem(tr("Bilinear"), QI_FILTER_BILINEAR);

  ui->comboBox->addItem(tr("Smart sharpen"), QI_FILTER_SMART);

  ScalingFilter currentFilter = settings->scalingFilter();
  int idx = ui->comboBox->findData(currentFilter);
  if (idx != -1) {
    ui->comboBox->setCurrentIndex(idx);
  } else {
    ui->comboBox->setCurrentIndex(1); // default to Bilinear
  }

#ifdef USE_UPSCAYL
  // Initialize Upscayl controls mapped to UI elements
  useUpscaylCheckBox = ui->useUpscaylCheckBox;
  upscaylModelComboBox = ui->upscaylModelComboBox;
  upscaylModelLabel = ui->labelModel;

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
  if (modelNames.isEmpty()) {
    modelNames.append("4xLSDIRCompactC3");
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
#else
  // Hide Upscayl elements if not compiled with Upscayl support
  ui->useUpscaylCheckBox->hide();
  ui->labelModel->hide();
  ui->upscaylModelComboBox->hide();
#endif
}

ResizeDialog::~ResizeDialog() { delete ui; }

void ResizeDialog::sizeSelect() {
  if (targetSize != originalSize) {
    ScalingFilter selectedFilter =
        static_cast<ScalingFilter>(ui->comboBox->currentData().toInt());
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

QSize ResizeDialog::newSize() { return targetSize; }

void ResizeDialog::widthChanged(int newWidth) {
  lastEdited = 0;
  float factor = static_cast<float>(newWidth) / originalSize.width();
  targetSize.setWidth(newWidth);
  if (ui->keepAspectRatio->isChecked()) {
    targetSize.setHeight(static_cast<int>(originalSize.height() * factor));
  }
  updateToTargetValues();
}

void ResizeDialog::heightChanged(int newHeight) {
  lastEdited = 1;
  float factor = static_cast<float>(newHeight) / originalSize.height();
  targetSize.setHeight(newHeight);
  if (ui->keepAspectRatio->isChecked()) {
    targetSize.setWidth(static_cast<int>(originalSize.width() * factor));
  }
  updateToTargetValues();
}

void ResizeDialog::updateToTargetValues() {
  ui->width->blockSignals(true);
  ui->height->blockSignals(true);
  ui->width->setValue(targetSize.width());
  ui->height->setValue(targetSize.height());
  ui->width->blockSignals(false);
  ui->height->blockSignals(false);
}

void ResizeDialog::fitDesktop() {
  targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatio);
  updateToTargetValues();
}

void ResizeDialog::fillDesktop() {
  targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatioByExpanding);
  updateToTargetValues();
}

void ResizeDialog::onAspectRatioCheckbox() {
  resetResCheckBox();
  (lastEdited) ? heightChanged(ui->height->value())
               : widthChanged(ui->width->value());
}

void ResizeDialog::onAbsoluteSizeRadioButton() {
  ui->width->blockSignals(true);
  ui->height->blockSignals(true);
  ui->percent->blockSignals(true);
  ui->keepAspectRatio->blockSignals(true);

  ui->width->setEnabled(true);
  ui->height->setEnabled(true);
  ui->percent->setEnabled(false);
  ui->keepAspectRatio->setEnabled(true);

  ui->width->blockSignals(false);
  ui->height->blockSignals(false);
  ui->percent->blockSignals(false);
  ui->keepAspectRatio->blockSignals(false);
}

void ResizeDialog::onPercentageRadioButton() {
  ui->width->blockSignals(true);
  ui->height->blockSignals(true);
  ui->percent->blockSignals(true);
  ui->keepAspectRatio->blockSignals(true);

  ui->width->setEnabled(false);
  ui->height->setEnabled(false);
  ui->percent->setEnabled(true);
  ui->keepAspectRatio->setChecked(true);
  ui->keepAspectRatio->setEnabled(false);
  percentChanged(ui->percent->value());

  ui->width->blockSignals(false);
  ui->height->blockSignals(false);
  ui->percent->blockSignals(false);
  ui->keepAspectRatio->blockSignals(false);
}

void ResizeDialog::resetResCheckBox() {
  ui->resComboBox->blockSignals(true);
  ui->resComboBox->setCurrentIndex(0);
  ui->resComboBox->blockSignals(false);
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
