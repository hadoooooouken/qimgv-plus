#include "foldergridcontroller.h"

#include <QDebug>
#include <QFontMetrics>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

FolderGridController::FolderGridController(ThumbnailListModel &model,
                                           const UiSettingsSnapshot &settings, QObject *parent)
    : QObject(parent),
      mModel(model),
      mFolderView(settings.folderView),
      mPanel(settings.panel),
      mViewer(settings.viewer),
      mIconSize(boundedFolderIconSize(settings.folderView.iconSize)) {
    mModel.setItemActivation(ThumbnailListModel::ItemActivation::OnDoubleClick);
    connect(&mModel, &ThumbnailListModel::selectionChanged, this,
            &FolderGridController::updateSelectedImageCount);
    connect(&mModel, &ThumbnailListModel::dirCountChanged, this,
            &FolderGridController::updateSelectedImageCount);
    // The centring of a row depends on whether the directory fills one.
    connect(&mModel, &ThumbnailListModel::countChanged, this,
            &FolderGridController::updateLayout);
    // Any activation ends the column memory of Up / Down.
    connect(&mModel, &ThumbnailListModel::activated, this, [this]() { mShiftedColumn = -1; });
    updateLayout();
}

ThumbnailListModel *FolderGridController::model() const {
    return &mModel;
}

FolderGridLayout FolderGridController::layout() const {
    return mLayout;
}

int FolderGridController::iconSize() const {
    return mIconSize;
}

int FolderGridController::zoomSliderValue() const {
    return static_cast<int>(std::lround(std::log2(mIconSize) * kSliderScale));
}

int FolderGridController::zoomSliderMinimum() {
    return static_cast<int>(std::lround(std::log2(FolderGridLayout::kMinimumIconSize) * kSliderScale));
}

int FolderGridController::zoomSliderMaximum() {
    return static_cast<int>(std::lround(std::log2(FolderGridLayout::kMaximumIconSize) * kSliderScale));
}

QRectF FolderGridController::rubberBand() const {
    return mRubberBand;
}

int FolderGridController::selectedImageCount() const {
    return mSelectedImageCount;
}

//------------------------------------------------------------------------------
// Configuration

void FolderGridController::applySettings(const UiSettingsSnapshot &settings) {
    mFolderView = settings.folderView;
    mPanel = settings.panel;
    mViewer = settings.viewer;
    // FolderView::readSettings() applies the stored icon size.
    const int size = boundedFolderIconSize(settings.folderView.iconSize);
    if (size != mIconSize) {
        mIconSize = size;
        emit iconSizeChanged();
    }
    updateLayout();
}

void FolderGridController::setLabelFont(const QFont &font) {
    mLabelFont = font;
    updateLayout();
}

void FolderGridController::setDevicePixelRatio(qreal ratio) {
    if (ratio <= 0.0) {
        qWarning() << "FolderGridController ignores the device pixel ratio" << ratio;
        return;
    }
    mDevicePixelRatio = ratio;
    configureModel();
}

void FolderGridController::setViewWidth(qreal width) {
    const int viewWidth = qMax(0, static_cast<int>(width));
    if (viewWidth == mViewWidth)
        return;
    mViewWidth = viewWidth;
    updateLayout();
}

void FolderGridController::setIconSize(int size) {
    const int bounded = boundedFolderIconSize(size);
    if (bounded == mIconSize)
        return;
    mIconSize = bounded;
    emit iconSizeChanged();
    updateLayout();
    if (!mSliderPressed)
        emit iconSizeCommitted(mIconSize);
}

// FolderView::onZoomSliderValueChanged().
void FolderGridController::setZoomSliderValue(int value) {
    if (std::abs(value - kSliderSnapValue) <= kSliderSnapRange)
        value = kSliderSnapValue;
    const int size = static_cast<int>(std::lround(std::pow(2.0, value / static_cast<double>(kSliderScale))));
    const int previous = mIconSize;
    setIconSize(size);
    // A snapped value moves the slider back to the snap point.
    if (mIconSize == previous)
        emit iconSizeChanged();
}

void FolderGridController::setZoomSliderPressed(bool pressed) {
    if (mSliderPressed == pressed)
        return;
    mSliderPressed = pressed;
    if (!pressed)
        emit iconSizeCommitted(mIconSize);
}

QColor FolderGridController::labelTextColor(const QColor &background) {
    return thumbnailLabelColor(background);
}

void FolderGridController::updateLayout() {
    const FolderGridLayout layout = folderGridLayoutFor({
        .viewWidth = mViewWidth,
        .iconSize = mIconSize,
        .itemCount = mModel.itemCount(),
        .textHeight = QFontMetrics(mLabelFont).height(),
    });
    if (layout != mLayout) {
        mLayout = layout;
        emit layoutChanged();
    }
    configureModel();
}

// The widget grid requests the logical icon size times the device pixel
// ratio and never crops (the squareThumbnails setting is the strip's).
void FolderGridController::configureModel() {
    mModel.configure({
        .pixelSize = static_cast<int>(mDevicePixelRatio * mIconSize),
        .crop = false,
        .unloadOffscreen = mPanel.unloadThumbnails,
        .cacheResolution = mPanel.thumbnailResolution,
    });
    mModel.setScrollConfig({
        .horizontal = false,
        .itemExtent = mLayout.cellHeight(),
        .columns = mLayout.columns,
        .leadingSpace = mLayout.top,
        .preloadDistance = kPreloadDistance,
        .focusShowsNeighbours = false,
        .thumbnailSize = mIconSize,
        .centerSelection = false,
        .smoothScroll = mViewer.smoothScroll,
        .wheelSpeed = mViewer.mouseScrollingSpeed,
        .trackpadDetection = mViewer.trackpadDetection,
    });
}

void FolderGridController::updateSelectedImageCount() {
    const int dirCount = mModel.dirCount();
    const QList<int> selection = mModel.selection();
    const int count = static_cast<int>(
        std::ranges::count_if(selection, [dirCount](int index) { return index >= dirCount; }));
    if (count == mSelectedImageCount)
        return;
    mSelectedImageCount = count;
    emit selectedImageCountChanged();
}

//------------------------------------------------------------------------------
// Pointer

void FolderGridController::press(int index, int button, int modifiers, QPointF position) {
    mModel.press(index, button, modifiers, position);
    const auto mouseButton = static_cast<Qt::MouseButton>(button);
    if (mouseButton != Qt::LeftButton || checkRange(index))
        return;
    // The background starts a rubber band from the selection the press left.
    mRubberBandActive = true;
    mRubberBandOrigin = QPointF(position.x(), position.y() + mModel.viewOffset());
    mRubberBandStartSelection = mModel.selection();
}

void FolderGridController::move(QPointF position, int buttons, int modifiers) {
    mModel.move(position, buttons);
    if (mRubberBandActive && Qt::MouseButtons::fromInt(buttons) & Qt::LeftButton)
        updateRubberBand(position, Qt::KeyboardModifiers::fromInt(modifiers));
}

void FolderGridController::release(int index, int button, QPointF position) {
    const auto mouseButton = static_cast<Qt::MouseButton>(button);
    const bool gesture = mModel.gestureActive();
    mModel.release(index, button, position);
    if (mouseButton == Qt::LeftButton)
        endRubberBand();
    else if (mouseButton == Qt::RightButton && !gesture)
        emit contextMenuRequested(position);
}

void FolderGridController::doubleClick(int index, int button) {
    mModel.doubleClick(index, button);
}

// FolderGridView::wheelEvent().
void FolderGridController::wheel(QPoint angleDelta, QPoint pixelDelta, int modifiers) {
    if (!(Qt::KeyboardModifiers::fromInt(modifiers) & Qt::ControlModifier)) {
        mModel.wheelScrolled(angleDelta, pixelDelta);
        return;
    }
    if (pixelDelta.y() > 0 || angleDelta.y() > 0)
        setIconSize(mIconSize + FolderGridLayout::kZoomStep);
    else if (pixelDelta.y() < 0 || angleDelta.y() < 0)
        setIconSize(mIconSize - FolderGridLayout::kZoomStep);
}

// ThumbnailView::rubberBandChanged handler: items whose cell the band
// touches are added to the selection held at its start; with Ctrl they
// toggle against it.
void FolderGridController::updateRubberBand(QPointF position, Qt::KeyboardModifiers modifiers) {
    const qreal offset = mModel.viewOffset();
    const QRectF content =
        QRectF(mRubberBandOrigin, QPointF(position.x(), position.y() + offset)).normalized();
    mRubberBand = content.translated(0.0, -offset);
    emit rubberBandChanged();

    QList<int> selection = mRubberBandStartSelection;
    const bool toggle = modifiers & Qt::ControlModifier;
    const int columns = qMax(1, mLayout.columns);
    const int cellHeight = qMax(1, mLayout.cellHeight());
    const int firstRow =
        qMax(0, static_cast<int>(std::floor((content.top() - mLayout.top) / cellHeight)));
    const int lastRow = static_cast<int>(std::floor((content.bottom() - mLayout.top) / cellHeight));
    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = 0; column < columns; ++column) {
            const int index = row * columns + column;
            if (!checkRange(index))
                break;
            if (!mLayout.itemRect(index).intersects(content))
                continue;
            if (toggle && mRubberBandStartSelection.contains(index))
                selection.removeAll(index);
            else if (!selection.contains(index))
                selection.append(index);
        }
    }
    std::ranges::sort(selection);
    if (selection != mModel.selection())
        mModel.select(selection);
}

void FolderGridController::endRubberBand() {
    if (!mRubberBandActive)
        return;
    mRubberBandActive = false;
    mRubberBandStartSelection.clear();
    if (mRubberBand.isNull())
        return;
    mRubberBand = {};
    emit rubberBandChanged();
}

//------------------------------------------------------------------------------
// Keyboard (FolderGridView::keyPressEvent())

bool FolderGridController::keyPressed(int key, int modifiers, const QString &text) {
    const auto keyboardModifiers = Qt::KeyboardModifiers::fromInt(modifiers);
    // ThumbnailView::keyPressEvent(): Shift anchors a range.
    if (key == Qt::Key_Shift) {
        mModel.beginRangeSelection();
        return true;
    }
    if ((keyboardModifiers & Qt::ShiftModifier) && !mModel.rangeSelectionActive())
        mModel.beginRangeSelection();

    if (key == Qt::Key_Enter || key == Qt::Key_Return) {
        mModel.activate(current());
        return true;
    }
    if (key == Qt::Key_Backspace) {
        emit actionRequested(QStringLiteral("goUp"));
        return true;
    }
    if (keyboardModifiers & Qt::ControlModifier) {
        const Qt::KeyboardModifiers otherModifiers =
            keyboardModifiers & ~(Qt::ControlModifier | Qt::KeypadModifier);
        if (key == Qt::Key_A && otherModifiers == Qt::NoModifier) {
            selectAll();
            return true;
        }
        return false;
    }

    switch (key) {
    case Qt::Key_Left:
        selectPrevious();
        return true;
    case Qt::Key_Right:
        selectNext();
        return true;
    case Qt::Key_Up:
        selectAbove();
        return true;
    case Qt::Key_Down:
        selectBelow();
        return true;
    case Qt::Key_PageUp:
        pageUp();
        return true;
    case Qt::Key_PageDown:
        pageDown();
        return true;
    case Qt::Key_Home:
        selectFirst();
        return true;
    case Qt::Key_End:
        selectLast();
        return true;
    default:
        break;
    }
    // Windows Explorer style type-ahead: a printable character without Alt
    // or Meta jumps to the next matching name (DirectoryPresenter matches).
    if (!(keyboardModifiers & (Qt::AltModifier | Qt::MetaModifier)) && !text.isEmpty() &&
        text.at(0).isPrint()) {
        emit typeAheadRequested(text);
        return true;
    }
    return false;
}

void FolderGridController::keyReleased(int key) {
    if (key == Qt::Key_Shift)
        mModel.endRangeSelection();
}

void FolderGridController::focusLost() {
    mModel.endRangeSelection();
    endRubberBand();
}

bool FolderGridController::checkRange(int index) const {
    return index >= 0 && index < mModel.itemCount();
}

int FolderGridController::current() const {
    return mModel.currentIndex();
}

int FolderGridController::itemAbove(int index) const {
    const int above = index - qMax(1, mLayout.columns);
    return checkRange(above) ? above : index;
}

int FolderGridController::itemBelow(int index) const {
    return qMin(mModel.itemCount() - 1, index + qMax(1, mLayout.columns));
}

int FolderGridController::columnOf(int index) const {
    return checkRange(index) ? index % qMax(1, mLayout.columns) : -1;
}

bool FolderGridController::sameRow(int one, int two) const {
    if (!checkRange(one) || !checkRange(two))
        return false;
    const int columns = qMax(1, mLayout.columns);
    return one / columns == two / columns;
}

void FolderGridController::selectOrExtend(int index) {
    if (mModel.rangeSelectionActive())
        mModel.selectRangeTo(index);
    else
        mModel.select(index);
    mModel.scrollToItem(current());
}

void FolderGridController::selectAll() {
    QList<int> all;
    all.reserve(mModel.itemCount());
    for (int index = 0; index < mModel.itemCount(); ++index)
        all.append(index);
    // The current item goes last, so that it stays current.
    const int currentIndex = current();
    if (currentIndex >= 0)
        all.move(currentIndex, all.size() - 1);
    mModel.select(all);
}

void FolderGridController::selectAbove() {
    const int last = current();
    if (mModel.itemCount() == 0 || last < 0 || sameRow(0, last))
        return;
    int target = itemAbove(last);
    if (mShiftedColumn >= 0) {
        target += mShiftedColumn - columnOf(last);
        mShiftedColumn = -1;
    }
    if (!checkRange(target))
        target = 0;
    selectOrExtend(target);
}

void FolderGridController::selectBelow() {
    const int last = current();
    const int count = mModel.itemCount();
    if (count == 0 || last < 0 || sameRow(last, count - 1))
        return;
    mShiftedColumn = -1;
    int target = itemBelow(last);
    if (!checkRange(target))
        target = count - 1;
    if (columnOf(target) != columnOf(last))
        mShiftedColumn = columnOf(last);
    selectOrExtend(target);
}

void FolderGridController::selectNext() {
    const int count = mModel.itemCount();
    if (count == 0 || current() == count - 1)
        return;
    mShiftedColumn = -1;
    selectOrExtend(qMin(current() + 1, count - 1));
}

void FolderGridController::selectPrevious() {
    if (mModel.itemCount() == 0 || current() == 0)
        return;
    mShiftedColumn = -1;
    selectOrExtend(qMax(current() - 1, 0));
}

void FolderGridController::pageUp() {
    const int last = current();
    if (mModel.itemCount() == 0 || last < 0 || sameRow(0, last))
        return;
    int target = last;
    for (int row = 0; row < kPageRows; ++row)
        target = itemAbove(target);
    if (mShiftedColumn >= 0) {
        target += mShiftedColumn - columnOf(target);
        mShiftedColumn = -1;
    }
    selectOrExtend(target);
}

void FolderGridController::pageDown() {
    const int last = current();
    const int count = mModel.itemCount();
    if (count == 0 || last < 0 || sameRow(last, count - 1))
        return;
    mShiftedColumn = -1;
    int target = last;
    for (int row = 0; row < kPageRows; ++row)
        target = itemBelow(target);
    if (columnOf(target) != columnOf(last))
        mShiftedColumn = columnOf(last);
    selectOrExtend(target);
}

void FolderGridController::selectFirst() {
    if (mModel.itemCount() == 0)
        return;
    mShiftedColumn = -1;
    selectOrExtend(0);
}

void FolderGridController::selectLast() {
    if (mModel.itemCount() == 0)
        return;
    mShiftedColumn = -1;
    selectOrExtend(mModel.itemCount() - 1);
}

//------------------------------------------------------------------------------
// Drops and the context menu

bool FolderGridController::acceptsDropAction(int action) {
    const auto dropAction = static_cast<Qt::DropAction>(action);
    return dropAction == Qt::CopyAction || dropAction == Qt::MoveAction;
}

// FolderGridView::dragMoveEvent(): a new target removes the frame of the
// previous one; the presenter frames the new one if it is a directory.
void FolderGridController::dragMoved(int index) {
    const int target = checkRange(index) ? index : -1;
    if (target != mDragTarget)
        mModel.setDragHover(-1);
    mDragTarget = target;
    emit draggedOver(target);
}

void FolderGridController::dragLeft() {
    mDragTarget = -1;
    mModel.setDragHover(-1);
}

void FolderGridController::drop(const QList<QUrl> &urls, QObject *source, int index, int action) {
    dragLeft();
    if (!acceptsDropAction(action)) {
        qWarning() << "FolderGridController ignores a drop with action" << action;
        return;
    }
    emit urlsDropped(urls, source, checkRange(index) ? index : -1,
                     static_cast<Qt::DropAction>(action));
}

void FolderGridController::openSelected() {
    emit openSelectedRequested();
}

void FolderGridController::requestBatchConversion() {
    emit batchConversionRequested();
}
