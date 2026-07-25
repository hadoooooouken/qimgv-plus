#include "folderview.h"
#include "settings.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <cmath>
#include <QSplitter>
#include <QHeaderView>
#include <QPainter>
#include "gui/customwidgets/clickablelabel.h"
#include "gui/folderview/treeviewcustom.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/uimetrics.h"
#include "utils/iconfontmanager.h"

namespace {
constexpr int kActionIconSizePx = UiMetrics::kStandardIconSizePx;
constexpr int kCaptionControlSpacingPx = 7;
constexpr int kComboBoxIconVerticalOffsetPx = 1;
constexpr int kFormatFilterIconVerticalOffsetPx = 2;
constexpr int kTopBarRightMarginPx = 5;
// sortingComboBox / folderSortingComboBox / formatFilterComboBox (their own
// StyledComboBox icon, not the dropdown chevron).
constexpr int kCompactIconSizePx = UiMetrics::kCompactIconSizePx;
} // namespace

class BatchConvertButton : public QPushButton {
public:
    QPixmap iconPixmap;
    BatchConvertButton(const QString& text, QWidget* parent = nullptr) : QPushButton(text, parent) {
        updateIcon();
        setStyleSheet("text-align: left; padding-left: 8px; padding-right: 38px;");
        
        connect(settings, &Settings::settingsChanged, this, [this]() {
            updateIcon();
            update();
        });
    }

    void updateIcon() {
        iconPixmap = IconFontManager::pixmap(
            FluentIcon::BatchConvert16,
            kCompactIconSizePx,
            settings->colorScheme().icons,
            devicePixelRatioF());
    }

    void paintEvent(QPaintEvent* e) override {
        QPushButton::paintEvent(e);
        QPainter p(this);
        int y = (height() - iconPixmap.height() / iconPixmap.devicePixelRatio()) / 2;
        int x = width() - iconPixmap.width() / iconPixmap.devicePixelRatio() - 14;
        p.drawPixmap(x, y, iconPixmap);
    }
};

FolderView::FolderView(QWidget *parent) :
    FloatingWidgetContainer(parent)
{
    setupUi();

    // ------- filesystem view --------
    QString style = "font: %1pt;";
    style = style.arg(QApplication::font().pointSize());
    dirTreeView->setStyleSheet(style);


    dirModel = new FileSystemModelCustom(this);
    dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    dirTreeView->setModel(dirModel);

    QHeaderView* header = dirTreeView->header();
    header->hideSection(1); // size
    header->hideSection(2); // type
    header->hideSection(3); // mod date

    dirModel->setRootPath("");
    connect(dirModel, &QFileSystemModel::directoryLoaded,
        this, [this](const QString &path) {
            if (path == m_pendingScrollPath) {
                QModelIndex current = dirTreeView->currentIndex();
                if (current.isValid() && dirModel->filePath(current) == path) {
                    dirTreeView->scrollTo(current, QAbstractItemView::PositionAtCenter);
                }
                m_pendingScrollPath.clear();
            }
        });
    // -------------------------------
    upButton->setAction("goUp");
    upButton->setIcon(FluentIcon::ChevronUp16, kCompactIconSizePx);
    upButton->setIconOffset(0, 2);
    upButton->setTriggerMode(TriggerMode::ClickTrigger);
    settingsButton->setAction("openSettings");
    settingsButton->setIcon(FluentIcon::Settings20, kActionIconSizePx);
    exitButton->setAction("exit");
    exitButton->setIcon(FluentIcon::ArrowExit20, kActionIconSizePx);
    docViewButton->setAction("documentView");
    docViewButton->setIcon(FluentIcon::DocumentView20, kActionIconSizePx);
    togglePlacesPanelButton->setCheckable(true);
    togglePlacesPanelButton->setIcon(FluentIcon::PanelLeft20, kActionIconSizePx);
    togglePlacesPanelButton->setIconOffset(1, 0);


    sortingComboBox->setIcon(FluentIcon::ArrowSort16, kCompactIconSizePx);
    folderSortingComboBox->setIcon(FluentIcon::Folder16, kCompactIconSizePx);
    formatFilterComboBox->setIcon(FluentIcon::Checkmark16, kCompactIconSizePx);
    const QPoint comboBoxIconOffset(0, kComboBoxIconVerticalOffsetPx);
    sortingComboBox->setIconOffset(comboBoxIconOffset);
    folderSortingComboBox->setIconOffset(comboBoxIconOffset);
    formatFilterComboBox->setIconOffset(QPoint(0, kFormatFilterIconVerticalOffsetPx));

    newBookmarkButton->setIcon(FluentIcon::BookmarkAdd20, kActionIconSizePx);
    homeButton->setIcon(FluentIcon::Home20, kActionIconSizePx);

    bookmarksLabel->setAcceptDrops(true);
    newBookmarkButton->setAcceptDrops(true);
    bookmarksLabel->installEventFilter(this);
    newBookmarkButton->installEventFilter(this);

    int min = thumbnailGrid->THUMBNAIL_SIZE_MIN;
    int max = thumbnailGrid->THUMBNAIL_SIZE_MAX;
    int step = thumbnailGrid->ZOOM_STEP;

    zoomSlider->setMinimum(700);
    zoomSlider->setMaximum(900);
    zoomSlider->setSingleStep(1);
    zoomSlider->setPageStep(50);

    splitter->setStretchFactor(1, 50);

    connect(thumbnailGrid, &FolderGridView::thumbnailsRequested,  this, &FolderView::thumbnailsRequested);
    connect(thumbnailGrid, &FolderGridView::thumbnailSizeChanged, this, &FolderView::onThumbnailSizeChanged);
    connect(thumbnailGrid, &FolderGridView::itemActivated,   this, &FolderView::itemActivated);
    connect(thumbnailGrid, &FolderGridView::draggedOut,      this, &FolderView::draggedOut);
    connect(thumbnailGrid, &FolderGridView::draggedOver,     this, &FolderView::draggedOver);
    connect(thumbnailGrid, &FolderGridView::droppedInto,     this, &FolderView::droppedInto);
    connect(thumbnailGrid, &FolderGridView::backRequested,    this, &FolderView::backRequested);
    connect(thumbnailGrid, &FolderGridView::forwardRequested, this, &FolderView::forwardRequested);
    connect(thumbnailGrid, &FolderGridView::batchRequested,   this, &FolderView::batchRequested);
    connect(thumbnailGrid, &FolderGridView::openSelectedRequested, this, &FolderView::openSelectedRequested);

    connect(bookmarksWidget, &BookmarksWidget::bookmarkClicked, this, &FolderView::onBookmarkClicked);

    connect(newBookmarkButton, &IconButton::clicked, this, &FolderView::newBookmark);
    connect(homeButton, &IconButton::clicked, this, &FolderView::onHomeBtn);

    connect(zoomSlider, &QSlider::valueChanged, this, &FolderView::onZoomSliderValueChanged);
    connect(zoomSlider, &QSlider::sliderReleased, this, [this]() {
        settings->setFolderViewIconSize(thumbnailGrid->thumbnailSize());
    });
    connect(sortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onSortingSelected);
    connect(folderSortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onFolderSortingSelected);
    connect(formatFilterComboBox, &FormatFilterComboBox::formatSelectionChanged, this, &FolderView::formatFilterSelected);
    connect(togglePlacesPanelButton, &ActionButton::toggled, this, &FolderView::onPlacesPanelButtonChecked);


    connect(dirTreeView, &TreeViewCustom::clicked, this, &FolderView::onTreeViewClicked);
    connect(bookmarksLabel, &ClickableLabel::clicked, this, &FolderView::toggleBookmarks);
    connect(directoriesLabel, &ClickableLabel::clicked, this, &FolderView::toggleFilesystemView);

    connect(dirTreeView, &TreeViewCustom::droppedIn, this, &FolderView::onDroppedInByIndex);
    connect(dirTreeView, &TreeViewCustom::tabbedOut, this, &FolderView::onTreeViewTabOut);
    connect(bookmarksWidget, &BookmarksWidget::droppedIn, this, &FolderView::onBookmarkDroppedIn);

    sortingComboBox->setItemDelegate(new QStyledItemDelegate(sortingComboBox));
    sortingComboBox->view()->setTextElideMode(Qt::ElideNone);
    folderSortingComboBox->setItemDelegate(new QStyledItemDelegate(folderSortingComboBox));
    folderSortingComboBox->view()->setTextElideMode(Qt::ElideNone);
    formatFilterComboBox->setItemDelegate(new QStyledItemDelegate(formatFilterComboBox));
    formatFilterComboBox->view()->setTextElideMode(Qt::ElideNone);

    connect(splitter, &QSplitter::splitterMoved, this, &FolderView::onSplitterMoved);

    readSettings();

    QSizePolicy sp_retain = sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    setSizePolicy(sp_retain);
    connect(settings, &Settings::settingsChanged, this, &FolderView::readSettings);

    selectionCountLabel->hide();
    batchButton->hide();

    connect(thumbnailGrid, &FolderGridView::selectionChanged, this, &FolderView::onSelectionChanged);
    connect(batchButton, &QPushButton::clicked, this, &FolderView::onBatchClicked);

    hide();
}

void FolderView::setupUi() {
    this->setFocusPolicy(Qt::StrongFocus);
    
    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    
    // Top Bar
    topBar = new QWidget(this);
    topBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    topBar->setAccessibleName("FolderViewTopBar");
    
    QHBoxLayout *horizontalLayout_5 = new QHBoxLayout(topBar);
    horizontalLayout_5->setSpacing(0);
    horizontalLayout_5->setContentsMargins(0, 0, kTopBarRightMarginPx, 0);
    
    togglePlacesPanelButton = new ActionButton(topBar);
    togglePlacesPanelButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    togglePlacesPanelButton->setMinimumWidth(40);
    togglePlacesPanelButton->setMaximumWidth(40);
    togglePlacesPanelButton->setAccessibleName("CheckableButtonLE");
    togglePlacesPanelButton->setToolTip(tr("Toggle side panel"));
    horizontalLayout_5->addWidget(togglePlacesPanelButton);
    
    pathBar = new QWidget(topBar);
    pathBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    pathBar->setAccessibleName("PathBar");
    
    QHBoxLayout *horizontalLayout = new QHBoxLayout(pathBar);
    horizontalLayout->setSpacing(0);
    horizontalLayout->setContentsMargins(0, 0, 0, 0);
    
    upButton = new ActionButton(pathBar);
    upButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    upButton->setMinimumWidth(34);
    upButton->setMaximumWidth(34);
    upButton->setAccessibleName("PathBarButton");
    upButton->setToolTip(tr("Go up"));
    horizontalLayout->addWidget(upButton);
    
    pathLabel = new QLabel("[path]", pathBar);
    pathLabel->setCursor(Qt::IBeamCursor);
    horizontalLayout->addWidget(pathLabel);
    
    horizontalLayout_5->addWidget(pathBar);
    
    pathbarSpacer = new QSpacerItem(0, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(pathbarSpacer);
    
    selectionCountLabel = new QLabel(tr("15 images selected"), topBar);
    selectionCountLabel->setAccessibleName("SelectionCountLabel");
    horizontalLayout_5->addWidget(selectionCountLabel);
    
    batchButton = new BatchConvertButton(tr("Batch convert"), topBar);
    batchButton->setFocusPolicy(Qt::NoFocus);
    batchButton->setAccessibleName("FolderViewBatchButton");
    horizontalLayout_5->addWidget(batchButton);
    
    gridSizeLabel = new QLabel(tr("Grid size"), topBar);
    gridSizeLabel->setAccessibleName("FolderViewGridSizeLabel");
    horizontalLayout_5->addWidget(gridSizeLabel);

    zoomSlider = new QSlider(Qt::Horizontal, topBar);
    zoomSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    zoomSlider->setMinimumWidth(110);
    zoomSlider->setMaximumWidth(110);
    zoomSlider->setFocusPolicy(Qt::NoFocus);
    zoomSlider->setContextMenuPolicy(Qt::NoContextMenu);
    zoomSlider->setAccessibleName("FolderViewSlider");
    zoomSlider->setTickPosition(QSlider::NoTicks);
    horizontalLayout_5->addWidget(zoomSlider);
    
    zoomSliderSpacer = new QSpacerItem(3, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(zoomSliderSpacer);
    
    QSpacerItem *horizontalSpacer_3 = new QSpacerItem(1, 20, QSizePolicy::Maximum, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(horizontalSpacer_3);
    
    folderSortingComboBox = new StyledComboBox(topBar);
    folderSortingComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    folderSortingComboBox->setContextMenuPolicy(Qt::NoContextMenu);
    folderSortingComboBox->setAccessibleName("PanelComboBox");
    folderSortingComboBox->setContextMenuPopupStyle(true);
    folderSortingComboBox->addItems({tr("A - Z"), tr("Z - A"), tr("Size"), tr("Size (desc)"), tr("Oldest"), tr("Newest")});
    folderSortingComboBox->setToolTip(tr("Folder icon sorting"));
    horizontalLayout_5->addWidget(folderSortingComboBox);
    
    horizontalSpacer_folderSort = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(horizontalSpacer_folderSort);
    
    sortingComboBox = new StyledComboBox(topBar);
    sortingComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    sortingComboBox->setContextMenuPolicy(Qt::NoContextMenu);
    sortingComboBox->setAccessibleName("PanelComboBox");
    sortingComboBox->setContextMenuPopupStyle(true);
    sortingComboBox->addItems({tr("A - Z"), tr("Z - A"), tr("Size"), tr("Size (desc)"), tr("Oldest"), tr("Newest")});
    sortingComboBox->setToolTip(tr("Sort folders and images"));
    horizontalLayout_5->addWidget(sortingComboBox);
    
    horizontalSpacer_formatFilter = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(horizontalSpacer_formatFilter);
    
    formatFilterComboBox = new FormatFilterComboBox(topBar);
    formatFilterComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    formatFilterComboBox->setContextMenuPolicy(Qt::NoContextMenu);
    formatFilterComboBox->setAccessibleName("PanelComboBox");
    formatFilterComboBox->setContextMenuPopupStyle(true);
    formatFilterComboBox->setToolTip(tr("Filter by file format"));
    horizontalLayout_5->addWidget(formatFilterComboBox);
    
    QSpacerItem *horizontalSpacer_2 = new QSpacerItem(8, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(horizontalSpacer_2);
    
    docViewButton = new ActionButton(topBar);
    docViewButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    docViewButton->setMinimumWidth(38);
    docViewButton->setMaximumWidth(38);
    docViewButton->setAccessibleName("PanelButton");
    docViewButton->setToolTip(tr("Viewer"));
    horizontalLayout_5->addWidget(docViewButton);
    horizontalLayout_5->addSpacing(kCaptionControlSpacingPx);
    
    settingsButton = new ActionButton(topBar);
    settingsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    settingsButton->setMinimumWidth(38);
    settingsButton->setMaximumWidth(38);
    settingsButton->setAccessibleName("PanelButton");
    settingsButton->setToolTip(tr("Settings"));
    horizontalLayout_5->addWidget(settingsButton);
    
    panelRightEdgeSpacer = new QSpacerItem(
        kCaptionControlSpacingPx, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
    horizontalLayout_5->addSpacerItem(panelRightEdgeSpacer);
    
    exitButton = new ActionButton(topBar);
    exitButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    exitButton->setMinimumWidth(38);
    exitButton->setMaximumWidth(38);
    exitButton->setAccessibleName("PanelButton");
    exitButton->setToolTip(tr("Quit qimgv-plus"));
    horizontalLayout_5->addWidget(exitButton);
    
    verticalLayout->addWidget(topBar);
    
    // Contents
    contentsWidget = new QWidget(this);
    contentsWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    contentsWidget->setAccessibleName("FolderViewContents");
    
    QHBoxLayout *horizontalLayout_7 = new QHBoxLayout(contentsWidget);
    horizontalLayout_7->setSpacing(0);
    horizontalLayout_7->setContentsMargins(0, 1, 0, 0);
    
    splitter = new QSplitter(Qt::Horizontal, contentsWidget);
    splitter->setAccessibleName("FolderViewSplitter");
    splitter->setHandleWidth(6);
    splitter->setChildrenCollapsible(false);
    
    // Places Panel
    placesPanel = new QWidget(splitter);
    placesPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    placesPanel->setMinimumWidth(260);
    placesPanel->setAccessibleName("PlacesPanel");
    
    QVBoxLayout *verticalLayout_2 = new QVBoxLayout(placesPanel);
    verticalLayout_2->setSpacing(6);
    verticalLayout_2->setContentsMargins(0, 7, 6, 0);
    
    QHBoxLayout *horizontalLayout_3 = new QHBoxLayout();
    horizontalLayout_3->setSpacing(16);
    horizontalLayout_3->setContentsMargins(8, 0, 5, 0);
    
    bookmarksLabel = new ClickableLabel(placesPanel);
    bookmarksLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    bookmarksLabel->setCursor(Qt::PointingHandCursor);
    bookmarksLabel->setMouseTracking(true);
    bookmarksLabel->setAccessibleName("PanelSectionHeader");
    bookmarksLabel->setText(tr("BOOKMARKS"));
    horizontalLayout_3->addWidget(bookmarksLabel);
    
    newBookmarkButton = new IconButton(placesPanel);
    newBookmarkButton->setMinimumSize(26, 26);
    newBookmarkButton->setMaximumSize(26, 26);
    newBookmarkButton->setAccessibleName("PlacesPanelButton");
    horizontalLayout_3->addWidget(newBookmarkButton);
    
    verticalLayout_2->addLayout(horizontalLayout_3);
    
    bookmarksWidget = new BookmarksWidget(placesPanel);
    verticalLayout_2->addWidget(bookmarksWidget);
    
    placesPanelSpacer = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);
    verticalLayout_2->addSpacerItem(placesPanelSpacer);
    
    QHBoxLayout *horizontalLayout_6 = new QHBoxLayout();
    horizontalLayout_6->setSpacing(16);
    horizontalLayout_6->setContentsMargins(8, 0, 5, 0);
    
    directoriesLabel = new ClickableLabel(placesPanel);
    directoriesLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    directoriesLabel->setCursor(Qt::PointingHandCursor);
    directoriesLabel->setMouseTracking(true);
    directoriesLabel->setAccessibleName("PanelSectionHeader");
    directoriesLabel->setText(tr("FILESYSTEM"));
    horizontalLayout_6->addWidget(directoriesLabel);
    
    QHBoxLayout *horizontalLayout_8 = new QHBoxLayout();
    horizontalLayout_8->setSpacing(0);
    horizontalLayout_8->setContentsMargins(0, 0, 0, 0);
    
    homeButton = new IconButton(placesPanel);
    homeButton->setMinimumSize(26, 26);
    homeButton->setMaximumSize(26, 26);
    homeButton->setAccessibleName("PlacesPanelButton");
    homeButton->setToolTip(tr("Home"));
    horizontalLayout_8->addWidget(homeButton);
    
    horizontalLayout_6->addLayout(horizontalLayout_8);
    verticalLayout_2->addLayout(horizontalLayout_6);
    
    dirTreeView = new TreeViewCustom(placesPanel);
    dirTreeView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    dirTreeView->setMinimumSize(1, 1);
    dirTreeView->setFrameShape(QFrame::NoFrame);
    dirTreeView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    dirTreeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    dirTreeView->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    dirTreeView->setUniformRowHeights(true);
    dirTreeView->setAnimated(false);
    dirTreeView->setHeaderHidden(true);
    dirTreeView->setExpandsOnDoubleClick(true);
    verticalLayout_2->addWidget(dirTreeView);
    
    verticalLayout_2->addSpacerItem(new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    
    splitter->addWidget(placesPanel);
    
    // Grid Holder
    thumbnailGridHolder = new QWidget(splitter);
    QHBoxLayout *horizontalLayout_2 = new QHBoxLayout(thumbnailGridHolder);
    horizontalLayout_2->setSpacing(0);
    horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
    
    thumbnailGrid = new FolderGridView(thumbnailGridHolder);
    thumbnailGrid->setFrameShape(QFrame::NoFrame);
    thumbnailGrid->setLineWidth(0);
    thumbnailGrid->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    thumbnailGrid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    horizontalLayout_2->addWidget(thumbnailGrid);
    
    splitter->addWidget(thumbnailGridHolder);
    horizontalLayout_7->addWidget(splitter);
    
    verticalLayout->addWidget(contentsWidget);
}

void FolderView::readSettings() {
    int currentRes = settings->thumbnailResolution();
    if (currentRes != lastThumbnailResolution) {
        thumbnailGrid->unloadAllThumbnails();
        lastThumbnailResolution = currentRes;
    }
    thumbnailGrid->setThumbnailSize(settings->folderViewIconSize());
    thumbnailGrid->setShowLabels(true);
    togglePlacesPanelButton->setChecked(settings->placesPanel());

    folderSortingComboBox->blockSignals(true);
    folderSortingComboBox->setCurrentIndex(static_cast<int>(settings->folderIconSortingMode()));
    folderSortingComboBox->blockSignals(false);

    formatFilterComboBox->setCheckedExtensions(settings->formatFilter());

    setPlacesPanel(settings->placesPanel());
    bookmarksWidget->setVisible(settings->placesPanelBookmarksExpanded());
    dirTreeView->setVisible(settings->placesPanelTreeExpanded());

    QList<int> sizes;
    sizes << settings->placesPanelWidth() << 1;
    splitter->setSizes(sizes);
}

void FolderView::onSplitterMoved() {
    settings->setPlacesPanelWidth(placesPanel->width());
}

void FolderView::onPlacesPanelButtonChecked(bool mode) {
    setPlacesPanel(mode);
    settings->setPlacesPanel(mode);
}

void FolderView::setPlacesPanel(bool mode) {
    if(width() >= 600)
        placesPanel->setVisible(mode);
}

void FolderView::toggleBookmarks() {
    if(bookmarksWidget->isVisible())
        bookmarksWidget->hide();
    else
        bookmarksWidget->show();
    settings->setPlacesPanelBookmarksExpanded(bookmarksWidget->isVisible());
}

void FolderView::toggleFilesystemView() {
    if(dirTreeView->isVisible())
        dirTreeView->hide();
    else
        dirTreeView->show();
    settings->setPlacesPanelTreeExpanded(dirTreeView->isVisible());
}

void FolderView::onTreeViewTabOut() {
    thumbnailGrid->setFocus();
}

void FolderView::onDroppedInByIndex(QList<QString> paths, QModelIndex index, Qt::DropAction action) {
    QString destDir = dirModel->filePath(index);
    if (action == Qt::CopyAction) {
        emit copyUrlsRequested(paths, destDir);
    } else if (action == Qt::MoveAction) {
        emit moveUrlsRequested(paths, destDir);
    }
}

void FolderView::onBookmarkDroppedIn(QList<QString> paths, QString dirPath, Qt::DropAction action) {
    if (action == Qt::CopyAction) {
        emit copyUrlsRequested(paths, dirPath);
    } else if (action == Qt::MoveAction) {
        emit moveUrlsRequested(paths, dirPath);
    }
}


void FolderView::onThumbnailSizeChanged(int newSize) {
    int sliderValue = std::round(std::log2(newSize) * 100.0);
    if (zoomSlider->value() != sliderValue) {
        zoomSlider->blockSignals(true);
        zoomSlider->setValue(sliderValue);
        zoomSlider->blockSignals(false);
    }
    if (!zoomSlider->isSliderDown()) {
        settings->setFolderViewIconSize(newSize);
    }
}

void FolderView::onZoomSliderValueChanged(int value) {
    if (std::abs(value - 800) <= 8) {
        value = 800;
        if (zoomSlider->value() != 800) {
            zoomSlider->blockSignals(true);
            zoomSlider->setValue(800);
            zoomSlider->blockSignals(false);
        }
    }
    int newSize = std::round(std::pow(2.0, value / 100.0));
    thumbnailGrid->setThumbnailSize(newSize);
}

// changed by user via combobox
void FolderView::onSortingSelected(int mode) {
    emit sortingSelected(static_cast<SortingMode>(mode));
}

void FolderView::onSortingChanged(SortingMode mode) {
    sortingComboBox->blockSignals(true);
    sortingComboBox->setCurrentIndex(static_cast<int>(mode));
    sortingComboBox->blockSignals(false);
}

void FolderView::onFolderSortingSelected(int mode) {
    emit folderSortingSelected(static_cast<SortingMode>(mode));
}

void FolderView::onFolderSortingChanged(SortingMode mode) {
    folderSortingComboBox->blockSignals(true);
    folderSortingComboBox->setCurrentIndex(static_cast<int>(mode));
    folderSortingComboBox->blockSignals(false);
}

void FolderView::refreshFilesystemModel(const QString &path) {
    if (!dirModel)
        return;

    dirModel->refreshPath(path);
    dirTreeView->viewport()->update();
}

FolderView::~FolderView() {
    dirTreeView->setModel(nullptr);
}

// probably unneeded
void FolderView::show() {
    QWidget::show();
    thumbnailGrid->setFocus();
}

// probably unneeded
void FolderView::hide() {
    QWidget::hide();
    thumbnailGrid->clearFocus();
}

void FolderView::onFullscreenModeChanged(bool mode) {
    if(mode) // hide caption-control spacing
        panelRightEdgeSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    else // restore caption-control spacing
        panelRightEdgeSpacer->changeSize(
            kCaptionControlSpacingPx, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    topBar->layout()->invalidate();
}

void FolderView::focusInEvent(QFocusEvent *event) {
    Q_UNUSED(event)
    thumbnailGrid->setFocus();
}

void FolderView::populate(int count) {
    thumbnailGrid->populate(count);
}

void FolderView::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    thumbnailGrid->setThumbnail(pos, thumb);
}

void FolderView::select(QList<int> indices) {
    thumbnailGrid->select(indices);
}

void FolderView::select(int index) {
    thumbnailGrid->select(index);
}

QList<int> FolderView::selection() {
    return thumbnailGrid->selection();
}

void FolderView::focusOn(int index) {
    thumbnailGrid->focusOn(index);
}

void FolderView::focusOnSelection() {
    thumbnailGrid->focusOnSelection();
}

void FolderView::onHomeBtn() {
    emit directorySelected(QDir::homePath());
}

void FolderView::setDirectoryPath(QString path) {
    pathLabel->setText(path);

    if(dirTreeView->currentIndex().data() == path)
        return;

    bookmarksWidget->onPathChanged(path);

    QModelIndex targetIndex = dirModel->index(path);
    bool keepExpand = dirTreeView->isExpanded(targetIndex);
    bool collapse = !dirTreeView->isExpanded(dirTreeView->currentIndex().parent());

    if(collapse)
        dirTreeView->collapseAll();
    dirTreeView->setCurrentIndex(targetIndex);

    if(keepExpand)
        dirTreeView->expand(targetIndex);

    if (!dirModel->canFetchMore(targetIndex)) {
        dirTreeView->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
        m_pendingScrollPath.clear();
    } else {
        m_pendingScrollPath = path;
    }
}

void FolderView::onTreeViewClicked(QModelIndex index) {
    emit directorySelected(dirModel->fileInfo(index).absoluteFilePath());
}

void FolderView::onBookmarkClicked(QString dirPath) {
    emit directorySelected(dirPath);
}

void FolderView::newBookmark() {
    QFileDialog dialog;
    QString currentPath = pathLabel->text();
    dialog.setDirectory(QDir(currentPath).exists() ? currentPath : QDir::homePath());
    dialog.setWindowTitle("Select directory");
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    dialog.setOption(QFileDialog::DontResolveSymlinks);
    connect(&dialog, &QFileDialog::fileSelected, bookmarksWidget, &BookmarksWidget::addBookmark);
    dialog.exec();
}

void FolderView::addItem() {
    thumbnailGrid->addItem();
}

void FolderView::insertItem(int index) {
    thumbnailGrid->insertItem(index);
}

void FolderView::removeItem(int index) {
    thumbnailGrid->removeItem(index);
}

void FolderView::reloadItem(int index) {
    thumbnailGrid->reloadItem(index);
}

void FolderView::setDragHover(int index) {
    thumbnailGrid->setDragHover(index);
}

// prevent passthrough to parent
void FolderView::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void FolderView::paintEvent(QPaintEvent *) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void FolderView::resizeEvent(QResizeEvent *event) {
    FloatingWidgetContainer::resizeEvent(event);
    if(width() < 600)
        placesPanel->setVisible(false);
    else if (togglePlacesPanelButton->isChecked())
        placesPanel->setVisible(true);

    if(width() < 510) {
        zoomSlider->setVisible(false);
        gridSizeLabel->setVisible(false);
        zoomSliderSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        pathbarSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        topBar->layout()->invalidate();
    } else {
        zoomSlider->setVisible(true);
        gridSizeLabel->setVisible(true);
        zoomSliderSpacer->changeSize(3, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        pathbarSpacer->changeSize(12, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        topBar->layout()->invalidate();
    }
}

bool FolderView::eventFilter(QObject *watched, QEvent *event) {
    if(watched == bookmarksLabel || watched == newBookmarkButton) {
        if(event->type() == QEvent::DragEnter) {
            auto *dragEnterEvent = static_cast<QDragEnterEvent*>(event);
            if(dragEnterEvent->mimeData()->hasUrls()) {
                dragEnterEvent->acceptProposedAction();
                return true;
            }
        } else if(event->type() == QEvent::DragMove) {
            auto *dragMoveEvent = static_cast<QDragMoveEvent*>(event);
            if(dragMoveEvent->mimeData()->hasUrls()) {
                dragMoveEvent->acceptProposedAction();
                return true;
            }
        } else if(event->type() == QEvent::Drop) {
            auto *dropEvent = static_cast<QDropEvent*>(event);
            if(dropEvent->mimeData()->hasUrls()) {
                const auto urls = dropEvent->mimeData()->urls();
                bool accepted = false;
                for(const auto &url : urls) {
                    QString localPath = url.toLocalFile();
                    if(!localPath.isEmpty() && QFileInfo(localPath).isDir()) {
                        bookmarksWidget->addBookmark(localPath);
                        accepted = true;
                    }
                }
                if(accepted) {
                    dropEvent->acceptProposedAction();
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FolderView::onSelectionChanged() {
    int imageCount = 0;
    for(int index : selection()) {
        if(index >= dirCount) {
            imageCount++;
        }
    }

    if(imageCount > 0) {
        if(imageCount == 1) {
            selectionCountLabel->setText(tr("1 image selected"));
        } else {
            selectionCountLabel->setText(tr("%1 images selected").arg(imageCount));
        }
        selectionCountLabel->show();
        batchButton->show();
    } else {
        selectionCountLabel->hide();
        batchButton->hide();
    }
}

void FolderView::setDirCount(int count) {
    dirCount = count;
    onSelectionChanged();
}

void FolderView::onBatchClicked() {
    emit batchRequested();
}
