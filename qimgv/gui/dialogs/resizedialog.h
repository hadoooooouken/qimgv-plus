#pragma once

#include <QCheckBox>
#include <QDebug>
#include <QScreen>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QComboBox>

#include "settings.h"

class QRadioButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

class ResizeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ResizeDialog(QSize initialSize, QWidget *parent = nullptr);
    ~ResizeDialog();
    QSize newSize();

public slots:
    int exec();

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    void setupUi();

    QRadioButton *byPercentage = nullptr;
    QDoubleSpinBox *percent = nullptr;
    QRadioButton *byAbsoluteSize = nullptr;
    QSpinBox *width = nullptr;
    QSpinBox *height = nullptr;
    QCheckBox *keepAspectRatio = nullptr;
    QComboBox *comboBox = nullptr;
    QLabel *label_4 = nullptr;

    QCheckBox *useUpscaylCheckBox = nullptr;
    QComboBox *upscaylModelComboBox = nullptr;
    QLabel *upscaylModelLabel = nullptr;

    QComboBox *resComboBox = nullptr;
    QPushButton *fitDesktopButton = nullptr;
    QPushButton *fillDesktopButton = nullptr;
    QPushButton *resetButton = nullptr;
    QPushButton *okButton = nullptr;
    QPushButton *cancelButton = nullptr;

    QSize originalSize, targetSize, desktopSize;
    void updateToTargetValues();
    int lastEdited; // 0 - width, 1 - height
    void resetResCheckBox();

private slots:
    void widthChanged(int);
    void heightChanged(int);
    void percentChanged(double);
    void sizeSelect();

    void setCommonResolution(int);
    void reset();
    void fitDesktop();
    void fillDesktop();
    void onAspectRatioCheckbox();
    void onPercentageRadioButton();
    void onAbsoluteSizeRadioButton();
signals:
    void sizeSelected(QSize, ScalingFilter, bool, QString);
};
