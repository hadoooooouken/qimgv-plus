#include "contextmenu.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/customwidgets/contextmenuitem.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>

ContextMenu::ContextMenu(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    qApp->installEventFilter(this);
    hide();
}

ContextMenu::~ContextMenu()
{
    qApp->removeEventFilter(this);
}

void ContextMenu::setupUi()
{
    setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoMousePropagation, true);

    setMinimumWidth(212);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 4, 0, 4);

    m_stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackedWidget);

    // -------------------- Main page --------------------
    QWidget *mainPage = new QWidget();
    QVBoxLayout *mainPageLayout = new QVBoxLayout(mainPage);
    mainPageLayout->setSpacing(7);
    mainPageLayout->setContentsMargins(0, 0, 0, 0);

    // --- Zoom buttons row ---
    QHBoxLayout *zoomLayout = new QHBoxLayout();
    zoomLayout->setSpacing(0);
    zoomLayout->setContentsMargins(4, 0, 4, 0);

    m_fitWindow = new ActionButton();
    m_fitWindow->setAccessibleName("ContextMenuButton");
    m_fitWindow->setAction("fitWindow");
    m_fitWindow->setIconPath(":/res/icons/common/buttons/contextmenu/fit-window18.png");
    m_fitWindow->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_fitWindow);

    m_fitWidth = new ActionButton();
    m_fitWidth->setAccessibleName("ContextMenuButton");
    m_fitWidth->setAction("fitWidth");
    m_fitWidth->setIconPath(":/res/icons/common/buttons/contextmenu/fit-width18.png");
    m_fitWidth->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_fitWidth);

    m_fitWindowStretch = new ActionButton();
    m_fitWindowStretch->setAccessibleName("ContextMenuButton");
    m_fitWindowStretch->setAction("fitWindowStretch");
    m_fitWindowStretch->setIconPath(":/res/icons/common/buttons/contextmenu/fit-height-stretch18.png");
    m_fitWindowStretch->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_fitWindowStretch);

    m_zoomOriginal = new ActionButton();
    m_zoomOriginal->setAccessibleName("ContextMenuButton");
    m_zoomOriginal->setAction("fitNormal");
    m_zoomOriginal->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-original18.png");
    m_zoomOriginal->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_zoomOriginal);

    m_zoomIn = new ActionButton();
    m_zoomIn->setAccessibleName("ContextMenuButton");
    m_zoomIn->setAction("zoomIn");
    m_zoomIn->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    m_zoomIn->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_zoomIn);

    m_zoomOut = new ActionButton();
    m_zoomOut->setAccessibleName("ContextMenuButton");
    m_zoomOut->setAction("zoomOut");
    m_zoomOut->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    m_zoomOut->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_zoomOut);

    mainPageLayout->addLayout(zoomLayout);

    // --- Separator line between zoom and transform buttons ---
    QWidget *editLine = new QWidget();
    editLine->setFixedHeight(1);
    editLine->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    editLine->setAccessibleName("HLineSeparator");
    QVBoxLayout *lineEditLayout = new QVBoxLayout();
    lineEditLayout->setContentsMargins(11, 0, 11, 0);
    lineEditLayout->addWidget(editLine);
    mainPageLayout->addLayout(lineEditLayout);

    // --- Transform buttons row ---
    QHBoxLayout *transformLayout = new QHBoxLayout();
    transformLayout->setSpacing(0);
    transformLayout->setContentsMargins(4, 0, 4, 0);

    m_rotateLeft = new ActionButton();
    m_rotateLeft->setAccessibleName("ContextMenuButton");
    m_rotateLeft->setAction("rotateLeft");
    m_rotateLeft->setIconPath(":/res/icons/common/menuitem/rotate-left16.png");
    m_rotateLeft->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_rotateLeft);

    m_rotateRight = new ActionButton();
    m_rotateRight->setAccessibleName("ContextMenuButton");
    m_rotateRight->setAction("rotateRight");
    m_rotateRight->setIconPath(":/res/icons/common/menuitem/rotate-right16.png");
    m_rotateRight->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_rotateRight);

    m_flipV = new ActionButton();
    m_flipV->setAccessibleName("ContextMenuButton");
    m_flipV->setAction("flipV");
    m_flipV->setIconPath(":/res/icons/common/menuitem/flip-v16.png");
    m_flipV->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_flipV);

    m_flipH = new ActionButton();
    m_flipH->setAccessibleName("ContextMenuButton");
    m_flipH->setAction("flipH");
    m_flipH->setIconPath(":/res/icons/common/menuitem/flip-h16.png");
    m_flipH->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_flipH);

    m_crop = new ActionButton();
    m_crop->setAccessibleName("ContextMenuButton");
    m_crop->setAction("crop");
    m_crop->setIconPath(":/res/icons/common/menuitem/image-crop16.png");
    m_crop->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_crop);

    m_resize = new ActionButton();
    m_resize->setAccessibleName("ContextMenuButton");
    m_resize->setAction("resize");
    m_resize->setIconPath(":/res/icons/common/menuitem/resize16.png");
    m_resize->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_resize);

    mainPageLayout->addLayout(transformLayout);

    // --- Horizontal line below transform buttons ---
    QWidget *bottomLine = new QWidget();
    bottomLine->setFixedHeight(1);
    bottomLine->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    bottomLine->setAccessibleName("HLineSeparator");
    QVBoxLayout *lineBottomLayout = new QVBoxLayout();
    lineBottomLayout->setContentsMargins(11, 0, 11, 0);
    lineBottomLayout->addWidget(bottomLine);
    mainPageLayout->addLayout(lineBottomLayout);

    // --- Action items (ContextMenuItem list) ---
    QVBoxLayout *actionsLayout = new QVBoxLayout();
    actionsLayout->setSpacing(0);
    actionsLayout->setContentsMargins(0, 0, 0, 0);

    auto addItem = [&](ContextMenuItem *&item, const QString &action, const QString &text, const QString &icon) {
        item = new ContextMenuItem();
        item->setAction(action);
        item->setText(text);
        item->setIconPath(icon);
        actionsLayout->addWidget(item);
    };

    auto addSeparator = [](QVBoxLayout *layout, int topMargin = 4, int bottomMargin = 4) {
        QWidget *line = new QWidget();
        line->setFixedHeight(1);
        line->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        line->setAccessibleName("HLineSeparator");
        QVBoxLayout *lineLayout = new QVBoxLayout();
        lineLayout->setContentsMargins(11, topMargin, 11, bottomMargin);
        lineLayout->addWidget(line);
        layout->addLayout(lineLayout);
    };

    addItem(m_colorAdjustments, "colorAdjustments", tr("Color adjustments"), ":/res/icons/common/menuitem/appearance16.png");
    addItem(m_panoramaMode,     "togglePanorama",     tr("Panorama mode"),      ":/res/icons/common/menuitem/view16.png");
    addItem(m_casSettings,      "casSettings",        tr("CAS Settings"),       ":/res/icons/common/menuitem/appearance16.png");
    m_casSettings->hide();

    addSeparator(actionsLayout, 4, 4);

    addItem(m_print,            "print",              tr("Print"),              ":/res/icons/common/menuitem/print16.png");
    addItem(m_copy,             "copyFile",           tr("Quick copy"),         ":/res/icons/common/menuitem/copy16.png");
    addItem(m_move,             "moveFile",           tr("Quick move"),         ":/res/icons/common/menuitem/move16.png");
    addItem(m_rename,           "renameFile",         tr("Rename"),             ":/res/icons/common/overlay/edit16.png");
    addItem(m_folderView,       "folderView",         tr("Folder View"),        ":/res/icons/common/menuitem/folderview16.png");

    // OpenWith is special – we will create it separately
    m_openWith = new ContextMenuItem();
    m_openWith->setText(tr("Open with..."));
    m_openWith->setIconPath(":/res/icons/common/menuitem/run16.png");
    m_openWith->setPassthroughClicks(false);
    connect(m_openWith, &ContextMenuItem::pressed, this, &ContextMenu::switchToScriptsPage);
    actionsLayout->addWidget(m_openWith);

    addItem(m_showLocation,     "showInDirectory",    tr("Show in folder"),     ":/res/icons/common/menuitem/folder16.png");
    addItem(m_settings,         "openSettings",       tr("Settings"),           ":/res/icons/common/menuitem/settings16.png");

    addSeparator(actionsLayout, 4, 4);

    addItem(m_trash,            "moveToTrash",        tr("Move to trash"),      ":/res/icons/common/menuitem/trash16.png");
    m_trash->setTextColor(QColor("#ff5c5c"));
    m_trash->setIconColor(QColor("#ff5c5c"));

    addItem(m_deletePermanently, "removeFile",        tr("Delete permanently"), ":/res/icons/common/buttons/panel/close16.png");
    m_deletePermanently->setTextColor(QColor("#ff5c5c"));
    m_deletePermanently->setIconColor(QColor("#ff5c5c"));

    mainPageLayout->addLayout(actionsLayout);
    m_stackedWidget->addWidget(mainPage);

    // -------------------- Scripts page --------------------
    QWidget *scriptsPage = new QWidget();
    QVBoxLayout *scriptsPageLayout = new QVBoxLayout(scriptsPage);
    scriptsPageLayout->setSpacing(0);
    scriptsPageLayout->setContentsMargins(0, 0, 0, 0);

    m_scriptsLayout = new QVBoxLayout();
    m_scriptsLayout->setSpacing(0);
    m_scriptsLayout->setContentsMargins(0, 0, 0, 0);
    scriptsPageLayout->addLayout(m_scriptsLayout);

    QSpacerItem *scriptsSpacer1 = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);
    scriptsPageLayout->addSpacerItem(scriptsSpacer1);

    m_backButton = new ContextMenuItem();
    m_backButton->setText(tr("Back"));
    m_backButton->setIconPath(":/res/icons/common/menuitem/back16.png");
    m_backButton->setPassthroughClicks(false);
    connect(m_backButton, &ContextMenuItem::pressed, this, &ContextMenu::switchToMainPage);
    scriptsPageLayout->addWidget(m_backButton);

    QSpacerItem *scriptsSpacer2 = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);
    scriptsPageLayout->addSpacerItem(scriptsSpacer2);

    m_scriptSetupButton = new ContextMenuItem();
    m_scriptSetupButton->setText(tr("Configure menu"));
    m_scriptSetupButton->setIconPath(":/res/icons/common/menuitem/settings16.png");
    connect(m_scriptSetupButton, &ContextMenuItem::pressed, this, &ContextMenu::showScriptSettings);
    scriptsPageLayout->addWidget(m_scriptSetupButton);

    m_stackedWidget->addWidget(scriptsPage);

    // Initial refresh of scripts
    fillOpenWithMenu();
    adjustSize();
}

void ContextMenu::fillOpenWithMenu()
{
    // Clear existing items in scriptsLayout
    QLayoutItem *child;
    while ((child = m_scriptsLayout->takeAt(0)) != nullptr) {
        if (QWidget *w = child->widget()) {
            w->setParent(nullptr);
            w->deleteLater();
        }
        delete child;
    }

    auto scripts = scriptManager->allScripts();
    QMap<QString, Script>::iterator i;
    for (i = scripts.begin(); i != scripts.end(); ++i) {
        if (!i.value().command.isEmpty()) {
            ContextMenuItem *btn = new ContextMenuItem();
            btn->setAction("s:" + i.key());
            btn->setIconPath(":/res/icons/common/menuitem/open16.png");
            btn->setText(i.key());
            m_scriptsLayout->addWidget(btn);
        }
    }
}

void ContextMenu::switchToMainPage()
{
    m_stackedWidget->setCurrentIndex(0);
    adjustSize();
}

void ContextMenu::switchToScriptsPage()
{
    m_stackedWidget->setCurrentIndex(1);
    adjustSize();
}

QSize ContextMenu::sizeHint() const
{
    if (!m_stackedWidget || !m_stackedWidget->currentWidget()) {
        return QWidget::sizeHint();
    }
    QSize size = m_stackedWidget->currentWidget()->sizeHint();
    size.setHeight(size.height() + layout()->contentsMargins().top() + layout()->contentsMargins().bottom());
    return size;
}

void ContextMenu::setImageEntriesEnabled(bool mode)
{
    m_rotateLeft->setEnabled(mode);
    m_rotateRight->setEnabled(mode);
    m_flipH->setEnabled(mode);
    m_flipV->setEnabled(mode);
    m_crop->setEnabled(mode);
    m_resize->setEnabled(mode);
    m_copy->setEnabled(mode);
    m_move->setEnabled(mode);
    m_rename->setEnabled(mode);
    m_trash->setEnabled(mode);
    m_deletePermanently->setEnabled(mode);
    m_colorAdjustments->setEnabled(mode);
    m_panoramaMode->setEnabled(mode);
    m_openWith->setEnabled(mode);
    m_showLocation->setEnabled(mode);
}

void ContextMenu::setCasSettingsVisible(bool visible)
{
    m_casSettings->setVisible(visible);
    adjustSize();
}

void ContextMenu::showAt(QPoint pos)
{
    fillOpenWithMenu();
    switchToMainPage();
    show();
    adjustSize();
    QRect geom = geometry();
    geom.moveTopLeft(pos);
    setGeometry(geom);
    raise();
}

void ContextMenu::setGeometry(QRect geom)
{
    if (parentWidget()) {
        QRect parentRect = parentWidget()->rect();
        if (geom.bottom() > parentRect.bottom())
            geom.moveBottom(parentRect.bottom());
        if (geom.right() > parentRect.right())
            geom.moveRight(parentRect.right());
        if (geom.left() < 0)
            geom.moveLeft(0);
        if (geom.top() < 0)
            geom.moveTop(0);
    }
    QWidget::setGeometry(geom);
}

void ContextMenu::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    hide();
}

void ContextMenu::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ContextMenu::keyPressEvent(QKeyEvent *event)
{
    quint32 nativeScanCode = event->nativeScanCode();
    QString key = actionManager->keyForNativeScancode(nativeScanCode);
    if (key == "Esc")
        hide();
    else
        actionManager->processEvent(event);
}

bool ContextMenu::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
        QPoint clickPos = QCursor::pos();
        if (!rect().contains(mapFromGlobal(clickPos))) {
            hide();
        }
    }
    return QWidget::eventFilter(obj, event);
}
