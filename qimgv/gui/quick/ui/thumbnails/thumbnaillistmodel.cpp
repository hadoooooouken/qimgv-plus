#include "thumbnaillistmodel.h"

#include <QDebug>
#include <QLineF>
#include <QVariant>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
const QList<int> kThumbnailRoles = {
    ThumbnailListModel::NameRole,    ThumbnailListModel::InfoRole,
    ThumbnailListModel::ThumbnailRole, ThumbnailListModel::LoadedRole,
    ThumbnailListModel::PendingRole, ThumbnailListModel::UnavailableRole,
};

QSet<int> shiftedSet(const QSet<int> &set, int firstIndex, int offset) {
    QSet<int> shifted;
    shifted.reserve(set.size());
    for (const int index : set)
        shifted.insert(index >= firstIndex ? index + offset : index);
    return shifted;
}
} // namespace

ThumbnailListModel::ThumbnailListModel(QObject *parent) : QAbstractListModel(parent) {
    mLoadTimer.setSingleShot(true);
    mLoadTimer.setInterval(kLoadDelayMs);
    connect(&mLoadTimer, &QTimer::timeout, this, &ThumbnailListModel::loadVisibleThumbnails);
}

int ThumbnailListModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : mCount;
}

QVariant ThumbnailListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !checkRange(index.row()))
        return {};
    const int row = index.row();
    const auto loaded = mLoaded.constFind(row);
    const bool isLoaded = loaded != mLoaded.cend();
    switch (role) {
    case NameRole:
        return isLoaded ? loaded->name : QString();
    case InfoRole:
        return isLoaded ? loaded->info : QString();
    case DirectoryRole:
        return row < mDirCount;
    case ThumbnailRole:
        return QVariant::fromValue(isLoaded ? loaded->handle : ThumbnailHandle());
    case LoadedRole:
        return isLoaded;
    case PendingRole:
        return mRequested.contains(row) || mPendingFinal.contains(row);
    case UnavailableRole:
        return mUnavailable.contains(row);
    case SelectedRole:
        return mSelection.contains(row);
    case DragHoverRole:
        return row == mDragHover;
    default:
        return {};
    }
}

QHash<int, QByteArray> ThumbnailListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {InfoRole, "info"},
        {DirectoryRole, "isDir"},
        {ThumbnailRole, "thumbnail"},
        {LoadedRole, "loaded"},
        {PendingRole, "pending"},
        {UnavailableRole, "unavailable"},
        {SelectedRole, "selected"},
        {DragHoverRole, "dragHover"},
    };
}

int ThumbnailListModel::itemCount() const {
    return mCount;
}

int ThumbnailListModel::currentIndex() const {
    return mSelection.isEmpty() ? -1 : mSelection.constLast();
}

QList<int> ThumbnailListModel::selection() const {
    return mSelection;
}

ThumbnailRequestConfig ThumbnailListModel::requestConfig() const {
    return mRequest;
}

ThumbnailScrollConfig ThumbnailListModel::scrollConfig() const {
    return mScroll;
}

qreal ThumbnailListModel::viewOffset() const {
    return mOffset;
}

//------------------------------------------------------------------------------
// Directory view

void ThumbnailListModel::populate(int count) {
    if (count < 0) {
        qWarning() << "ThumbnailListModel received a negative item count:" << count;
        return;
    }
    beginResetModel();
    mCount = count;
    mSelection.clear();
    mDragHover = -1;
    mLoaded.clear();
    mRequested.clear();
    mPendingFinal.clear();
    mUnavailable.clear();
    mReadyReported = false;
    mScrollDirection = ScrollDirection::Forwards;
    endResetModel();
    emit countChanged();
    emit selectionChanged();
    // The view starts at the beginning of the new directory.
    stopAnimation();
    mOffset = 0.0;
    emit scrollRequested(mOffset, 0);
    loadVisibleThumbnails();
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailListModel::setThumbnail(int pos, const ThumbnailEntry &entry, int size) {
    if (size != mRequest.pixelSize || !checkRange(pos))
        return;
    mRequested.remove(pos);
    mUnavailable.remove(pos);
    // Without unloading every thumbnail is kept; otherwise only those the
    // view can reach soon.
    if (!mRequest.unloadOffscreen || (mActive && preloadRange().contains(pos)))
        mLoaded.insert(pos, entry);
    emitRowChanged(pos, kThumbnailRoles);
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailListModel::setThumbnailPending(int pos, bool pending) {
    if (!checkRange(pos))
        return;
    if (pending)
        mPendingFinal.insert(pos);
    else
        mPendingFinal.remove(pos);
    emitRowChanged(pos, {PendingRole});
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailListModel::setThumbnailUnavailable(int pos, int size) {
    if (size != mRequest.pixelSize || !checkRange(pos))
        return;
    mRequested.remove(pos);
    mUnavailable.insert(pos);
    emitRowChanged(pos, kThumbnailRoles);
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailListModel::select(QList<int> indices) {
    indices.removeIf([this](int index) { return !checkRange(index); });
    const QList<int> previous = mSelection;
    mSelection = indices;
    QSet<int> changed(previous.cbegin(), previous.cend());
    changed.unite(QSet<int>(indices.cbegin(), indices.cend()));
    for (const int index : std::as_const(changed))
        emitRowChanged(index, {SelectedRole});
    emit selectionChanged();
}

void ThumbnailListModel::select(int index) {
    select(QList<int>{checkRange(index) ? index : 0});
}

void ThumbnailListModel::clearSelection() {
    select(QList<int>());
}

// The last selected item stays selected.
void ThumbnailListModel::deselect(int index) {
    if (!checkRange(index) || mSelection.count() <= 1)
        return;
    mSelection.removeAll(index);
    emitRowChanged(index, {SelectedRole});
    emit selectionChanged();
}

void ThumbnailListModel::focusOn(int index) {
    if (!checkRange(index) || mExtent <= 0.0 || mScroll.itemExtent <= 0)
        return;
    const qreal start = static_cast<qreal>(index) * mScroll.itemExtent;
    const qreal extent = mScroll.itemExtent;
    if (mScroll.centerSelection) {
        const qreal targetCenter = start + extent / 2.0;
        if (mScroll.smoothScroll) {
            const qreal currentCenter = mOffset + mExtent / 2.0;
            scrollSmooth(static_cast<int>(currentCenter - targetCenter), 1.0, 1.0, false);
        } else {
            scrollTo(targetCenter - mExtent / 2.0);
        }
    } else {
        // Shows part of the next thumbnail when there is room
        // (QGraphicsView::ensureVisible() with a margin).
        const qreal margin = mExtent > extent * 2.0 ? extent / 2.0 : 0.0;
        if (start - margin < mOffset)
            scrollTo(start - margin);
        else if (start + extent + margin > mOffset + mExtent)
            scrollTo(start + extent + margin - mExtent);
    }
    loadVisibleThumbnails();
}

void ThumbnailListModel::focusOnSelection() {
    if (!mSelection.isEmpty())
        focusOn(mSelection.constLast());
}

void ThumbnailListModel::insertItem(int index) {
    if (index < 0 || index > mCount) {
        qWarning() << "ThumbnailListModel cannot insert out-of-range index:" << index;
        return;
    }
    QList<int> newSelection = mSelection;
    for (int &selected : newSelection) {
        if (index <= selected)
            ++selected;
    }
    beginInsertRows({}, index, index);
    shiftCachedItems(index, 1);
    if (mDragHover >= index)
        ++mDragHover;
    // The selection is published again below; until then it must not name
    // rows beyond the count.
    mSelection.clear();
    ++mCount;
    endInsertRows();
    emit countChanged();
    select(newSelection);
    loadVisibleThumbnailsDelayed();
}

void ThumbnailListModel::removeItem(int index) {
    if (!checkRange(index)) {
        qWarning() << "ThumbnailListModel cannot remove out-of-range index:" << index;
        return;
    }
    QList<int> newSelection = mSelection;
    newSelection.removeAll(index);
    for (int &selected : newSelection) {
        if (selected >= index)
            --selected;
    }
    beginRemoveRows({}, index, index);
    mLoaded.remove(index);
    mRequested.remove(index);
    mPendingFinal.remove(index);
    mUnavailable.remove(index);
    shiftCachedItems(index + 1, -1);
    if (mDragHover == index)
        mDragHover = -1;
    else if (mDragHover > index)
        --mDragHover;
    mSelection.clear();
    --mCount;
    endRemoveRows();
    emit countChanged();
    // The neighbour takes the selection of the removed item.
    if (newSelection.isEmpty() && mCount > 0)
        newSelection.append(index >= mCount ? mCount - 1 : index);
    select(newSelection);
    loadVisibleThumbnailsDelayed();
}

void ThumbnailListModel::reloadItem(int index) {
    if (!checkRange(index))
        return;
    mLoaded.remove(index);
    mUnavailable.remove(index);
    mRequested.insert(index);
    emitRowChanged(index, kThumbnailRoles);
    emit thumbnailsNeeded({index}, mRequest.pixelSize, mRequest.crop, true);
}

void ThumbnailListModel::setDragHover(int index) {
    const int newIndex = checkRange(index) ? index : -1;
    if (newIndex == mDragHover)
        return;
    const int previous = mDragHover;
    mDragHover = newIndex;
    if (checkRange(previous))
        emitRowChanged(previous, {DragHoverRole});
    if (checkRange(newIndex))
        emitRowChanged(newIndex, {DragHoverRole});
}

void ThumbnailListModel::setDirCount(int count) {
    const int newCount = qMax(0, count);
    if (newCount == mDirCount)
        return;
    const int last = qMin(qMax(newCount, mDirCount), mCount) - 1;
    mDirCount = newCount;
    if (last >= 0)
        emit dataChanged(index(0), index(last), {DirectoryRole});
}

//------------------------------------------------------------------------------
// Configuration

void ThumbnailListModel::configure(const ThumbnailRequestConfig &config) {
    if (config == mRequest)
        return;
    const bool unload = config.pixelSize != mRequest.pixelSize ||
                        config.crop != mRequest.crop ||
                        config.cacheResolution != mRequest.cacheResolution;
    mRequest = config;
    if (unload)
        unloadAllThumbnails();
    loadVisibleThumbnails();
}

void ThumbnailListModel::setScrollConfig(const ThumbnailScrollConfig &config) {
    if (config == mScroll)
        return;
    mScroll = config;
    focusOnSelection();
    loadVisibleThumbnails();
}

void ThumbnailListModel::setLoadingBlocked(bool blocked) {
    if (mLoadingBlocked == blocked)
        return;
    mLoadingBlocked = blocked;
    if (!blocked)
        loadVisibleThumbnails();
}

//------------------------------------------------------------------------------
// View

void ThumbnailListModel::setActive(bool active) {
    if (mActive == active)
        return;
    mActive = active;
    if (!active) {
        // The view is gone or hidden: an animation it ran will not report.
        mAnimating = false;
        mPressIndex = -1;
        mGestureActive = false;
        return;
    }
    focusOnSelection();
    loadVisibleThumbnails();
}

void ThumbnailListModel::setViewport(qreal offset, qreal extent) {
    const bool offsetChanged = !qFuzzyCompare(offset + 1.0, mOffset + 1.0);
    const bool extentChanged = !qFuzzyCompare(extent + 1.0, mExtent + 1.0);
    if (!offsetChanged && !extentChanged)
        return;
    const bool firstLayout = mExtent <= 0.0 && extent > 0.0;
    mOffset = offset;
    mExtent = extent;
    if (firstLayout) {
        focusOnSelection();
        loadVisibleThumbnails();
    } else if (offsetChanged) {
        loadVisibleThumbnails();
    } else {
        loadVisibleThumbnailsDelayed();
    }
}

void ThumbnailListModel::scrollAnimationFinished() {
    if (!mAnimating)
        return;
    mAnimating = false;
    loadVisibleThumbnails();
}

void ThumbnailListModel::wheelScrolled(QPoint angleDelta, QPoint pixelDelta) {
    const int pixel = pixelDelta.y();
    int angle = angleDelta.y();
    bool isWheel = true;
    if (mScroll.trackpadDetection) {
        const bool touchpadQuiet = !mLastTouchpadScroll.isValid() ||
                                   mLastTouchpadScroll.elapsed() > kTouchpadQuietMs;
        isWheel = angle != 0 && std::abs(angle) >= kWheelStepAngle &&
                  angle % kWheelAngleGranularity == 0 && touchpadQuiet;
    }

    if (isWheel) {
        angle = static_cast<int>(angle * mScroll.wheelSpeed);
        if (!mScroll.smoothScroll) {
            if (pixel != 0)
                scrollByItem(pixel);
            else if (angle != 0)
                scrollByItem(angle);
        } else if (angle != 0) {
            scrollSmooth(angle, kWheelScrollMultiplier, kScrollAcceleration, true);
        }
        return;
    }
    // A touchpad: one of the deltas may be scaled, use the larger one.
    mLastTouchpadScroll.start();
    scrollPrecise(std::abs(angle) > std::abs(pixel) ? angle : pixel);
}

void ThumbnailListModel::press(int index, int button, int modifiers, QPointF position) {
    const auto mouseButton = static_cast<Qt::MouseButton>(button);
    const auto keyboardModifiers = Qt::KeyboardModifiers::fromInt(modifiers);
    mReleaseSelects = false;
    mPressPosition = position;
    mPressIndex = checkRange(index) ? index : -1;

    if (mouseButton == Qt::RightButton) {
        mGestureActive = false;
        return;
    }
    if (mPressIndex >= 0) {
        if (mouseButton == Qt::LeftButton) {
            if (keyboardModifiers & Qt::ControlModifier) {
                if (!mSelection.contains(mPressIndex))
                    select(mSelection + QList<int>{mPressIndex});
                else
                    deselect(mPressIndex);
            } else if (keyboardModifiers & Qt::ShiftModifier) {
                // The widget strip never takes the keyboard focus, so it has
                // no range anchor and Shift-click does nothing there.
            } else if (mSelection.count() <= 1) {
                emit activated(mPressIndex);
                return;
            } else {
                mReleaseSelects = true;
            }
        }
    } else if (mouseButton == Qt::LeftButton &&
               !(keyboardModifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
        clearSelection();
    }
    if (mouseButton == Qt::BackButton)
        emit backNavigationRequested();
    else if (mouseButton == Qt::ForwardButton)
        emit forwardNavigationRequested();
}

void ThumbnailListModel::move(QPointF position, int buttons) {
    const auto mouseButtons = Qt::MouseButtons::fromInt(buttons);
    if (mouseButtons & Qt::RightButton) {
        if (!mGestureActive) {
            const qreal travel = mScroll.horizontal ? position.x() - mPressPosition.x()
                                                    : position.y() - mPressPosition.y();
            if (std::abs(travel) > kGestureThreshold) {
                mGestureActive = true;
                scrollToEdge(travel < 0);
            }
        }
        return;
    }
    if (mouseButtons != Qt::LeftButton || mSelection.isEmpty())
        return;
    if (QLineF(mPressPosition, position).length() >= kDragThreshold &&
        mSelection.contains(mPressIndex)) {
        emit dragOutRequested();
    }
}

void ThumbnailListModel::release(int index, int button, QPointF position) {
    const auto mouseButton = static_cast<Qt::MouseButton>(button);
    if (mouseButton == Qt::RightButton) {
        if (!mGestureActive && checkRange(index) && !mSelection.contains(index))
            select(index);
        mGestureActive = false;
        return;
    }
    if (mReleaseSelects && QLineF(mPressPosition, position).length() < kDragThreshold &&
        checkRange(index)) {
        select(index);
    }
    mReleaseSelects = false;
}

void ThumbnailListModel::doubleClick(int index, int button) {
    if (static_cast<Qt::MouseButton>(button) == Qt::LeftButton && checkRange(index))
        emit activated(index);
}

//------------------------------------------------------------------------------
// Ranges and loading

bool ThumbnailListModel::checkRange(int index) const {
    return index >= 0 && index < mCount;
}

ThumbnailListModel::ItemRange ThumbnailListModel::itemRange(qreal start, qreal end) const {
    if (mCount == 0 || mScroll.itemExtent <= 0)
        return {};
    const int first = qBound(0, static_cast<int>(std::floor(start / mScroll.itemExtent)), mCount - 1);
    const int last = qBound(0, static_cast<int>(std::floor(end / mScroll.itemExtent)), mCount - 1);
    return first <= last ? ItemRange{first, last} : ItemRange{};
}

ThumbnailListModel::ItemRange ThumbnailListModel::preloadRange() const {
    return itemRange(mOffset - kPreloadDistance, mOffset + mExtent + kPreloadDistance);
}

ThumbnailListModel::ItemRange ThumbnailListModel::visibleRange() const {
    return itemRange(mOffset, mOffset + mExtent);
}

qreal ThumbnailListModel::contentExtent() const {
    return static_cast<qreal>(mCount) * mScroll.itemExtent;
}

qreal ThumbnailListModel::maximumOffset() const {
    return qMax(0.0, contentExtent() - mExtent);
}

bool ThumbnailListModel::atStart() const {
    return mOffset <= 0.0;
}

bool ThumbnailListModel::atEnd() const {
    return mOffset >= maximumOffset();
}

bool ThumbnailListModel::loadingAllowed() const {
    return mActive && !mLoadingBlocked && !mAnimating;
}

void ThumbnailListModel::loadVisibleThumbnails() {
    mLoadTimer.stop();
    if (!loadingAllowed())
        return;
    const ItemRange range = preloadRange();
    if (range.isValid()) {
        QList<int> loadList;
        for (int index = range.first; index <= range.last; ++index) {
            if (!mLoaded.contains(index) && !mRequested.contains(index) &&
                !mUnavailable.contains(index))
                loadList.append(index);
        }
        if (mScrollDirection == ScrollDirection::Backwards)
            std::reverse(loadList.begin(), loadList.end());
        if (!loadList.isEmpty()) {
            for (const int index : std::as_const(loadList))
                mRequested.insert(index);
            emit thumbnailsNeeded(loadList, mRequest.pixelSize, mRequest.crop, false);
        }
    }
    if (mRequest.unloadOffscreen) {
        for (auto it = mLoaded.begin(); it != mLoaded.end();) {
            if (!range.contains(it.key()))
                it = mLoaded.erase(it);
            else
                ++it;
        }
    }
    notifyIfVisibleThumbnailsReady();
}

void ThumbnailListModel::loadVisibleThumbnailsDelayed() {
    mLoadTimer.start();
}

void ThumbnailListModel::unloadAllThumbnails() {
    mLoaded.clear();
    mRequested.clear();
    mPendingFinal.clear();
    mUnavailable.clear();
    mReadyReported = false;
    emitAllRowsChanged(kThumbnailRoles);
}

bool ThumbnailListModel::visibleThumbnailsLoaded() const {
    if (!mActive)
        return false;
    const ItemRange range = visibleRange();
    if (!range.isValid())
        return mCount == 0;
    for (int index = range.first; index <= range.last; ++index) {
        if (mPendingFinal.contains(index) ||
            (!mLoaded.contains(index) && !mUnavailable.contains(index)))
            return false;
    }
    return true;
}

void ThumbnailListModel::notifyIfVisibleThumbnailsReady() {
    if (!mReadyReported && visibleThumbnailsLoaded()) {
        mReadyReported = true;
        emit visibleThumbnailsReady();
    }
}

void ThumbnailListModel::shiftCachedItems(int firstIndex, int offset) {
    if (offset == 0)
        return;
    QHash<int, ThumbnailEntry> shiftedThumbnails;
    shiftedThumbnails.reserve(mLoaded.size());
    for (auto it = mLoaded.cbegin(); it != mLoaded.cend(); ++it)
        shiftedThumbnails.insert(it.key() >= firstIndex ? it.key() + offset : it.key(), it.value());
    mLoaded.swap(shiftedThumbnails);
    mRequested = shiftedSet(mRequested, firstIndex, offset);
    mPendingFinal = shiftedSet(mPendingFinal, firstIndex, offset);
    mUnavailable = shiftedSet(mUnavailable, firstIndex, offset);
}

void ThumbnailListModel::emitRowChanged(int row, const QList<int> &roles) {
    if (checkRange(row))
        emit dataChanged(index(row), index(row), roles);
}

void ThumbnailListModel::emitAllRowsChanged(const QList<int> &roles) {
    if (mCount > 0)
        emit dataChanged(index(0), index(mCount - 1), roles);
}

//------------------------------------------------------------------------------
// Scrolling (ThumbnailView::scroll*)

// At once; the view reports the offset back, which changes nothing then.
void ThumbnailListModel::scrollTo(qreal offset) {
    stopAnimation();
    mOffset = qBound(0.0, offset, maximumOffset());
    emit scrollRequested(mOffset, 0);
    loadVisibleThumbnails();
}

// Nothing is requested until the view reports the end of the animation.
void ThumbnailListModel::animateTo(qreal offset, int durationMs) {
    mAnimating = true;
    mAnimationTarget = qBound(0.0, offset, maximumOffset());
    mAnimationClock.start();
    emit scrollRequested(mAnimationTarget, durationMs);
}

void ThumbnailListModel::stopAnimation() {
    mAnimating = false;
}

void ThumbnailListModel::scrollSmooth(int delta, double multiplier, double acceleration,
                                      bool additive) {
    mScrollDirection = delta < 0 ? ScrollDirection::Forwards : ScrollDirection::Backwards;
    if ((delta > 0 && atStart()) || (delta < 0 && atEnd()))
        return;
    const qreal start = mOffset;
    qreal target = start - delta * multiplier;
    // A step against the running scroll starts over from here.
    const bool redirect = (target < start && start < mAnimationTarget) ||
                          (target > start && start > mAnimationTarget);
    bool accelerate = false;
    if (mAnimating) {
        accelerate = mAnimationClock.elapsed() < kAccelerationThresholdMs;
        if (!redirect && additive)
            target = mAnimationTarget - delta * multiplier * acceleration;
        // Thumbnails are loaded between consecutive scroll animations.
        mAnimating = false;
        loadVisibleThumbnails();
    }
    const int duration = accelerate ? static_cast<int>(kScrollDurationMs / kScrollAcceleration)
                                    : kScrollDurationMs;
    animateTo(target, duration);
}

void ThumbnailListModel::scrollPrecise(int delta) {
    mScrollDirection = delta < 0 ? ScrollDirection::Forwards : ScrollDirection::Backwards;
    stopAnimation();
    if ((delta > 0 && atStart()) || (delta < 0 && atEnd()))
        return;
    scrollTo(mOffset - delta);
}

// Explorer-like: scrolls to the next item that is not fully visible.
void ThumbnailListModel::scrollByItem(int delta) {
    const qreal minimumScroll = qMin(mScroll.thumbnailSize / 2, kMaximumMinimumScrollPx);
    const ItemRange range =
        itemRange(mOffset - minimumScroll, mOffset + mExtent + minimumScroll);
    if (!range.isValid())
        return;
    scrollToItem(delta > 0 ? range.first - 1 : range.last + 1);
}

void ThumbnailListModel::scrollToItem(int index) {
    if (!checkRange(index))
        return;
    const qreal start = static_cast<qreal>(index) * mScroll.itemExtent;
    const qreal end = start + mScroll.itemExtent;
    if (start >= mOffset && end <= mOffset + mExtent)
        return;
    const int delta = static_cast<int>(start >= mOffset ? (mOffset + mExtent) - end
                                                        : mOffset - start);
    if (mScroll.smoothScroll)
        scrollSmooth(delta, 1.0, 1.0, false);
    else
        scrollPrecise(delta);
}

void ThumbnailListModel::scrollToEdge(bool end) {
    const qreal center = mOffset + mExtent / 2.0;
    const qreal targetCenter = end ? contentExtent() : 0.0;
    const qreal distance = std::abs(targetCenter - center);
    if (distance <= 0.0)
        return;
    const int duration = qBound(
        kEdgeScrollMinimumMs,
        static_cast<int>(kEdgeScrollBaseMs + std::sqrt(distance) * kEdgeScrollDistanceFactor),
        kEdgeScrollMaximumMs);
    animateTo(targetCenter - mExtent / 2.0, duration);
}
