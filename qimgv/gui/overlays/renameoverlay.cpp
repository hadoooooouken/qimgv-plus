#include "renameoverlay.h"
#include "gui/customwidgets/iconwidget.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/customwidgets/pushbuttonfocusind.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>

RenameOverlay::RenameOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent)
{
    setupUi();
    connect(findChild<QPushButton*>("cancelButton"), &QPushButton::clicked, this, &RenameOverlay::onCancel);
    connect(closeButton,  &IconButton::clicked,  this, &RenameOverlay::hide);
    connect(okButton,     &QPushButton::clicked, this, &RenameOverlay::rename);
    okButton->setHighlighted(true);
    closeButton->setIconPath(":/res/icons/common/overlay/close-dim16.png");
    headerIcon->setIconPath(":/res/icons/common/overlay/edit16.png");
    setPosition(FloatingWidgetPosition::CENTER);
    setAcceptKeyboardFocus(true);

    keyFilter.append(actionManager->shortcutsForAction("exit"));
    keyFilter.append(actionManager->shortcutsForAction("renameFile"));

    hide();
    if(parent)
        setContainerSize(parent->size());
}

RenameOverlay::~RenameOverlay() = default;

void RenameOverlay::setupUi() {
    this->setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *outerVLayout = new QVBoxLayout(this);
    outerVLayout->setSpacing(0);
    outerVLayout->setContentsMargins(0, 0, 0, 0);
    outerVLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    // --- centering HBox: [spacer] [center column] [spacer] ---
    QHBoxLayout *centeringHLayout = new QHBoxLayout();
    centeringHLayout->setSpacing(0);
    centeringHLayout->setContentsMargins(0, 0, 0, 0);

    centeringHLayout->addSpacerItem(new QSpacerItem(0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // --- center column VBox: [spacer] [overlay widget] [spacer] ---
    QVBoxLayout *centerVLayout = new QVBoxLayout();
    centerVLayout->setSpacing(0);
    centerVLayout->setContentsMargins(0, 0, 0, 0);

    centerVLayout->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // --- overlay widget ---
    QWidget *overlay = new QWidget(this);
    overlay->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    overlay->setAccessibleName("RenameOverlayWidget");

    QVBoxLayout *overlayLayout = new QVBoxLayout(overlay);
    overlayLayout->setSpacing(10);
    overlayLayout->setContentsMargins(0, 0, 0, 0);

    // --- header ---
    QWidget *headerWidget = new QWidget(overlay);
    headerWidget->setAccessibleName("OverlayHeaderWidget");

    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setSpacing(0);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    headerIcon = new IconWidget(headerWidget);
    headerIcon->setAccessibleName("OverlayHeaderIcon");
    headerLayout->addWidget(headerIcon);

    QLabel *headerLabel = new QLabel(tr("Rename"), headerWidget);
    QSizePolicy labelPolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    labelPolicy.setHorizontalStretch(1);
    headerLabel->setSizePolicy(labelPolicy);
    headerLabel->setAccessibleName("OverlayHeaderLabel");
    headerLayout->addWidget(headerLabel);

    closeButton = new IconButton(headerWidget);
    closeButton->setFocusPolicy(Qt::NoFocus);
    closeButton->setAccessibleName("OverlayHeaderButton");
    headerLayout->addWidget(closeButton);

    overlayLayout->addWidget(headerWidget);

    // --- content area (fileName + buttons) ---
    QVBoxLayout *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(6);
    contentLayout->setContentsMargins(9, 0, 9, 9);

    fileName = new QLineEdit(overlay);
    fileName->setMinimumWidth(330);
    contentLayout->addWidget(fileName);

    // --- buttons row ---
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(0);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSizeConstraint(QLayout::SetMinimumSize);

    buttonsLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    okButton = new PushButtonFocusInd(overlay);
    okButton->setFocusPolicy(Qt::NoFocus);
    okButton->setAccessibleName("ButtonSetLeft");
    okButton->setText(tr("Rename"));
    buttonsLayout->addWidget(okButton);

    QPushButton *cancelButton = new QPushButton(tr("Cancel"), overlay);
    cancelButton->setObjectName("cancelButton");
    cancelButton->setFocusPolicy(Qt::NoFocus);
    cancelButton->setAccessibleName("ButtonSetRight");
    buttonsLayout->addWidget(cancelButton);

    contentLayout->addLayout(buttonsLayout);
    overlayLayout->addLayout(contentLayout);

    centerVLayout->addWidget(overlay);

    centerVLayout->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    centeringHLayout->addLayout(centerVLayout);

    centeringHLayout->addSpacerItem(new QSpacerItem(0, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // Set stretch factors: 1, 0, 1
    centeringHLayout->setStretch(0, 1);
    centeringHLayout->setStretch(1, 0);
    centeringHLayout->setStretch(2, 1);

    outerVLayout->addLayout(centeringHLayout);
}

void RenameOverlay::show() {
    selectName();
    OverlayWidget::show();
    QTimer::singleShot(0, fileName, SLOT(setFocus()));
}

void RenameOverlay::hide() {
    OverlayWidget::hide();
}

void RenameOverlay::setName(QString name) {
    fileName->setText(name);
    origName = name;
    selectName();
}

void RenameOverlay::setBackdropEnabled(bool mode) {
    if(backdrop == mode)
        return;
    backdrop = mode;
    recalculateGeometry();
}

void RenameOverlay::recalculateGeometry() {
    if(!backdrop)
        OverlayWidget::recalculateGeometry();
    else // expand
        setGeometry(0, 0, containerSize().width(), containerSize().height());
}

void RenameOverlay::selectName() {
    int end = fileName->text().lastIndexOf(".");
    if(end < 0)
        end = fileName->text().size();
    fileName->setSelection(0, end);
}

void RenameOverlay::rename() {
    if(fileName->text().isEmpty())
        return;
    hide();
    emit renameRequested(fileName->text());
}

void RenameOverlay::onCancel() {
    hide();
    fileName->setText(origName);
}

void RenameOverlay::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        event->accept();
        onCancel();
    } else if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        rename();
    } else {
        auto shortcut = ShortcutBuilder::fromEvent(event);
        if(!shortcut.isEmpty() && keyFilter.contains(shortcut))
            event->ignore();
        else
            event->accept();
    }
}

void RenameOverlay::mousePressEvent(QMouseEvent *event) {
    event->accept();
    if(qApp->widgetAt(mapToGlobal(event->pos())) == this)
        this->onCancel();
}
