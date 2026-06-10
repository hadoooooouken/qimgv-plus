#include "croppanel.h"
#include "gui/customwidgets/styledcombobox.h"
#include "gui/customwidgets/pushbuttonfocusind.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpacerItem>

CropPanel::CropPanel(CropOverlay *_overlay, QWidget *parent) :
    SidePanelWidget(parent),
    overlay(_overlay)
{
    setupUi();
    setFocusPolicy(Qt::NoFocus);

    ARcomboBox->setItemDelegate(new QStyledItemDelegate(ARcomboBox));
    ARcomboBox->view()->setTextElideMode(Qt::ElideNone);

    headerIcon->setIconPath(":/res/icons/common/other/image-crop48.png");

    ARcomboBox->setIconPath(":res/icons/common/other/dropDownArrow.png");

    hide();

    if(settings->defaultCropAction() == ACTION_CROP)
        setFocusCropBtn();
    else
        setFocusCropSaveBtn();

    connect(cropButton, &PushButtonFocusInd::rightPressed, this, &CropPanel::setFocusCropBtn);
    connect(cropSaveButton, &PushButtonFocusInd::rightPressed, this, &CropPanel::setFocusCropSaveBtn);

    connect(cancelButton, SIGNAL(clicked()), this, SIGNAL(cancel()));
    connect(cropButton, SIGNAL(clicked()), this, SLOT(doCrop()));
    connect(cropSaveButton, SIGNAL(clicked()), this, SLOT(doCropSave()));
    connect(width, SIGNAL(valueChanged(int)), this, SLOT(onSelectionChange()));
    connect(height, SIGNAL(valueChanged(int)), this, SLOT(onSelectionChange()));
    connect(posX, SIGNAL(valueChanged(int)), this, SLOT(onSelectionChange()));
    connect(posY, SIGNAL(valueChanged(int)), this, SLOT(onSelectionChange()));
    connect(ARX, SIGNAL(valueChanged(double)), this, SLOT(onAspectRatioChange()));
    connect(ARY, SIGNAL(valueChanged(double)), this, SLOT(onAspectRatioChange()));
    connect(ARcomboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onAspectRatioSelected()));
    connect(swapARButton, &QPushButton::clicked, this, &CropPanel::onSwapARClicked);

    connect(overlay, SIGNAL(selectionChanged(QRect)),
            this, SLOT(onSelectionOutsideChange(QRect)));
    connect(this, SIGNAL(selectionChanged(QRect)),
            overlay, SLOT(onSelectionOutsideChange(QRect)));
    connect(this, SIGNAL(aspectRatioChanged(QPointF)),
            overlay, SLOT(setAspectRatio(QPointF)));
    connect(overlay, SIGNAL(escPressed()), this, SIGNAL(cancel()));
    connect(overlay, SIGNAL(cropDefault()), this, SLOT(doCropDefaultAction()));
    connect(overlay, SIGNAL(cropSave()), this, SLOT(doCropSave()));
    connect(this, SIGNAL(selectAll()), overlay, SLOT(selectAll()));
    connect(resetButton, SIGNAL(clicked()), this, SLOT(doReset()));
}

CropPanel::~CropPanel() = default;

void CropPanel::setupUi() {
    this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
    this->setMinimumSize(250, 300);

    QVBoxLayout *verticalLayout_2 = new QVBoxLayout(this);
    verticalLayout_2->setSpacing(8);
    verticalLayout_2->setContentsMargins(18, 12, 18, 0);
    verticalLayout_2->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // Header Icon
    QHBoxLayout *horizontalLayout_6 = new QHBoxLayout();
    horizontalLayout_6->setContentsMargins(0, 0, 0, 4);
    
    headerIcon = new IconWidget(this);
    headerIcon->setMinimumSize(48, 48);
    horizontalLayout_6->addWidget(headerIcon);
    horizontalLayout_6->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    verticalLayout_2->addLayout(horizontalLayout_6);

    // Selection GroupBox
    QGroupBox *groupBox = new QGroupBox(this);
    QVBoxLayout *verticalLayout = new QVBoxLayout(groupBox);
    verticalLayout->setSpacing(8);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    QLabel *label_6 = new QLabel(tr("Selection"), groupBox);
    label_6->setAlignment(Qt::AlignCenter);
    verticalLayout->addWidget(label_6);

    auto createSpinBoxLayout = [this, groupBox](const QString& labelText, SpinBoxInputFix*& spinBox) -> QHBoxLayout* {
        QHBoxLayout *layout = new QHBoxLayout();
        layout->setSpacing(10);
        layout->setContentsMargins(0, 0, 0, 0);

        QLabel *label = new QLabel(labelText, groupBox);
        label->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
        layout->addWidget(label);

        spinBox = new SpinBoxInputFix(groupBox);
        spinBox->setAlignment(Qt::AlignCenter);
        spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spinBox->setKeyboardTracking(false);
        spinBox->setMaximum(65535);
        layout->addWidget(spinBox);

        return layout;
    };

    verticalLayout->addLayout(createSpinBoxLayout(tr("Width"), width));
    verticalLayout->addLayout(createSpinBoxLayout(tr("Height"), height));

    QWidget *line = new QWidget(groupBox);
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    line->setMinimumSize(0, 1);
    line->setMaximumSize(16777215, 1);
    line->setAccessibleName("HLineSeparator");
    verticalLayout->addWidget(line);

    verticalLayout->addLayout(createSpinBoxLayout(tr("Pos_X"), posX));
    verticalLayout->addLayout(createSpinBoxLayout(tr("Pos_Y"), posY));

    verticalLayout_2->addWidget(groupBox);

    // Aspect Ratio GroupBox
    QGroupBox *groupBox_3 = new QGroupBox(this);
    QVBoxLayout *verticalLayout_4 = new QVBoxLayout(groupBox_3);
    verticalLayout_4->setSpacing(8);
    verticalLayout_4->setContentsMargins(0, 0, 0, 0);
    verticalLayout_4->setSizeConstraint(QLayout::SetMinAndMaxSize);

    QLabel *label_5 = new QLabel(tr("Aspect Ratio"), groupBox_3);
    label_5->setAlignment(Qt::AlignCenter);
    verticalLayout_4->addWidget(label_5);

    ARcomboBox = new StyledComboBox(groupBox_3);
    ARcomboBox->setContextMenuPolicy(Qt::NoContextMenu);
    ARcomboBox->addItems({tr("Free"), tr("Custom"), tr("Current Image"), tr("This Screen"), 
                          tr("1:1"), tr("4:3"), tr("16:9"), tr("16:10")});
    verticalLayout_4->addWidget(ARcomboBox);

    QWidget *ARInputWidget = new QWidget(groupBox_3);
    QHBoxLayout *horizontalLayout_7 = new QHBoxLayout(ARInputWidget);
    horizontalLayout_7->setSpacing(6);
    horizontalLayout_7->setContentsMargins(0, 0, 0, 0);

    ARX = new QDoubleSpinBox(ARInputWidget);
    ARX->setAlignment(Qt::AlignCenter);
    ARX->setButtonSymbols(QAbstractSpinBox::NoButtons);
    ARX->setKeyboardTracking(false);
    ARX->setDecimals(4);
    ARX->setMinimum(0.0001);
    ARX->setValue(1.0);
    horizontalLayout_7->addWidget(ARX);

    swapARButton = new QPushButton(tr("⇄"), ARInputWidget);
    swapARButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    swapARButton->setFocusPolicy(Qt::NoFocus);
    swapARButton->setAccessibleName("SwapButton");
    swapARButton->setToolTip(tr("Swap aspect ratio"));
    horizontalLayout_7->addWidget(swapARButton);

    ARY = new QDoubleSpinBox(ARInputWidget);
    ARY->setAlignment(Qt::AlignCenter);
    ARY->setButtonSymbols(QAbstractSpinBox::NoButtons);
    ARY->setKeyboardTracking(false);
    ARY->setDecimals(4);
    ARY->setMinimum(0.0001);
    ARY->setValue(1.0);
    horizontalLayout_7->addWidget(ARY);

    verticalLayout_4->addWidget(ARInputWidget);
    verticalLayout_2->addWidget(groupBox_3);

    verticalLayout_2->addSpacerItem(new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Fixed));

    // Crop Buttons
    QHBoxLayout *horizontalLayout = new QHBoxLayout();
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    horizontalLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    cropButton = new PushButtonFocusInd(this);
    cropButton->setFocusPolicy(Qt::NoFocus);
    cropButton->setAccessibleName("Button");
    cropButton->setText(tr("Crop"));
    horizontalLayout->addWidget(cropButton);

    cropSaveButton = new PushButtonFocusInd(this);
    cropSaveButton->setFocusPolicy(Qt::NoFocus);
    cropSaveButton->setAccessibleName("Button");
    cropSaveButton->setText(tr("Crop && Save"));
    horizontalLayout->addWidget(cropSaveButton);

    verticalLayout_2->addLayout(horizontalLayout);

    resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setFocusPolicy(Qt::NoFocus);
    resetButton->setAccessibleName("Button");
    verticalLayout_2->addWidget(resetButton);

    cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setFocusPolicy(Qt::NoFocus);
    cancelButton->setAccessibleName("Button");
    verticalLayout_2->addWidget(cancelButton);

    verticalLayout_2->addSpacerItem(new QSpacerItem(250, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Minimum));
    verticalLayout_2->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // Fix tab stops
    setTabOrder(width, height);
    setTabOrder(height, posX);
    setTabOrder(posX, posY);
    setTabOrder(posY, ARcomboBox);
    setTabOrder(ARcomboBox, ARX);
    setTabOrder(ARX, ARY);
}

void CropPanel::setImageRealSize(QSize sz) {
    width->setMaximum(sz.width());
    height->setMaximum(sz.height());
    realSize = sz;
    // reset to free mode on image change
    ARcomboBox->setCurrentIndex(0);
    // update aspect ratio in input fields

    onAspectRatioSelected();
}

void CropPanel::doCropDefaultAction() {
    if(settings->defaultCropAction() == ACTION_CROP)
        doCrop();
    else
        doCropSave();
}

void CropPanel::doCrop() {
    QRect target(posX->value(), posY->value(),
                 width->value(), height->value());
    if(target.width() > 0 && target.height() > 0 && target.size() != realSize)
        emit crop(target);
    else
        emit cancel();
}

void CropPanel::doCropSave() {
    QRect target(posX->value(), posY->value(),
                 width->value(), height->value());
    if(target.width() > 0 && target.height() > 0 && target.size() != realSize)
        emit cropAndSave(target);
    else
        emit cancel();
}

// on user input
void CropPanel::onSelectionChange() {
    emit selectionChanged(QRect(posX->value(),
                                posY->value(),
                                width->value(),
                                height->value()));
}

void CropPanel::onAspectRatioChange() {
    ARcomboBox->setCurrentIndex(1); // "Custom"
    if(ARX->value() && ARY->value())
        emit aspectRatioChanged(QPointF(ARX->value(), ARY->value()));
}

// 0 == free
// 1 == custom (from input fields)
// 2 == current image
// 3 == current screen
// 4 ...
void CropPanel::onAspectRatioSelected() {
    QPointF newAR(1, 1);

    int index = ARcomboBox->currentIndex();
    switch(index) {
    case 0:
    {
        overlay->setLockAspectRatio(false);
        if(realSize.height() != 0)
            newAR = QPointF(qreal(realSize.width()) / realSize.height(), 1.0);
        break;
    }
    case 1:
    {
        newAR = QPointF(ARX->value(), ARY->value());
        break;
    }
    case 2:
    {
        newAR = QPointF(qreal(realSize.width()) / realSize.height(), 1.0);
        break;
    }
    case 3:
    {
        QScreen* screen = nullptr;
#if QT_VERSION >= 0x050A00
        screen = QGuiApplication::screenAt(mapToGlobal(ARcomboBox->geometry().topLeft()));
        if(!screen)
            screen = QGuiApplication::primaryScreen();
#else
        screen = QGuiApplication::primaryScreen();
#endif
        newAR = QPointF(qreal(screen->geometry().width()) / screen->geometry().height(), 1.0);
        break;
    }
    case 4:
    {
        newAR = QPointF(1.0, 1.0);
        break;
    }
    case 5:
    {
        newAR = QPointF(4.0, 3.0);
        break;
    }
    case 6:
    {
        newAR = QPointF(16.0, 9.0);
        break;
    }
    case 7:
    {
        newAR = QPointF(16.0, 10.0);
        break;
    }
    default: // apply aspect ratio; update input fields
    {
        break;
    }
    }

    ARX->blockSignals(true);
    ARY->blockSignals(true);
    ARX->setValue(newAR.x());
    ARY->setValue(newAR.y());
    ARX->blockSignals(false);
    ARY->blockSignals(false);
    if(index)
        overlay->setAspectRatio(newAR);
}

void CropPanel::setFocusCropBtn() {
    settings->setDefaultCropAction(ACTION_CROP);
    cropSaveButton->setHighlighted(false);
    cropButton->setHighlighted(true);
}

void CropPanel::setFocusCropSaveBtn() {
    settings->setDefaultCropAction(ACTION_CROP_SAVE);
    cropSaveButton->setHighlighted(true);
    cropButton->setHighlighted(false);
}

// update input box values
void CropPanel::onSelectionOutsideChange(QRect rect) {
    width->blockSignals(true);
    height->blockSignals(true);
    posX->blockSignals(true);
    posY->blockSignals(true);

    width->setValue(rect.width());
    height->setValue(rect.height());
    posX->setValue(rect.left());
    posY->setValue(rect.top());

    width->blockSignals(false);
    height->blockSignals(false);
    posX->blockSignals(false);
    posY->blockSignals(false);
}

void CropPanel::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void CropPanel::show() {
    QWidget::show();
    // stackoverflow sorcery
    QTimer::singleShot(0,width,SLOT(setFocus()));
}

void CropPanel::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        if(event->modifiers() == Qt::ShiftModifier)
            doCropSave();
        else
            doCropDefaultAction();
    } else if(event->key() == Qt::Key_Escape) {
        emit cancel();
    } else if(event->matches(QKeySequence::SelectAll)) {
        emit selectAll();
    } else {
        event->ignore();
    }
}

void CropPanel::onSwapARClicked() {
    double x = ARX->value();
    double y = ARY->value();
    ARX->blockSignals(true);
    ARY->blockSignals(true);
    ARX->setValue(y);
    ARY->setValue(x);
    ARX->blockSignals(false);
    ARY->blockSignals(false);

    ARcomboBox->setCurrentIndex(1); // "Custom"
    emit aspectRatioChanged(QPointF(y, x));
}

void CropPanel::doReset() {
    ARcomboBox->setCurrentIndex(0);
    overlay->fitSelectionToAspectRatio();
}

void CropPanel::wheelEvent(QWheelEvent *event) {
    event->accept();
}
