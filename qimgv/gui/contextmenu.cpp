#include "contextmenu.h"
#include "settings.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/customwidgets/contextmenuitem.h"
#include "gui/uimetrics.h"
#include "utils/iconfontmanager.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>

namespace {
// All ContextMenu icons (zoom row, transform row, and the ContextMenuItem
// list via addItem()) render at this glyph size. Change this one value
// instead of every setIcon() call site.
constexpr int kIconSizePx = UiMetrics::kStandardIconSizePx;
constexpr int kMenuItemHorizontalInsetPx = 3;
constexpr int kSeparatorHorizontalMarginPx = 11;
} // namespace

ContextMenu::ContextMenu(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(settings, &Settings::settingsChanged,
            this, &ContextMenu::updateDestructiveActionColors);
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
    m_mainPage = mainPage;
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
    m_fitWindow->setIcon(FluentIcon::ArrowExpand20, kIconSizePx);
    m_fitWindow->setIconOffset(-2, 0);
    m_fitWindow->setToolTip(tr("Fit to window"));
    m_fitWindow->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_fitWindow);

    m_fitWidth = new ActionButton();
    m_fitWidth->setAccessibleName("ContextMenuButton");
    m_fitWidth->setAction("fitWidth");
    m_fitWidth->setIcon(FluentIcon::ArrowAutofitWidth20, kIconSizePx);
    m_fitWidth->setToolTip(tr("Fit to width"));
    m_fitWidth->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_fitWidth);

    m_fitHeight = new ActionButton();
    m_fitHeight->setAccessibleName("ContextMenuButton");
    m_fitHeight->setAction("fitHeight");
    m_fitHeight->setIcon(FluentIcon::ArrowAutofitHeight20, kIconSizePx);
    m_fitHeight->setToolTip(tr("Fit to height"));
    m_fitHeight->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_fitHeight);

    m_zoomOriginal = new ActionButton();
    m_zoomOriginal->setAccessibleName("ContextMenuButton");
    m_zoomOriginal->setAction("fitNormal");
    m_zoomOriginal->setIcon(FluentIcon::ZoomOriginal20, kIconSizePx);
    m_zoomOriginal->setIconOffset(0, 1);
    m_zoomOriginal->setToolTip(tr("Original size"));
    m_zoomOriginal->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_zoomOriginal);

    m_zoomIn = new ActionButton();
    m_zoomIn->setAccessibleName("ContextMenuButton");
    m_zoomIn->setAction("zoomIn");
    m_zoomIn->setIcon(FluentIcon::ZoomIn20, kIconSizePx);
    m_zoomIn->setToolTip(tr("Zoom in"));
    m_zoomIn->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_zoomIn);

    m_zoomOut = new ActionButton();
    m_zoomOut->setAccessibleName("ContextMenuButton");
    m_zoomOut->setAction("zoomOut");
    m_zoomOut->setIcon(FluentIcon::ZoomOut20, kIconSizePx);
    m_zoomOut->setToolTip(tr("Zoom out"));
    m_zoomOut->setTriggerMode(TriggerMode::PressTrigger);
    zoomLayout->addWidget(m_zoomOut);

    mainPageLayout->addLayout(zoomLayout);

    // --- Transform buttons row ---
    QHBoxLayout *transformLayout = new QHBoxLayout();
    transformLayout->setSpacing(0);
    transformLayout->setContentsMargins(4, 0, 4, 0);

    m_rotateLeft = new ActionButton();
    m_rotateLeft->setAccessibleName("ContextMenuButton");
    m_rotateLeft->setAction("rotateLeft");
    m_rotateLeft->setIcon(FluentIcon::ArrowRotateCounterclockwise20, kIconSizePx);
    m_rotateLeft->setIconOffset(-2, 0);
    m_rotateLeft->setToolTip(tr("Rotate left"));
    m_rotateLeft->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_rotateLeft);

    m_rotateRight = new ActionButton();
    m_rotateRight->setAccessibleName("ContextMenuButton");
    m_rotateRight->setAction("rotateRight");
    m_rotateRight->setIcon(FluentIcon::ArrowRotateClockwise20, kIconSizePx);
    m_rotateRight->setToolTip(tr("Rotate right"));
    m_rotateRight->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_rotateRight);

    m_flipV = new ActionButton();
    m_flipV->setAccessibleName("ContextMenuButton");
    m_flipV->setAction("flipV");
    m_flipV->setIcon(FluentIcon::FlipVertical20, kIconSizePx);
    m_flipV->setIconOffset(-3, 0);
    m_flipV->setToolTip(tr("Flip vertical"));
    m_flipV->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_flipV);

    m_flipH = new ActionButton();
    m_flipH->setAccessibleName("ContextMenuButton");
    m_flipH->setAction("flipH");
    m_flipH->setIcon(FluentIcon::FlipHorizontal20, kIconSizePx);
    m_flipH->setIconOffset(-1, 2);
    m_flipH->setToolTip(tr("Flip horizontal"));
    m_flipH->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_flipH);

    m_crop = new ActionButton();
    m_crop->setAccessibleName("ContextMenuButton");
    m_crop->setAction("crop");
    m_crop->setIcon(FluentIcon::Crop20, kIconSizePx);
    m_crop->setToolTip(tr("Crop"));
    m_crop->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_crop);

    m_resize = new ActionButton();
    m_resize->setAccessibleName("ContextMenuButton");
    m_resize->setAction("resize");
    m_resize->setIcon(FluentIcon::Resize20, kIconSizePx);
    m_resize->setToolTip(tr("Resize"));
    m_resize->setTriggerMode(TriggerMode::PressTrigger);
    transformLayout->addWidget(m_resize);

    mainPageLayout->addLayout(transformLayout);

    // --- Horizontal line below transform buttons ---
    QWidget *bottomLine = new QWidget();
    bottomLine->setFixedHeight(1);
    bottomLine->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    bottomLine->setAccessibleName("HLineSeparator");
    QVBoxLayout *lineBottomLayout = new QVBoxLayout();
    lineBottomLayout->setContentsMargins(kSeparatorHorizontalMarginPx, 0,
                                         kSeparatorHorizontalMarginPx, 0);
    lineBottomLayout->addWidget(bottomLine);
    mainPageLayout->addLayout(lineBottomLayout);

    // --- Action items (ContextMenuItem list) ---
    QVBoxLayout *actionsLayout = new QVBoxLayout();
    actionsLayout->setSpacing(0);
    actionsLayout->setContentsMargins(kMenuItemHorizontalInsetPx, 0,
                                      kMenuItemHorizontalInsetPx, 0);

    auto addItem = [](ContextMenuItem *&item, QVBoxLayout *layout, const QString &action, const QString &text, FluentIcon icon) {
        item = new ContextMenuItem();
        item->setAction(action);
        item->setText(text);
        item->setIcon(icon, kIconSizePx);
        layout->addWidget(item);
    };

    auto addSeparator = [](QVBoxLayout *layout, int topMargin = 4, int bottomMargin = 4) {
        QWidget *line = new QWidget();
        line->setFixedHeight(1);
        line->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        line->setAccessibleName("HLineSeparator");
        QVBoxLayout *lineLayout = new QVBoxLayout();
        constexpr int nestedSeparatorMarginPx =
            kSeparatorHorizontalMarginPx - kMenuItemHorizontalInsetPx;
        lineLayout->setContentsMargins(nestedSeparatorMarginPx, topMargin,
                                       nestedSeparatorMarginPx, bottomMargin);
        lineLayout->addWidget(line);
        layout->addLayout(lineLayout);
    };

    addItem(m_colorAdjustments, actionsLayout, "colorAdjustments", tr("Color adjustments"),    FluentIcon::Adjustments20);
    addItem(m_panoramaMode,     actionsLayout, "togglePanorama",     tr("Panorama mode"),      FluentIcon::Panorama20);
    addItem(m_aiUpscale,        actionsLayout, "toggleUpscayl",      tr("AI Upscale"),         FluentIcon::AiUpscale20);
    addItem(m_casSettings,      actionsLayout, "casSettings",        tr("CAS Settings"),       FluentIcon::Blur20);
    m_casSettings->hide();

    addSeparator(actionsLayout, 4, 4);

    addItem(m_copy,             actionsLayout, "copyFile",           tr("Quick copy"),         FluentIcon::CopyAdd20);
    addItem(m_move,             actionsLayout, "moveFile",           tr("Quick move"),         FluentIcon::Move20);
    addItem(m_folderView,       actionsLayout, "folderView",         tr("Folder View"),        FluentIcon::Grid20);

    addItem(m_showLocation,     actionsLayout, "showInDirectory",    tr("Show in folder"),     FluentIcon::ShowInFolder20);
    addItem(m_imageInfo,        actionsLayout, "toggleImageInfo",    tr("Image info"),         FluentIcon::Info20);
    addItem(m_settings,         actionsLayout, "openSettings",       tr("Settings"),           FluentIcon::Settings20);

    addSeparator(actionsLayout, 4, 4);

    // "More" - toggles m_moreContainer in place, no ActionManager dispatch.
    m_more = new ContextMenuItem();
    m_more->setText(tr("More"));
    m_more->setIcon(FluentIcon::ChevronDown20, kIconSizePx);
    m_more->setIconOffset(0, 2);
    m_more->setPassthroughClicks(false);
    connect(m_more, &ContextMenuItem::pressed, this, &ContextMenu::toggleMoreExpanded);
    actionsLayout->addWidget(m_more);

    m_moreContainer = new QWidget();
    QVBoxLayout *moreLayout = new QVBoxLayout(m_moreContainer);
    moreLayout->setSpacing(0);
    moreLayout->setContentsMargins(0, 0, 0, 0);

    // OpenWith is special – we will create it separately
    m_openWith = new ContextMenuItem();
    m_openWith->setText(tr("Open with..."));
    m_openWith->setIcon(FluentIcon::OpenWith20, kIconSizePx);
    m_openWith->setPassthroughClicks(false);
    connect(m_openWith, &ContextMenuItem::pressed, this, &ContextMenu::switchToScriptsPage);
    moreLayout->addWidget(m_openWith);

    addItem(m_rename,           moreLayout, "renameFile",         tr("Rename"),             FluentIcon::Rename20);
    addItem(m_setWallpaper,     moreLayout, "setWallpaper",       tr("Set as wallpaper"),   FluentIcon::Wallpaper20);
    addItem(m_print,            moreLayout, "print",              tr("Print"),              FluentIcon::Print20);

    addSeparator(moreLayout, 4, 4);

    addItem(m_trash,            moreLayout, "moveToTrash",        tr("Move to trash"),      FluentIcon::Delete20);

    addItem(m_deletePermanently, moreLayout, "removeFile",        tr("Delete permanently"), FluentIcon::Dismiss20);
    m_deletePermanently->setIconOffset(0, 1);
    updateDestructiveActionColors();

    m_moreContainer->hide();
    actionsLayout->addWidget(m_moreContainer);

    mainPageLayout->addLayout(actionsLayout);
    m_stackedWidget->addWidget(mainPage);

    // -------------------- Scripts page --------------------
    QWidget *scriptsPage = new QWidget();
    QVBoxLayout *scriptsPageLayout = new QVBoxLayout(scriptsPage);
    scriptsPageLayout->setSpacing(0);
    scriptsPageLayout->setContentsMargins(kMenuItemHorizontalInsetPx, 0,
                                          kMenuItemHorizontalInsetPx, 0);

    m_scriptsLayout = new QVBoxLayout();
    m_scriptsLayout->setSpacing(0);
    m_scriptsLayout->setContentsMargins(0, 0, 0, 0);
    scriptsPageLayout->addLayout(m_scriptsLayout);

    QSpacerItem *scriptsSpacer1 = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);
    scriptsPageLayout->addSpacerItem(scriptsSpacer1);

    m_backButton = new ContextMenuItem();
    m_backButton->setText(tr("Back"));
    m_backButton->setIcon(FluentIcon::ArrowLeft, kIconSizePx);
    m_backButton->setPassthroughClicks(false);
    connect(m_backButton, &ContextMenuItem::pressed, this, &ContextMenu::switchToMainPage);
    scriptsPageLayout->addWidget(m_backButton);

    QSpacerItem *scriptsSpacer2 = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);
    scriptsPageLayout->addSpacerItem(scriptsSpacer2);

    m_scriptSetupButton = new ContextMenuItem();
    m_scriptSetupButton->setText(tr("Configure menu"));
    m_scriptSetupButton->setIcon(FluentIcon::Settings20, kIconSizePx);
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
            btn->setIcon(FluentIcon::ShowInFolder20, kIconSizePx);
            btn->setText(i.key());
            m_scriptsLayout->addWidget(btn);
        }
    }
}

void ContextMenu::updateDestructiveActionColors()
{
    const ColorScheme &colors = settings->colorScheme();
    m_trash->setTextColor(colors.trash);
    m_trash->setIconColor(colors.trash);
    m_deletePermanently->setTextColor(colors.danger);
    m_deletePermanently->setIconColor(colors.danger);
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

void ContextMenu::toggleMoreExpanded()
{
    bool expanded = !m_moreContainer->isVisible();
    m_moreContainer->setVisible(expanded);
    m_more->setIcon(expanded ? FluentIcon::ChevronUp20 : FluentIcon::ChevronDown20, kIconSizePx);
    // actionsLayout caches its sizeHint independently of mainPageLayout;
    // hiding/showing m_moreContainer doesn't invalidate that nested cache
    // on its own. activate() forces a full synchronous relayout of the
    // current page before sizeHint()/adjustSize() are queried below.
    m_stackedWidget->currentWidget()->layout()->activate();
    adjustSize();
}

QSize ContextMenu::sizeHint() const
{
    if (!m_stackedWidget || !m_stackedWidget->currentWidget()) {
        return QWidget::sizeHint();
    }
    QWidget *current = m_stackedWidget->currentWidget();
    QSize size = current->sizeHint();
    if (current == m_mainPage) {
        // m_moreContainer's own sizeHint() is unaffected by its hidden
        // state (hidden-ness only matters when a widget is queried as an
        // item of another layout, not when queried directly). Include the
        // parent actions layout's horizontal insets so the collapsed width
        // already matches the width required after expansion.
        const int expandedWidth = m_moreContainer->sizeHint().width()
                                  + 2 * kMenuItemHorizontalInsetPx;
        size.setWidth(qMax(size.width(), expandedWidth));
    }
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
    m_imageInfo->setEnabled(mode);
    m_setWallpaper->setEnabled(mode);
    m_aiUpscale->setEnabled(mode);
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
    m_moreContainer->hide();
    m_more->setIcon(FluentIcon::ChevronDown20, kIconSizePx);
    m_stackedWidget->currentWidget()->layout()->activate();
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
