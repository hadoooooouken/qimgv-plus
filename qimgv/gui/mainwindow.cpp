#include "mainwindow.h"
#include "settings.h"
#include <QClipboard>
#include <QApplication>

namespace {
constexpr int MIN_WINDOW_WIDTH = 256;
constexpr int MIN_WINDOW_HEIGHT = 256;
}



MW::MW(QWidget *parent)
    : FloatingWidgetContainer(parent),
      currentDisplay(0),
      maximized(false),
      activeSidePanel(SIDEPANEL_NONE),
      copyOverlay(nullptr),
      saveOverlay(nullptr),
      renameOverlay(nullptr),
      colorAdjustmentsOverlay(nullptr),
      casSettingsOverlay(nullptr),
      infoBarFullscreen(nullptr),
      imageInfoOverlay(nullptr),
      floatingMessage(nullptr),
      floatingMessageFolderView(nullptr),
      cropPanel(nullptr),
      cropOverlay(nullptr),
      panelPosition(PANEL_TOP),
      m_pseudoFullscreen(false)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);

    setMinimumSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);

    // do not steal focus when clicked
    // this is just a container. accept key events only
    // via passthrough from child widgets
    setFocusPolicy(Qt::NoFocus);

    this->setLayout(&layout);

    setWindowTitle(QCoreApplication::applicationName() + " " +
                   QCoreApplication::applicationVersion());

    this->setMouseTracking(true);
    this->setAcceptDrops(true);
    this->setAccessibleName("mainwindow");
    windowGeometryChangeTimer.setSingleShot(true);
    windowGeometryChangeTimer.setInterval(30);
    setupUi();

    connect(settings, &Settings::settingsChanged, this, &MW::readSettings);
    connect(&windowGeometryChangeTimer, &QTimer::timeout, this, &MW::onWindowGeometryChanged);
    connect(this, &MW::fullscreenStateChanged, this, &MW::adaptToWindowState);

    readSettings();
    currentDisplay = settings->lastDisplay();
    maximized = settings->maximizedWindow();
    restoreWindowGeometry();
}

MW::~MW() {
    if (floatingMessage) {
        delete floatingMessage;
    }
    if (floatingMessageFolderView) {
        delete floatingMessageFolderView;
    }
}

/*                                                             |--[ImageViewer]
 *                        |--[DocumentWidget]--[ViewerWidget]--|
 * [MW]--[CentralWidget]--|
 *                        |--[FolderView]
 *
 *  (not counting floating widgets)
 *  ViewerWidget exists for input handling reasons (correct overlay hover handling)
 */
void MW::setupUi() {
    viewerWidget.reset(new ViewerWidget(this));
    docWidget.reset(new DocumentWidget(viewerWidget));
    folderView.reset(new FolderViewProxy(this));
    connect(folderView.get(), &FolderViewProxy::sortingSelected, this, &MW::sortingSelected);
    connect(folderView.get(), &FolderViewProxy::folderSortingSelected, this, &MW::folderSortingSelected);
    connect(folderView.get(), &FolderViewProxy::directorySelected, this, &MW::opened);
    connect(folderView.get(), &FolderViewProxy::copyUrlsRequested, this, &MW::copyUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::moveUrlsRequested, this, &MW::moveUrlsRequested);
    connect(folderView.get(), &FolderViewProxy::showFoldersChanged, this, &MW::showFoldersChanged);
    connect(folderView.get(), &FolderViewProxy::batchRequested, this, &MW::batchRequested);

    centralWidget.reset(new CentralWidget(docWidget, folderView, this));
    layout.addWidget(centralWidget.get());
    controlsOverlay = new ControlsOverlay(docWidget.get());
    infoBarFullscreen = new FullscreenInfoOverlayProxy(viewerWidget.get());
    sidePanel = new SidePanel(this);
    layout.addWidget(sidePanel);
    imageInfoOverlay = new ImageInfoOverlayProxy(viewerWidget.get());
    floatingMessage = new FloatingMessageProxy(viewerWidget.get());
    connect(viewerWidget.get(), &ViewerWidget::scalingRequested, this, &MW::scalingRequested);
    connect(viewerWidget.get(), &ViewerWidget::draggedOut,       this, &MW::draggedOut);
    connect(viewerWidget.get(), &ViewerWidget::nextImageRequested, this, &MW::nextImageRequested);
    connect(viewerWidget.get(), &ViewerWidget::prevImageRequested, this, &MW::prevImageRequested);
    connect(viewerWidget.get(), &ViewerWidget::showScriptSettings, this, &MW::showScriptSettings);
    connect(viewerWidget.get(), &ViewerWidget::scaleChanged, this, &MW::onScaleChanged);
    connect(this, &MW::zoomIn,        viewerWidget.get(), &ViewerWidget::zoomIn);
    connect(this, &MW::zoomOut,       viewerWidget.get(), &ViewerWidget::zoomOut);
    connect(this, &MW::zoomInCursor,  viewerWidget.get(), &ViewerWidget::zoomInCursor);
    connect(this, &MW::zoomOutCursor, viewerWidget.get(), &ViewerWidget::zoomOutCursor);
    connect(this, &MW::scrollUp,    viewerWidget.get(), &ViewerWidget::scrollUp);
    connect(this, &MW::scrollDown,  viewerWidget.get(), &ViewerWidget::scrollDown);
    connect(this, &MW::scrollLeft,  viewerWidget.get(), &ViewerWidget::scrollLeft);
    connect(this, &MW::scrollRight, viewerWidget.get(), &ViewerWidget::scrollRight);
    connect(this, &MW::toggleTransparencyGrid, viewerWidget.get(), &ViewerWidget::toggleTransparencyGrid);
}

void MW::setupFullUi() {
    setupCropPanel();
    docWidget->allowPanelInit();
    docWidget->setupMainPanel();
    infoBarFullscreen->init();
}

void MW::setupCropPanel() {
    if(cropPanel)
        return;
    cropOverlay = new CropOverlay(viewerWidget.get());
    cropPanel = new CropPanel(cropOverlay, this);
    connect(cropPanel, &CropPanel::cancel, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::crop,   this, &MW::cropRequested);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::hideCropPanel);
    connect(cropPanel, &CropPanel::cropAndSave, this, &MW::cropAndSaveRequested);
}

void MW::setupCopyOverlay() {
    copyOverlay = new CopyOverlay(viewerWidget.get());
    connect(copyOverlay, &CopyOverlay::copyRequested, this, &MW::copyRequested);
    connect(copyOverlay, &CopyOverlay::moveRequested, this, &MW::moveRequested);
}

void MW::setupSaveOverlay() {
    saveOverlay = new SaveConfirmOverlay(viewerWidget.get());
    connect(saveOverlay, &SaveConfirmOverlay::saveClicked,    this, &MW::saveRequested);
    connect(saveOverlay, &SaveConfirmOverlay::saveAsClicked,  this, &MW::saveAsClicked);
    connect(saveOverlay, &SaveConfirmOverlay::discardClicked, this, &MW::discardEditsRequested);
}

void MW::setupRenameOverlay() {
    renameOverlay = new RenameOverlay(this);
    renameOverlay->setName(info.fileName);
    connect(renameOverlay, &RenameOverlay::renameRequested, this, &MW::renameRequested);
}

void MW::toggleFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    if(colorAdjustmentsOverlay)
        colorAdjustmentsOverlay->hide();
    if(casSettingsOverlay)
        casSettingsOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->toggleViewMode();
    onInfoUpdated();
}

void MW::enableFolderView() {
    hideCropPanel();
    if(copyOverlay)
        copyOverlay->hide();
    if(renameOverlay)
        renameOverlay->hide();
    if(colorAdjustmentsOverlay)
        colorAdjustmentsOverlay->hide();
    if(casSettingsOverlay)
        casSettingsOverlay->hide();
    docWidget->hideFloatingPanel();
    imageInfoOverlay->hide();
    centralWidget->showFolderView();
    onInfoUpdated();
}

void MW::enableDocumentView() {
    centralWidget->showDocumentView();
    onInfoUpdated();
}

ViewMode MW::currentViewMode() {
    return centralWidget->currentViewMode();
}

void MW::fitWindow() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindow();
    } else {
        showMessage(tr("Zoom temporary disabled"));
    }
}

void MW::fitWidth() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWidth();
    } else {
        showMessage(tr("Zoom temporary disabled"));
    }
}

void MW::fitOriginal() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitOriginal();
    } else {
        showMessage(tr("Zoom temporary disabled"));
    }
}

void MW::fitWindowStretch() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->fitWindowStretch();
    } else {
        showMessage(tr("Zoom temporary disabled"));
    }
}

// switch between 1:1 and Fit All
void MW::switchFitMode() {
    if(viewerWidget->interactionEnabled()) {
        viewerWidget->switchFitMode();
    } else {
        showMessage(tr("Zoom temporary disabled"));
    }
}

void MW::closeImage() {
    info.fileName = "";
    info.filePath = "";
    viewerWidget->closeImage();
}

void MW::preShowResize(QSize sz) {
    auto screens = qApp->screens();
    if(this->windowState() != Qt::WindowNoState || !screens.count() || screens.count() <= currentDisplay)
        return;
    int decorationSize = frameGeometry().height() - height();
    float maxSzMulti = settings->autoResizeLimit() / 100.f;
    QRect availableGeom = screens.at(currentDisplay)->availableGeometry();
    QSize maxSz = availableGeom.size() * maxSzMulti;
    maxSz.setHeight(maxSz.height() - decorationSize);
    if(!sz.isEmpty()) {
        if(sz.width() > maxSz.width() || sz.height() > maxSz.height())
            sz.scale(maxSz, Qt::KeepAspectRatio);
    } else {
        sz = maxSz;
    }
    QRect newGeom(0,0, sz.width(), sz.height());
    newGeom.moveCenter(availableGeom.center());
    newGeom.translate(0, decorationSize / 2);

    if(this->isVisible())
        setGeometry(newGeom);
    else // setGeometry wont work on hidden windows, so we just save for it to be restored later
        settings->setWindowGeometry(newGeom);
}

void MW::showImage(std::shared_ptr<const QImage> image, QString filePath) {
    if(settings->autoResizeWindow())
        preShowResize(image->size());
    viewerWidget->showImage(image, filePath);
    updateCropPanelData();
}

void MW::showAnimation(std::shared_ptr<QMovie> movie) {
    if(settings->autoResizeWindow())
        preShowResize(movie->frameRect().size());
    viewerWidget->showAnimation(movie);
    updateCropPanelData();
}


void MW::showContextMenu() {
    viewerWidget->showContextMenu();
}

void MW::onSortingChanged(SortingMode mode) {
    folderView.get()->onSortingChanged(mode);
    if(centralWidget.get()->currentViewMode() == ViewMode::MODE_DOCUMENT) {
        switch(mode) {
            case SortingMode::SORT_NAME:      showMessage(tr("Sorting: By Name"));              break;
            case SortingMode::SORT_NAME_DESC: showMessage(tr("Sorting: By Name (desc.)"));      break;
            case SortingMode::SORT_TIME:      showMessage(tr("Sorting: By Time"));              break;
            case SortingMode::SORT_TIME_DESC: showMessage(tr("Sorting: By Time (desc.)"));      break;
            case SortingMode::SORT_SIZE:      showMessage(tr("Sorting: By File Size"));         break;
            case SortingMode::SORT_SIZE_DESC: showMessage(tr("Sorting: By File Size (desc.)")); break;
        }
    }
}

void MW::onFolderSortingChanged(SortingMode mode) {
    folderView.get()->onFolderSortingChanged(mode);
    if(centralWidget.get()->currentViewMode() == ViewMode::MODE_FOLDERVIEW) {
        switch(mode) {
            case SortingMode::SORT_NAME:      showMessage(tr("Folder Thumbnails: By Name"));              break;
            case SortingMode::SORT_NAME_DESC: showMessage(tr("Folder Thumbnails: By Name (desc.)"));      break;
            case SortingMode::SORT_TIME:      showMessage(tr("Folder Thumbnails: Oldest"));               break;
            case SortingMode::SORT_TIME_DESC: showMessage(tr("Folder Thumbnails: Newest"));               break;
            case SortingMode::SORT_SIZE:      showMessage(tr("Folder Thumbnails: By File Size"));         break;
            case SortingMode::SORT_SIZE_DESC: showMessage(tr("Folder Thumbnails: By File Size (desc.)")); break;
        }
    }
}

void MW::setDirectoryPath(QString path) {
    //closeImage();
    info.directoryPath = path;
    info.directoryName = path.split("/").last();
    folderView->setDirectoryPath(path);
    onInfoUpdated();
}

void MW::toggleLockZoom() {
    viewerWidget->toggleLockZoom();
    if(viewerWidget->lockZoomEnabled())
        showMessage(tr("Zoom lock: ON"));
    else
        showMessage(tr("Zoom lock: OFF"));
    onInfoUpdated();
}

void MW::toggleLockView() {
    viewerWidget->toggleLockView();
    if(viewerWidget->lockViewEnabled())
        showMessage(tr("View lock: ON"));
    else
        showMessage(tr("View lock: OFF"));
    onInfoUpdated();
}

void MW::toggleFullscreenInfoBar() {
    if(this->isFullScreen()) {
        showInfoBarFullscreen = !showInfoBarFullscreen;
        settings->setInfoBarFullscreen(showInfoBarFullscreen);
        if(showInfoBarFullscreen)
            infoBarFullscreen->showWhenReady();
        else
            infoBarFullscreen->hide();
    }
}

void MW::toggleImageInfoOverlay() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(imageInfoOverlay->isHidden())
        imageInfoOverlay->show();
    else
        imageInfoOverlay->hide();
}

void MW::toggleRenameOverlay(QString currentName) {
    if(!renameOverlay)
        setupRenameOverlay();
    if(renameOverlay->isHidden()) {
        renameOverlay->setBackdropEnabled((centralWidget->currentViewMode() == MODE_FOLDERVIEW));
        renameOverlay->setName(currentName);
        renameOverlay->show();
    } else {
        renameOverlay->hide();
    }
}

void MW::toggleColorAdjustments() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(!colorAdjustmentsOverlay) {
        colorAdjustmentsOverlay = new ColorAdjustmentsOverlayProxy(viewerWidget.get());
        connect(colorAdjustmentsOverlay, &ColorAdjustmentsOverlayProxy::adjustmentsChanged,
                this, [this](float exp, float c, float b, float temp, float tint, float s, float h) {
            viewerWidget->setColorAdjustments(exp, c, b, temp, tint, s, h);
        });
        connect(colorAdjustmentsOverlay, &ColorAdjustmentsOverlayProxy::applyRequested,
                this, &MW::colorAdjustmentsApplyRequested);
    }
    if(colorAdjustmentsOverlay->isHidden()) {
        colorAdjustmentsOverlay->setCustomPosition(QCursor::pos());
        colorAdjustmentsOverlay->show();
    } else {
        colorAdjustmentsOverlay->hide();
    }
}

void MW::toggleCasSettings() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(!casSettingsOverlay) {
        casSettingsOverlay = new CasSettingsOverlay(viewerWidget.get());
        connect(casSettingsOverlay, &CasSettingsOverlay::casSettingsChanged,
                this, [this](float sharpening, float contrast) {
            viewerWidget->updateCasSettings();
        });
    }
    if(casSettingsOverlay->isHidden()) {
        casSettingsOverlay->setCustomPosition(QCursor::pos());
        casSettingsOverlay->show();
    } else {
        casSettingsOverlay->hide();
    }
}


void MW::toggleScalingFilter() {
    ScalingFilter configuredFilter = settings->scalingFilter();
    if(viewerWidget->scalingFilter() == configuredFilter) {
        setFilterNearest();
    }
    else {
        setFilter(configuredFilter);
    }
}

void MW::cycleScalingFilter() {
    ScalingFilter currentFilter = viewerWidget->scalingFilter();
    int nextFilterInt = static_cast<int>(currentFilter) + 1;
    if (nextFilterInt > static_cast<int>(QI_FILTER_SMART_GPU)) {
        nextFilterInt = 0;
    }
    ScalingFilter nextFilter = static_cast<ScalingFilter>(nextFilterInt);
    setFilter(nextFilter);
}

void MW::setFilterNearest() {
    showMessage(tr("Filter: ") + tr("Nearest"), 600);
    viewerWidget->setFilterNearest();
}

void MW::setFilterBilinear() {
    showMessage(tr("Filter: ") + tr("Bilinear"), 600);
    viewerWidget->setFilterBilinear();
}

void MW::setFilter(ScalingFilter filter) {
    QString filterName;
    switch (filter) {
        case QI_FILTER_NEAREST:
            filterName = tr("Nearest");
            break;
        case ScalingFilter::QI_FILTER_BILINEAR:
            filterName = tr("Bilinear");
            break;
        case QI_FILTER_SMART:
            filterName = tr("Smart sharpen");
            break;
        case QI_FILTER_CAS:
            filterName = "FidelityFX-CAS (GPU)";
            break;
        case QI_FILTER_SMART_GPU:
            filterName = tr("Smart sharpen (GPU)");
            break;
        default:
            filterName = tr("Configured ") + QString::number(static_cast<int>(filter));
            break;
    }
    showMessage(tr("Filter: ") + filterName, 600);
    settings->setScalingFilter(filter);
    viewerWidget->setScalingFilter(filter);
}

#ifdef USE_UPSCAYL
void MW::toggleUpscayl() {
    bool current = settings->useUpscayl();
    settings->setUseUpscayl(!current);
    settings->sendChangeNotification();
    showMessage(settings->useUpscayl() ? tr("Use Upscayl: ON") : tr("Use Upscayl: OFF"), 600);
    if (!settings->useUpscayl()) {
        hideUpscaledCrop();
    }
}
#endif

bool MW::isCropPanelActive() {
    return (activeSidePanel == SIDEPANEL_CROP);
}

void MW::onScalingFinished(QImage scaled) {
    viewerWidget->onScalingFinished(scaled);
}

void MW::onUpscaleFinished(const QImage &cropImg, QRect origCrop) {
    viewerWidget->setUpscaledCrop(cropImg, origCrop);
}

void MW::hideUpscaledCrop() {
    if (viewerWidget) {
        viewerWidget->hideUpscaledCrop();
    }
}

QRect MW::visibleImageRect() const {
    if (viewerWidget) {
        return viewerWidget->visibleImageRect();
    }
    return QRect();
}

QRect MW::visibleOriginalImageRect() const {
    if (viewerWidget) {
        return viewerWidget->visibleOriginalImageRect();
    }
    return QRect();
}

QPixmap MW::currentScaledPixmapCopy() const {
    if (viewerWidget) {
        return viewerWidget->currentScaledPixmapCopy();
    }
    return QPixmap();
}

float MW::getDpr() const {
    if (viewerWidget) {
        return viewerWidget->getDpr();
    }
    return 1.0f;
}

float MW::currentScale() const {
    if (viewerWidget) {
        return viewerWidget->currentScale();
    }
    return 1.0f;
}

bool MW::isFullScreen() const {
    return m_pseudoFullscreen;
}

bool MW::panoramaMode() const {
    if (viewerWidget) {
        return viewerWidget->panoramaMode();
    }
    return false;
}

bool MW::isBusyInteracting() const {
    if (viewerWidget) {
        return viewerWidget->isBusyInteracting();
    }
    return false;
}


void MW::saveWindowGeometry() {
    if(this->windowState() == Qt::WindowNoState)
        settings->setWindowGeometry(geometry());
    settings->setMaximizedWindow(maximized);
}

// does not apply fullscreen; window size / maximized state only
void MW::restoreWindowGeometry() {
    this->setGeometry(settings->windowGeometry());
    if(settings->maximizedWindow())
        this->setWindowState(Qt::WindowMaximized);
    updateCurrentDisplay();
}

void MW::updateCurrentDisplay() {
    auto screens = qApp->screens();
    currentDisplay = screens.indexOf(this->window()->screen());
}

void MW::onWindowGeometryChanged() {
    saveWindowGeometry();
    updateCurrentDisplay();
}

void MW::saveCurrentDisplay() {
    settings->setLastDisplay(qApp->screens().indexOf(this->window()->screen()));
}

//#############################################################
//######################### EVENTS ############################
//#############################################################

void MW::mouseMoveEvent(QMouseEvent *event) {
    event->ignore();
}

bool MW::event(QEvent *event) {
    // only save maximized state if we are already visible
    // this filter out out events while the window is still being set up
    if(event->type() == QEvent::WindowStateChange && this->isVisible() && !this->isFullScreen())
        maximized = isMaximized();
    if(event->type() == QEvent::Move || event->type() == QEvent::Resize)
        windowGeometryChangeTimer.start();
    if(event->type() == QEvent::WindowDeactivate) {
        docWidget->hideFloatingPanel(true);
        if(viewerWidget)
            viewerWidget->hideContextMenu();
    }
    return QWidget::event(event);
}

// hook up to actionManager
void MW::keyPressEvent(QKeyEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::wheelEvent(QWheelEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mousePressEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    actionManager->processEvent(event);
}

void MW::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
    QMouseEvent *fakePressEvent = new QMouseEvent(
        QEvent::MouseButtonPress,
        event->pos(),
        event->button(),
        event->buttons(),
        event->modifiers()
    );
    actionManager->processEvent(fakePressEvent);
    actionManager->processEvent(event);
    delete fakePressEvent;
}

void MW::close() {
    saveWindowGeometry();
    saveCurrentDisplay();
    // try to close window sooner
    // since qt6.3 QWidget::close() no longer works on hidden windows (bug?)
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    this->hide();
#endif
    if(copyOverlay)
        copyOverlay->saveSettings();
    if(colorAdjustmentsOverlay) {
        delete colorAdjustmentsOverlay;
        colorAdjustmentsOverlay = nullptr;
    }
    if(casSettingsOverlay) {
        delete casSettingsOverlay;
        casSettingsOverlay = nullptr;
    }
    QWidget::close();
}

void MW::closeEvent(QCloseEvent *event) {
    // catch the close event when user presses X on the window itself
    event->accept();
    actionManager->invokeAction("exit");
}

void MW::dragEnterEvent(QDragEnterEvent *e) {
    if(e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
    }
}

void MW::dropEvent(QDropEvent *event) {
    emit droppedIn(event->mimeData(), event->source());
}

void MW::resizeEvent(QResizeEvent *event) {
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
    }
    FloatingWidgetContainer::resizeEvent(event);
}

void MW::showDefault() {
    if(!this->isVisible()) {
        if(settings->fullscreenMode())
            showFullScreen();
        else
            showWindowed();
    }
}

void MW::showSaveDialog(QString filePath) {
    QString newFilePath = getSaveFileName(filePath);
    if(!newFilePath.isEmpty())
        emit saveAsRequested(newFilePath);
}

QString MW::getSaveFileName(QString filePath) {
    docWidget->hideFloatingPanel();
    QStringList filters;
    // generate filter for writable images
    auto writerFormats = QImageWriter::supportedImageFormats();
    if(writerFormats.contains("jpg"))  filters.append("JPEG (*.jpg *.jpeg *jpe *jfif)");
    if(writerFormats.contains("png"))  filters.append("PNG (*.png)");
    if(writerFormats.contains("webp")) filters.append("WebP (*.webp)");
    if(writerFormats.contains("jxl"))  filters.append("JPEG-XL (*.jxl)");
    if(writerFormats.contains("avif")) filters.append("AVIF (*.avif *.avifs)");
    if(writerFormats.contains("qoi"))  filters.append("QOI (*.qoi)");
    if(writerFormats.contains("bmp"))  filters.append("BMP (*.bmp)");
    if(writerFormats.contains("tif"))  filters.append("TIFF (*.tif *.tiff)");
    QString filterString = filters.join(";; ");

    // find matching filter for the current image
    QString selectedFilter = "JPEG (*.jpg *.jpeg *jpe *jfif)";
    QFileInfo fi(filePath);
    for(const auto &filter : std::as_const(filters)) {
        if(filter.contains(fi.suffix().toLower())) {
            selectedFilter = filter;
            break;
        }
    }
    QString newFilePath = QFileDialog::getSaveFileName(this, tr("Save File as..."), filePath, filterString, &selectedFilter);
    return newFilePath;
}


void MW::showResizeDialog(QSize initialSize) {
    ResizeDialog dialog(initialSize, this);
    connect(&dialog, &ResizeDialog::sizeSelected, this, &MW::resizeRequested);
    dialog.exec();
}

void MW::showBatchConverter(const QList<QString> &paths) {
    BatchConverterDialog dialog(paths, this);
    dialog.exec();
}

DialogResult MW::fileReplaceDialog(QString src, QString dst, FileReplaceMode mode, bool multiple) {
    FileReplaceDialog dialog(this);
    dialog.setModal(true);
    dialog.setSource(src);
    dialog.setDestination(dst);
    dialog.setMode(mode);
    dialog.setMulti(multiple);

    dialog.exec();

    return dialog.getResult();
}

void MW::showSettings() {
    docWidget->hideFloatingPanel();
    if(viewerWidget)
        viewerWidget->hideContextMenu();
    SettingsDialog settingsDialog(this);
    settingsDialog.exec();
}

void MW::showScriptSettings() {
    docWidget->hideFloatingPanel();
    if(viewerWidget)
        viewerWidget->hideContextMenu();
    SettingsDialog settingsDialog(this);
    settingsDialog.switchToPage(4);
    settingsDialog.exec();
}

void MW::triggerFullScreen() {
    if(!isFullScreen()) {
        showFullScreen();
    } else {
        showWindowed();
    }
}

void MW::showFullScreen() {
    //do not save immediately on application start
    if(!isHidden())
        saveWindowGeometry();
    auto screens = qApp->screens();
    // When the app is launched directly in fullscreen mode on a multi-monitor setup,
    // it lacks initial geometry. This check ensures the window is moved to the
    // user's saved display (currentDisplay) before going fullscreen.
    int _currentDisplay = screens.indexOf(this->window()->screen());
    // move to target screen
    if(screens.count() > currentDisplay && currentDisplay != _currentDisplay) {
        this->move(screens.at(currentDisplay)->geometry().x(),
                   screens.at(currentDisplay)->geometry().y());
    }
    // Pseudo-fullscreen: borderless window spanning the screen
    this->setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint);
    this->setGeometry(screens.at(currentDisplay)->geometry());
    
    m_pseudoFullscreen = true;
    this->show();

    emit fullscreenStateChanged(true);
}

void MW::showWindowed() {
    this->setWindowFlags(this->windowFlags() & ~Qt::FramelessWindowHint);

    restoreWindowGeometry();
    m_pseudoFullscreen = false;
    this->show();
    emit fullscreenStateChanged(false);
}

void MW::updateCropPanelData() {
    if(cropPanel && activeSidePanel == SIDEPANEL_CROP) {
        cropPanel->setImageRealSize(viewerWidget->sourceSize());
        cropOverlay->setImageDrawRect(viewerWidget->imageRect());
        cropOverlay->setImageScale(viewerWidget->currentScale());
        cropOverlay->setImageRealSize(viewerWidget->sourceSize());
    }
}

void MW::showSaveOverlay() {
    if(!settings->showSaveOverlay())
        return;
    if(!saveOverlay)
        setupSaveOverlay();
    saveOverlay->show();
}

void MW::hideSaveOverlay() {
    if(!saveOverlay)
        return;
    saveOverlay->hide();
}

void MW::triggerCropPanel() {
    if(activeSidePanel != SIDEPANEL_CROP) {
        showCropPanel();
    } else {
        hideCropPanel();
    }
}

void MW::showCropPanel() {
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;

    if(activeSidePanel != SIDEPANEL_CROP) {
        docWidget->hideFloatingPanel();
        sidePanel->setWidget(cropPanel);
        sidePanel->show();
        cropOverlay->show();
        activeSidePanel = SIDEPANEL_CROP;
        // reset & lock zoom so CropOverlay won't go crazy
        viewerWidget->fitWindow();
        setInteractionEnabled(false);
        // feed the panel current image info
        updateCropPanelData();
    }
}

void MW::setInteractionEnabled(bool mode) {
    docWidget->setInteractionEnabled(mode);
    viewerWidget->setInteractionEnabled(mode);
}

void MW::hideCropPanel() {
    sidePanel->hide();
    if(activeSidePanel == SIDEPANEL_CROP) {
        cropOverlay->hide();
        setInteractionEnabled(true);
    }
    activeSidePanel = SIDEPANEL_NONE;
}

void MW::triggerCopyOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_COPY) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_COPY);
        copyOverlay->show();
    }
}

void MW::copyViewportToClipboard() {
    if (!viewerWidget->isDisplaying() || !viewerWidget->copyCurrentViewportToClipboard()) {
        showWarning(tr("No viewport image available to copy."));
        return;
    }
    showMessageSuccess(tr("Viewport image copied to clipboard"));
}

void MW::triggerMoveOverlay() {
    if(!viewerWidget->isDisplaying())
        return;
    if(!copyOverlay)
        setupCopyOverlay();

    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW)
        return;
    if(copyOverlay->operationMode() == OVERLAY_MOVE) {
        copyOverlay->isHidden() ? copyOverlay->show() : copyOverlay->hide();
    } else {
        copyOverlay->setDialogMode(OVERLAY_MOVE);
        copyOverlay->show();
    }
}

// quit fullscreen or exit the program
void MW::closeFullScreenOrExit() {
    if(this->isFullScreen()) {
        this->showWindowed();
    } else {
        actionManager->invokeAction("exit");
    }
}

void MW::setCurrentInfo(int _index, int _fileCount, QString _filePath, QString _fileName, QSize _imageSize, qint64 _fileSize, QString _format, QString _colorProfile, bool slideshow, bool shuffle, bool edited) {
    info.index = _index;
    info.fileCount = _fileCount;
    info.fileName = _fileName;
    info.filePath = _filePath;
    info.imageSize = _imageSize;
    info.fileSize = _fileSize;
    info.format = _format;
    info.colorProfile = _colorProfile;
    info.slideshow = slideshow;
    info.shuffle = shuffle;
    info.edited = edited;
    onInfoUpdated();
}

void MW::onInfoUpdated() {
    QString posString;
    if(info.fileCount)
        posString = "[ " + QString::number(info.index + 1) + "/" + QString::number(info.fileCount) + " ]";
    QString resString;
    if(info.imageSize.width()) {
        resString = QString::number(info.imageSize.width()) + " x " + QString::number(info.imageSize.height());
        int w = info.imageSize.width();
        int h = info.imageSize.height();
        if(w > 0 && h > 0) {
            int a = w, b = h;
            while(b != 0) {
                int t = b;
                b = a % b;
                a = t;
            }
            int gcd = a;
            resString += " (" + QString::number(w / gcd) + ":" + QString::number(h / gcd) + ")";
        }
    }
    QString sizeString;
    if(info.fileSize)
        sizeString = this->locale().formattedDataSize(info.fileSize, 1);
    QString formatString = info.format.toUpper();

    if(renameOverlay)
        renameOverlay->setName(info.fileName);

    QString windowTitle;
    if(centralWidget->currentViewMode() == MODE_FOLDERVIEW) {
        windowTitle = tr("Folder view");
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
    } else if(info.fileName.isEmpty()) {
        windowTitle = qApp->applicationName();
        infoBarFullscreen->setInfo("", tr("No file opened."), "");
    } else {
        windowTitle = info.fileName;
        int scalePercent = qRound(viewerWidget->currentScale() * 100.0f);
        windowTitle.append(QString(" [%1%]").arg(scalePercent));
        lastScalePercent = scalePercent;

        if(settings->windowTitleExtendedInfo()) {
            windowTitle.prepend(posString + "  ");
            if(!resString.isEmpty())
                windowTitle.append(" - " + resString);
            if(!info.colorProfile.isEmpty())
                windowTitle.append(" - " + info.colorProfile);
            if(!formatString.isEmpty())
                windowTitle.append(" - " + formatString);
            if(!sizeString.isEmpty())
                windowTitle.append(" - " + sizeString);
        }

        // toggleable states
        QString states;
        if(info.slideshow)
            states.append(" [slideshow]");
        if(info.shuffle)
            states.append(" [shuffle]");
        if(viewerWidget->lockZoomEnabled())
            states.append(" [zoom lock]");
        if(viewerWidget->lockViewEnabled())
            states.append(" [view lock]");

        if(!states.isEmpty())
            windowTitle.append(" -" + states);
        if(info.edited)
            windowTitle.prepend("* ");

        QString rightInfo = resString;
        if(!info.colorProfile.isEmpty()) {
            if(!rightInfo.isEmpty())
                rightInfo += "  " + info.colorProfile;
            else
                rightInfo = info.colorProfile;
        }
        if(!formatString.isEmpty()) {
            if(!rightInfo.isEmpty())
                rightInfo += "  " + formatString;
            else
                rightInfo = formatString;
        }
        if(!sizeString.isEmpty()) {
            if(!rightInfo.isEmpty())
                rightInfo += "  " + sizeString;
            else
                rightInfo = sizeString;
        }

        infoBarFullscreen->setInfo(posString, info.fileName + (info.edited ? "  *" : ""), rightInfo);
    }
    if(this->windowTitle() != windowTitle)
        setWindowTitle(windowTitle);
}

void MW::onScaleChanged(qreal scale) {
    int percent = qRound(scale * 100.0f);
    if(percent != lastScalePercent) {
        lastScalePercent = percent;
        onInfoUpdated();
    }
}

void MW::setExifInfo(QMap<QString, QString> info) {
    if(imageInfoOverlay)
        imageInfoOverlay->setExifInfo(info);
}

std::shared_ptr<FolderViewProxy> MW::getFolderView() {
    return folderView;
}

std::shared_ptr<ThumbnailStripProxy> MW::getThumbnailPanel() {
    return docWidget->thumbPanel();
}

FloatingMessageProxy *MW::activeFloatingMessage() {
    if (currentViewMode() == MODE_FOLDERVIEW) {
        if (!floatingMessageFolderView) {
            auto *container = folderView->getWidgetContainer();
            if (container) {
                floatingMessageFolderView = new FloatingMessageProxy(container);
            }
        }
        if (floatingMessageFolderView) {
            return floatingMessageFolderView;
        }
    }
    return floatingMessage;
}

void MW::showMessageDirectory(QString dirName) {
    activeFloatingMessage()->showMessage(dirName, FloatingMessageIcon::ICON_DIRECTORY, 1700);
}

void MW::showMessageDirectoryEnd() {
    activeFloatingMessage()->showMessage(tr("End of directory"), FloatingMessageIcon::NO_ICON, 600);
}

void MW::showMessageDirectoryStart() {
    activeFloatingMessage()->showMessage(tr("Start of directory"), FloatingMessageIcon::NO_ICON, 600);
}

void MW::showMessageFitWindow() {
    activeFloatingMessage()->showMessage(tr("Fit Window"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitWidth() {
    activeFloatingMessage()->showMessage(tr("Fit Width"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessageFitOriginal() {
    activeFloatingMessage()->showMessage(tr("Fit 1:1"), FloatingMessageIcon::NO_ICON, 350);
}

void MW::showMessage(QString text) {
    activeFloatingMessage()->showMessage(text,  FloatingMessageIcon::NO_ICON, 1500);
}

void MW::showMessage(QString text, int duration) {
    activeFloatingMessage()->showMessage(text, FloatingMessageIcon::NO_ICON, duration);
}

void MW::hideMessage() {
    if(floatingMessage) {
        floatingMessage->hide();
    }
    if(floatingMessageFolderView) {
        floatingMessageFolderView->hide();
    }
}

void MW::showMessageSuccess(QString text) {
    activeFloatingMessage()->showMessage(text,  FloatingMessageIcon::ICON_SUCCESS, 1500);
}

void MW::showWarning(QString text) {
    activeFloatingMessage()->showMessage(text,  FloatingMessageIcon::ICON_WARNING, 1500);
}

void MW::showError(QString text) {
    activeFloatingMessage()->showMessage(text,  FloatingMessageIcon::ICON_ERROR, 2800);
}

bool MW::showConfirmation(QString title, QString msg) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(title);
    msgBox.setText(msg);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes);
    msgBox.addButton(QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    msgBox.setModal(true);
    if(msgBox.exec() == QMessageBox::Yes)
        return true;
    else
        return false;
}

void MW::readSettings() {
    panelPosition = settings->panelPosition();
    showInfoBarFullscreen = settings->infoBarFullscreen();
    adaptToWindowState();
}

// changes ui elements according to fullscreen state
void MW::adaptToWindowState() {
    docWidget->hideFloatingPanel();
    if(isFullScreen()) { //-------------------------------------- fullscreen ---
        if(showInfoBarFullscreen)
            infoBarFullscreen->showWhenReady();
        else
            infoBarFullscreen->hide();    

        auto pos = settings->panelPosition();
        if(!settings->panelEnabled() || pos == PANEL_BOTTOM || pos == PANEL_LEFT)
            controlsOverlay->show();
        else
            controlsOverlay->hide();
    } else { //------------------------------------------------------ window ---
        infoBarFullscreen->hide();
        controlsOverlay->hide();
    }
    folderView->onFullscreenModeChanged(isFullScreen());
    docWidget->onFullscreenModeChanged(isFullScreen());
    viewerWidget->onFullscreenModeChanged(isFullScreen());
}

void MW::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    FloatingWidgetContainer::paintEvent(event);
}

void MW::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    docWidget->hideFloatingPanel(true);
}

// block native tab-switching so we can use it in shortcuts
//bool MW::focusNextPrevChild(bool) {
//    return false;
//}

void MW::togglePanorama() { viewerWidget->togglePanorama(); }
