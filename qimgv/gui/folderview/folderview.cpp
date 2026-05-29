#include "folderview.h"
#include "ui_folderview.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>

FolderView::FolderView(QWidget *parent) :
    FloatingWidgetContainer(parent),
    ui(new Ui::FolderView)
{
    ui->setupUi(this);

    // ------- filesystem view --------
    QString style = "font: %1pt;";
    style = style.arg(QApplication::font().pointSize());
    ui->dirTreeView->setStyleSheet(style);


    dirModel = new FileSystemModelCustom(this);
    dirModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    ui->dirTreeView->setModel(dirModel);

    QHeaderView* header = ui->dirTreeView->header();
    header->hideSection(1); // size
    header->hideSection(2); // type
    header->hideSection(3); // mod date

    dirModel->setRootPath("");
    // -------------------------------
    ui->upButton->setAction("goUp");
    ui->upButton->setIconPath(":res/icons/common/buttons/panel/up16.png");
    ui->upButton->setTriggerMode(TriggerMode::ClickTrigger);
    ui->settingsButton->setAction("openSettings");
    ui->settingsButton->setIconPath(":res/icons/common/buttons/panel/settings16.png");
    ui->exitButton->setAction("exit");
    ui->exitButton->setIconPath(":res/icons/common/buttons/panel/close16.png");
    ui->exitButton->setIconOffset(-1, 0);
    ui->docViewButton->setAction("documentView");
    ui->docViewButton->setIconPath(":res/icons/common/buttons/panel/document-view20.png");
    ui->togglePlacesPanelButton->setCheckable(true);
    ui->togglePlacesPanelButton->setIconPath(":res/icons/common/buttons/panel/toggle-panel20.png");
    ui->togglePlacesPanelButton->setIconOffset(1, 0);


    ui->sortingComboBox->setIconPath(":res/icons/common/other/sorting-mode16.png");
    ui->folderSortingComboBox->setIconPath(":/res/icons/common/menuitem/document-view16.png");

    ui->newBookmarkButton->setIconPath(":res/icons/common/buttons/panel-small/add-new12.png");
    ui->homeButton->setIconPath(":res/icons/common/buttons/panel-small/home12.png");
    ui->rootButton->setIconPath(":res/icons/common/buttons/panel-small/root12.png");

    ui->bookmarksLabel->setAcceptDrops(true);
    ui->newBookmarkButton->setAcceptDrops(true);
    ui->bookmarksLabel->installEventFilter(this);
    ui->newBookmarkButton->installEventFilter(this);

    int min = ui->thumbnailGrid->THUMBNAIL_SIZE_MIN;
    int max = ui->thumbnailGrid->THUMBNAIL_SIZE_MAX;
    int step = ui->thumbnailGrid->ZOOM_STEP;

    ui->zoomSlider->setMinimum(min / step);
    ui->zoomSlider->setMaximum(max / step);
    ui->zoomSlider->setSingleStep(1);
    ui->zoomSlider->setPageStep(1);

    ui->splitter->setStretchFactor(1, 50);

    connect(ui->thumbnailGrid, &FolderGridView::thumbnailsRequested,  this, &FolderView::thumbnailsRequested);
    connect(ui->thumbnailGrid, &FolderGridView::thumbnailSizeChanged, this, &FolderView::onThumbnailSizeChanged);
    connect(ui->thumbnailGrid, &FolderGridView::itemActivated,   this, &FolderView::itemActivated);
    connect(ui->thumbnailGrid, &FolderGridView::draggedOut,      this, &FolderView::draggedOut);
    connect(ui->thumbnailGrid, &FolderGridView::draggedOver,     this, &FolderView::draggedOver);
    connect(ui->thumbnailGrid, &FolderGridView::droppedInto,     this, &FolderView::droppedInto);
    connect(ui->thumbnailGrid, &FolderGridView::backRequested,    this, &FolderView::backRequested);
    connect(ui->thumbnailGrid, &FolderGridView::forwardRequested, this, &FolderView::forwardRequested);

    connect(ui->bookmarksWidget, &BookmarksWidget::bookmarkClicked, this, &FolderView::onBookmarkClicked);

    connect(ui->newBookmarkButton, &IconButton::clicked, this, &FolderView::newBookmark);
    connect(ui->homeButton, &IconButton::clicked, this, &FolderView::onHomeBtn);
    connect(ui->rootButton, &IconButton::clicked, this, &FolderView::onRootBtn);

    connect(ui->zoomSlider, &QSlider::valueChanged, this, &FolderView::onZoomSliderValueChanged);
    connect(ui->sortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onSortingSelected);
    connect(ui->folderSortingComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &FolderView::onFolderSortingSelected);
    connect(ui->togglePlacesPanelButton, &ActionButton::toggled, this, &FolderView::onPlacesPanelButtonChecked);


    connect(ui->dirTreeView, &TreeViewCustom::droppedIn, this, &FolderView::onDroppedInByIndex);
    connect(ui->dirTreeView, &TreeViewCustom::tabbedOut, this, &FolderView::onTreeViewTabOut);
    connect(ui->bookmarksWidget, &BookmarksWidget::droppedIn, this, &FolderView::moveUrlsRequested); // ask what to do via popup? copy or move

    ui->sortingComboBox->setItemDelegate(new QStyledItemDelegate(ui->sortingComboBox));
    ui->sortingComboBox->view()->setTextElideMode(Qt::ElideNone);
    ui->folderSortingComboBox->setItemDelegate(new QStyledItemDelegate(ui->folderSortingComboBox));
    ui->folderSortingComboBox->view()->setTextElideMode(Qt::ElideNone);

    connect(ui->splitter, &QSplitter::splitterMoved, this, &FolderView::onSplitterMoved);

    readSettings();

    QSizePolicy sp_retain = sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    setSizePolicy(sp_retain);
    connect(settings, &Settings::settingsChanged, this, &FolderView::readSettings);

    ui->selectionCountLabel->hide();
    ui->batchButton->hide();

    connect(ui->thumbnailGrid, &FolderGridView::selectionChanged, this, &FolderView::onSelectionChanged);
    connect(ui->batchButton, &QPushButton::clicked, this, &FolderView::onBatchClicked);

    hide();
}

void FolderView::readSettings() {
    int currentRes = settings->thumbnailResolution();
    if (currentRes != lastThumbnailResolution) {
        ui->thumbnailGrid->unloadAllThumbnails();
        lastThumbnailResolution = currentRes;
    }
    ui->thumbnailGrid->setThumbnailSize(settings->folderViewIconSize());
    ui->thumbnailGrid->setShowLabels(true);
    ui->togglePlacesPanelButton->setChecked(settings->placesPanel());

    ui->folderSortingComboBox->blockSignals(true);
    ui->folderSortingComboBox->setCurrentIndex(static_cast<int>(settings->folderIconSortingMode()));
    ui->folderSortingComboBox->blockSignals(false);

    setPlacesPanel(settings->placesPanel());
    ui->bookmarksWidget->setVisible(settings->placesPanelBookmarksExpanded());
    ui->dirTreeView->setVisible(settings->placesPanelTreeExpanded());

    QList<int> sizes;
    sizes << settings->placesPanelWidth() << 1;
    ui->splitter->setSizes(sizes);
}

void FolderView::onSplitterMoved() {
    settings->setPlacesPanelWidth(ui->placesPanel->width());
}

void FolderView::onPlacesPanelButtonChecked(bool mode) {
    setPlacesPanel(mode);
    settings->setPlacesPanel(mode);
}

void FolderView::setPlacesPanel(bool mode) {
    if(width() >= 600)
        ui->placesPanel->setVisible(mode);
}

void FolderView::toggleBookmarks() {
    if(ui->bookmarksWidget->isVisible())
        ui->bookmarksWidget->hide();
    else
        ui->bookmarksWidget->show();
    settings->setPlacesPanelBookmarksExpanded(ui->bookmarksWidget->isVisible());
}

void FolderView::toggleFilesystemView() {
    if(ui->dirTreeView->isVisible())
        ui->dirTreeView->hide();
    else
        ui->dirTreeView->show();
    settings->setPlacesPanelTreeExpanded(ui->dirTreeView->isVisible());
}

void FolderView::onTreeViewTabOut() {
    ui->thumbnailGrid->setFocus();
    // TODO: maybe add a focus change indication? a border blink or something
}

// TODO: ask what to do
void FolderView::onDroppedInByIndex(QList<QString> paths, QModelIndex index) {
    emit moveUrlsRequested(paths, dirModel->filePath(index));
}


void FolderView::onThumbnailSizeChanged(int newSize) {
    ui->zoomSlider->setValue(newSize / ui->thumbnailGrid->ZOOM_STEP);
    settings->setFolderViewIconSize(newSize);
}

void FolderView::onZoomSliderValueChanged(int value) {
    ui->thumbnailGrid->setThumbnailSize(value * ui->thumbnailGrid->ZOOM_STEP);
}

// changed by user via combobox
void FolderView::onSortingSelected(int mode) {
    emit sortingSelected(static_cast<SortingMode>(mode));
}

void FolderView::onSortingChanged(SortingMode mode) {
    ui->sortingComboBox->blockSignals(true);
    ui->sortingComboBox->setCurrentIndex(static_cast<int>(mode));
    ui->sortingComboBox->blockSignals(false);
}

void FolderView::onFolderSortingSelected(int mode) {
    emit folderSortingSelected(static_cast<SortingMode>(mode));
}

void FolderView::onFolderSortingChanged(SortingMode mode) {
    ui->folderSortingComboBox->blockSignals(true);
    ui->folderSortingComboBox->setCurrentIndex(static_cast<int>(mode));
    ui->folderSortingComboBox->blockSignals(false);
}

FolderView::~FolderView() {
    ui->dirTreeView->setModel(nullptr);
    delete ui;
}

// probably unneeded
void FolderView::show() {
    QWidget::show();
    ui->thumbnailGrid->setFocus();
}

// probably unneeded
void FolderView::hide() {
    QWidget::hide();
    ui->thumbnailGrid->clearFocus();
}

void FolderView::onFullscreenModeChanged(bool mode) {
    ui->exitButton->setHidden(!mode);
    if(mode) // hide 2px spacer
        ui->panelRightEdgeSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    else // show spacer
        ui->panelRightEdgeSpacer->changeSize(2, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->topBar->layout()->invalidate();
}

void FolderView::focusInEvent(QFocusEvent *event) {
    Q_UNUSED(event)
    ui->thumbnailGrid->setFocus();
}

void FolderView::populate(int count) {
    ui->thumbnailGrid->populate(count);
}

void FolderView::setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) {
    ui->thumbnailGrid->setThumbnail(pos, thumb);
}

void FolderView::select(QList<int> indices) {
    ui->thumbnailGrid->select(indices);
}

void FolderView::select(int index) {
    ui->thumbnailGrid->select(index);
}

QList<int> FolderView::selection() {
    return ui->thumbnailGrid->selection();
}

void FolderView::focusOn(int index) {
    ui->thumbnailGrid->focusOn(index);
}

void FolderView::focusOnSelection() {
    ui->thumbnailGrid->focusOnSelection();
}

void FolderView::onHomeBtn() {
    emit directorySelected(QDir::homePath());
}

void FolderView::onRootBtn() {
    emit directorySelected("/");
}

void FolderView::setDirectoryPath(QString path) {
    ui->pathLabel->setText(path);

    if(ui->dirTreeView->currentIndex().data() == path)
        return;

    ui->bookmarksWidget->onPathChanged(path);

    QModelIndex targetIndex = dirModel->index(path);
    bool keepExpand = ui->dirTreeView->isExpanded(targetIndex);
    bool collapse = !ui->dirTreeView->isExpanded(ui->dirTreeView->currentIndex().parent());

    if(collapse)
        ui->dirTreeView->collapseAll();
    ui->dirTreeView->setCurrentIndex(targetIndex);

    if(keepExpand)
        ui->dirTreeView->expand(targetIndex);

    // ok, i'm done with this shit. none of the "solutions" work
    // just do scrollTo after a delay and hope that model is loaded by then
    // larger than ~150ms becomes too noticeable
    QTimer::singleShot(150, this, &FolderView::fsTreeScrollToCurrent);
}

void FolderView::fsTreeScrollToCurrent() {
    ui->dirTreeView->scrollTo(ui->dirTreeView->currentIndex());
}

void FolderView::onTreeViewClicked(QModelIndex index) {
    emit directorySelected(dirModel->fileInfo(index).absoluteFilePath());
}

void FolderView::onBookmarkClicked(QString dirPath) {
    emit directorySelected(dirPath);
}

void FolderView::newBookmark() {
    QFileDialog dialog;
    dialog.setDirectory(QDir::homePath());
    dialog.setWindowTitle("Select directory");
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);
    dialog.setOption(QFileDialog::DontResolveSymlinks);
    connect(&dialog, &QFileDialog::fileSelected, ui->bookmarksWidget, &BookmarksWidget::addBookmark);
    dialog.exec();
}

void FolderView::addItem() {
    ui->thumbnailGrid->addItem();
}

void FolderView::insertItem(int index) {
    ui->thumbnailGrid->insertItem(index);
}

void FolderView::removeItem(int index) {
    ui->thumbnailGrid->removeItem(index);
}

void FolderView::reloadItem(int index) {
    ui->thumbnailGrid->reloadItem(index);
}

void FolderView::setDragHover(int index) {
    ui->thumbnailGrid->setDragHover(index);
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
    Q_UNUSED(event)
    if(width() < 600)
        ui->placesPanel->setVisible(false);
    else if (ui->togglePlacesPanelButton->isChecked())
        ui->placesPanel->setVisible(true);

    if(width() < 510) {
        ui->zoomSlider->setVisible(false);
        ui->zoomSliderSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->pathbarSpacer->changeSize(0, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->topBar->layout()->invalidate();
    } else {
        ui->zoomSlider->setVisible(true);
        ui->zoomSliderSpacer->changeSize(3, 20, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->pathbarSpacer->changeSize(12, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        ui->topBar->layout()->invalidate();
    }
}

bool FolderView::eventFilter(QObject *watched, QEvent *event) {
    if(watched == ui->bookmarksLabel || watched == ui->newBookmarkButton) {
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
                        ui->bookmarksWidget->addBookmark(localPath);
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
            ui->selectionCountLabel->setText(tr("1 image selected"));
        } else {
            ui->selectionCountLabel->setText(tr("%1 images selected").arg(imageCount));
        }
        ui->selectionCountLabel->show();
        ui->batchButton->show();
    } else {
        ui->selectionCountLabel->hide();
        ui->batchButton->hide();
    }
}

void FolderView::setDirCount(int count) {
    dirCount = count;
    onSelectionChanged();
}

void FolderView::onBatchClicked() {
    // Placeholder for batch convert
}
