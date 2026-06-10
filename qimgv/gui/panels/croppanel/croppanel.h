#pragma once

#include <QWidget>
#include <QScreen>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QPainter>
#include <QtGlobal>
#include "gui/customwidgets/sidepanelwidget.h"
#include "gui/customwidgets/iconwidget.h"
#include "gui/customwidgets/spinboxinputfix.h"
#include "gui/overlays/cropoverlay.h"
#include <QTimer>
#include <QDebug>

class QGroupBox;
class QLabel;
class QDoubleSpinBox;
class QPushButton;
class StyledComboBox;
class PushButtonFocusInd;

class CropPanel : public SidePanelWidget
{
    Q_OBJECT

public:
    explicit CropPanel(CropOverlay *_overlay, QWidget *parent = nullptr);
    ~CropPanel();
    void setImageRealSize(QSize);

public slots:
    void onSelectionOutsideChange(QRect rect);
    void show();

signals:
    void crop(QRect);
    void cropAndSave(QRect);
    void cancel();
    void cropClicked();
    void selectionChanged(QRect);
    void selectAll();
    void aspectRatioChanged(QPointF);

protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *event);
    void wheelEvent(QWheelEvent *event);

private slots:
    void doCrop();
    void doCropSave();
    void onSelectionChange();
    void onAspectRatioChange(); // via manual input
    void onAspectRatioSelected(); // via ComboBox
    void onSwapARClicked();
    void setFocusCropBtn();
    void setFocusCropSaveBtn();

    void doCropDefaultAction();
    void doReset();

private:
    void setupUi();

    IconWidget *headerIcon = nullptr;
    
    SpinBoxInputFix *width = nullptr;
    SpinBoxInputFix *height = nullptr;
    SpinBoxInputFix *posX = nullptr;
    SpinBoxInputFix *posY = nullptr;

    StyledComboBox *ARcomboBox = nullptr;
    QDoubleSpinBox *ARX = nullptr;
    QDoubleSpinBox *ARY = nullptr;
    QPushButton *swapARButton = nullptr;

    PushButtonFocusInd *cropButton = nullptr;
    PushButtonFocusInd *cropSaveButton = nullptr;
    QPushButton *resetButton = nullptr;
    QPushButton *cancelButton = nullptr;

    QRect cropRect;
    CropOverlay *overlay;
    QSize realSize;
};
