#pragma once

#include "components/thumbnailer/thumbnailer.h"
#include "settings.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/imagelib.h"
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QMutex>
#include <QRunnable>
#include <QStringList>
#include <QThreadPool>

namespace Ui {
class BatchConverterDialog;
}

#ifdef USE_UPSCAYL
class RealESRGAN;
#endif

class BatchItemWidget : public QWidget {
  Q_OBJECT
public:
  explicit BatchItemWidget(const QString &filePath, QWidget *parent = nullptr);

  void setStatus(const QString &statusText, const QString &details = "",
                 bool success = true);
  void setThumbnail(std::shared_ptr<Thumbnail> thumb);
  bool isChecked() const { return checkBox->isChecked(); }
  void setChecked(bool checked) { checkBox->setChecked(checked); }
  QString filePath() const { return path; }
  qint64 fileSize() const { return size; }
  QSize imageSize() const { return imgSize; }

signals:
  void checkedStateChanged();

private:
  QString path;
  qint64 size = 0;
  QSize imgSize;

  QCheckBox *checkBox;
  QLabel *thumbLabel;
  QLabel *nameLabel;
  QLabel *srcInfoLabel;
  QLabel *statusLabel;
  QLabel *destInfoLabel;
};

class BatchConverterDialog : public QDialog {
  Q_OBJECT

public:
  explicit BatchConverterDialog(const QList<QString> &filePaths,
                                QWidget *parent = nullptr);
  ~BatchConverterDialog();

public slots:
  void onProgressUpdated(int index, QString status, QString details,
                         bool success);

private slots:
  void onQualitySliderChanged(int value);
  void onQualitySpinBoxChanged(int value);

  // Sliders & SpinBoxes sync slots
  void onExposureSliderChanged(int value);
  void onExposureSpinBoxChanged(double value);
  void onContrastSliderChanged(int value);
  void onContrastSpinBoxChanged(double value);
  void onSaturationSliderChanged(int value);
  void onSaturationSpinBoxChanged(double value);
  void onTempSliderChanged(int value);
  void onTempSpinBoxChanged(double value);
  void onTintSliderChanged(int value);
  void onTintSpinBoxChanged(double value);
  void onBrightnessSliderChanged(int value);
  void onBrightnessSpinBoxChanged(double value);
  void onHueSliderChanged(int value);
  void onHueSpinBoxChanged(double value);

  // Resize sync slots
  void onPercentChanged(double val);
  void onWidthChanged(int val);
  void onHeightChanged(int val);
  void onCommonResolutionChanged(int index);
  void onFitDesktop();
  void onFillDesktop();
  void onResetSizes();
  void onResizeRadioToggled();

  void onSelectAll();
  void onDeselectAll();
  void onCheckedStateChanged();
  void onBrowseClicked();
  void onFormatChanged(int index);
  void onUseUpscaylToggled(bool checked);
  void onConvertClicked();
  void onCancelClicked();

  // Enable/disable blocks when checkboxes are toggled
  void onResizeEnabledChanged(bool enabled);
  void onColorEnabledChanged(bool enabled);

private:
  Ui::BatchConverterDialog *ui;
  QStringList inputPaths;
  QThreadPool threadPool;
  QMutex listMutex;
  Thumbnailer *thumbnailer;

  int totalFiles = 0;
  int processedFiles = 0;
  int successCount = 0;
  int failedCount = 0;
  bool isConverting = false;
  bool isCancelled = false;

  QSize originalSize, targetSize, desktopSize;
  int lastEdited = 0; // 0 for width, 1 for height

  QList<QWidget*> m_resizeWidgets;
  QList<QWidget*> m_colorWidgets;

  void updateUiState();
  void startConversion();
  void finalizeConversion();
  void updateSelectedCount();
  void updateToTargetValues();
  void cleanupSharedUpscayl();

  void collectResizeWidgets();
  void collectColorWidgets();
  void setResizeWidgetsEnabled(bool enabled);
  void setColorWidgetsEnabled(bool enabled);

#ifdef USE_UPSCAYL
  RealESRGAN *sharedResrgan = nullptr;
#endif

  friend class BatchWorkerTask;
};

class BatchWorkerTask : public QRunnable {
public:
  BatchWorkerTask(BatchConverterDialog *dialog, int index,
                  const QString &srcPath, const QString &destPath,
                  const QString &format, int quality, bool doResize,
                  QSize targetSize, bool keepAspectRatio, bool useUpscayl,
                  QString upscaylModel, int scalingFilter, float brightness,
                  float contrast, float saturation, float temp, float tint,
                  float exposure, float hue)
      : dialog(dialog), index(index), srcPath(srcPath), destPath(destPath),
        format(format), quality(quality), doResize(doResize),
        targetSize(targetSize), keepAspectRatio(keepAspectRatio),
        useUpscayl(useUpscayl), upscaylModel(upscaylModel),
        scalingFilter(scalingFilter), brightness(brightness),
        contrast(contrast), saturation(saturation), temp(temp), tint(tint),
        exposure(exposure), hue(hue) {}

  void run() override;

private:
  BatchConverterDialog *dialog;
  int index;
  QString srcPath;
  QString destPath;
  QString format;
  int quality;
  bool doResize;
  QSize targetSize;
  bool keepAspectRatio;
  bool useUpscayl;
  QString upscaylModel;
  int scalingFilter;
  float brightness;
  float contrast;
  float saturation;
  float temp;
  float tint;
  float exposure;
  float hue;
};