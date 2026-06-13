#pragma once

#include <atomic>
#include "components/thumbnailer/thumbnailer.h"
#include "settings.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/imagelib.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMutex>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRunnable>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QThreadPool>
#include <QVBoxLayout>
#include <QBoxLayout>

#ifdef USE_UPSCAYL
class RealESRGAN;
#endif

class BatchItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit BatchItemWidget(const QString &filePath, QWidget *parent = nullptr);

    void setStatus(const QString &statusText, const QString &details = "", bool success = true);
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

class LinkedSliderSpin : public QWidget {
    Q_OBJECT
public:
    LinkedSliderSpin(const QString &labelText, double minVal, double maxVal, double defaultVal,
                     double factor = 1.0, int decimals = 0, const QString &suffix = "",
                     QWidget *parent = nullptr);

    double value() const;
    void setValue(double val);

signals:
    void valueChanged(double val);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateSpinBox(int val);
    void updateSlider(double val);

private:
    QLabel *label;
    QSlider *slider;
    QDoubleSpinBox *spinBox;
    double m_factor;
    double m_defaultValue;
};

class BatchConverterDialog : public QDialog {
    Q_OBJECT

public:
    explicit BatchConverterDialog(const QList<QString> &filePaths, QWidget *parent = nullptr);
    ~BatchConverterDialog();

public slots:
    void onProgressUpdated(int index, QString status, QString details, bool success);

private slots:
    void onQualitySliderChanged(int value);
    void onQualitySpinBoxChanged(int value);

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

    void onResizeEnabledChanged(bool enabled);
    void onColorEnabledChanged(bool enabled);

private:
    void setupUi();
    void setupLeftPanel(QBoxLayout *mainLayout);
    void setupRightPanel(QBoxLayout *mainLayout);
    void setupFormatSection(QVBoxLayout *scrollLayout);
    void setupResizeSection(QVBoxLayout *scrollLayout);
    void setupColorSection(QVBoxLayout *scrollLayout);
    void setupRenameSection(QVBoxLayout *scrollLayout);
    void setupBottomPanel(QVBoxLayout *mainLayout);

    QStringList inputPaths;
    QThreadPool threadPool;
    Thumbnailer *thumbnailer;

    // UI Pointers
    QPushButton *selectAllBtn;
    QPushButton *deselectAllBtn;
    QLabel *selectedCountLabel;
    QListWidget *fileListWidget;
    QScrollArea *scrollArea;

    QComboBox *formatComboBox;
    QSlider *qualitySlider;
    QSpinBox *qualitySpinBox;

    QWidget *resizeContainer;
    QCheckBox *resizeEnableCheckBox;
    QRadioButton *byPercentage;
    QRadioButton *byAbsoluteSize;
    QDoubleSpinBox *percent;
    QSpinBox *width;
    QSpinBox *height;
    QCheckBox *keepAspectRatio;
    QCheckBox *useUpscaylCheckBox;
    QComboBox *filterComboBox;
    QComboBox *upscaylModelComboBox;
    QComboBox *resComboBox;
    QPushButton *fitDesktopButton;
    QPushButton *fillDesktopButton;
    QPushButton *resetButton;

    QWidget *colorContainer;
    QCheckBox *colorEnableCheckBox;
    LinkedSliderSpin *exposureWidget;
    LinkedSliderSpin *contrastWidget;
    LinkedSliderSpin *brightnessWidget;
    LinkedSliderSpin *saturationWidget;
    LinkedSliderSpin *hueWidget;
    LinkedSliderSpin *tempWidget;
    LinkedSliderSpin *tintWidget;

    QWidget *outputContainer;
    QLineEdit *outDirEdit;
    QPushButton *outDirBrowseBtn;
    QCheckBox *subfolderCheckBox;
    QLineEdit *patternEdit;
    QCheckBox *overwriteCheckBox;

    QProgressBar *progressBar;
    QLabel *statusLabel;
    QPushButton *convertButton;
    QPushButton *cancelButton;

    int totalFiles = 0;
    int processedFiles = 0;
    int successCount = 0;
    int failedCount = 0;
    std::atomic<bool> isConverting{false};
    std::atomic<bool> isCancelled{false};

    QSize originalSize, targetSize, desktopSize;
    int lastEdited = 0;

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
                    QString upscaylModel, int scalingFilter,
                    float exposure, float contrast, float brightness,
                    float temp, float tint, float saturation, float hue)
        : dialog(dialog), index(index), srcPath(srcPath), destPath(destPath),
          format(format), quality(quality), doResize(doResize),
          targetSize(targetSize), keepAspectRatio(keepAspectRatio),
          useUpscayl(useUpscayl), upscaylModel(upscaylModel),
          scalingFilter(scalingFilter), exposure(exposure), contrast(contrast),
          brightness(brightness), temp(temp), tint(tint),
          saturation(saturation), hue(hue) {}

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
    float exposure;
    float contrast;
    float brightness;
    float temp;
    float tint;
    float saturation;
    float hue;
};