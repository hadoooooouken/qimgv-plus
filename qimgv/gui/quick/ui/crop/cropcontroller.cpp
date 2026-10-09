#include "cropcontroller.h"

#include <QDebug>
#include <QtGlobal>

#include <cmath>

namespace {
// Ratios of the fixed presets.
constexpr QPointF kSquareRatio{1.0, 1.0};
constexpr QPointF kFourByThreeRatio{4.0, 3.0};
constexpr QPointF kSixteenByNineRatio{16.0, 9.0};
constexpr QPointF kSixteenByTenRatio{16.0, 10.0};

Qt::CursorShape cursorFor(CropHandle handle) {
    switch (handle) {
    case CropHandle::TopLeft:
    case CropHandle::BottomRight:
        return Qt::SizeFDiagCursor;
    case CropHandle::TopRight:
    case CropHandle::BottomLeft:
        return Qt::SizeBDiagCursor;
    case CropHandle::Left:
    case CropHandle::Right:
        return Qt::SizeHorCursor;
    case CropHandle::Top:
    case CropHandle::Bottom:
        return Qt::SizeVerCursor;
    case CropHandle::Move:
        return Qt::OpenHandCursor;
    case CropHandle::None:
        break;
    }
    return Qt::ArrowCursor;
}

// The handles in the order of CropController::handles().
constexpr CropHandle kHandleOrder[] = {
    CropHandle::TopLeft, CropHandle::TopRight, CropHandle::BottomLeft, CropHandle::BottomRight,
    CropHandle::Left,    CropHandle::Right,    CropHandle::Top,        CropHandle::Bottom,
};

// Width-to-height ratio of size as (ratio, 1).
QPointF ratioOf(QSize size) {
    return QPointF(qreal(size.width()) / size.height(), 1.0);
}
} // namespace

CropController::CropController(const UiSettingsSnapshot &settings, QObject *parent)
    : QObject(parent) {
    applySettings(settings);
}

//------------------------------------------------------------------------------
// Mode
//------------------------------------------------------------------------------
bool CropController::isActive() const {
    return mActive;
}

void CropController::toggle() {
    if (mActive) {
        close();
        return;
    }
    if (mFolderViewActive || imageSize().isEmpty())
        return;
    resetSelection();
    setActive(true);
}

void CropController::close() {
    if (!mActive)
        return;
    mSelection.endDrag();
    mSelection.clear();
    setCursorShape(Qt::ArrowCursor);
    notifySelection();
    setActive(false);
}

void CropController::setFolderViewActive(bool active) {
    mFolderViewActive = active;
    if (active)
        close();
}

void CropController::applySettings(const UiSettingsSnapshot &settings) {
    if (mDefaultAction == settings.overlays.defaultCropAction)
        return;
    mDefaultAction = settings.overlays.defaultCropAction;
    emit defaultActionChanged();
}

void CropController::setActive(bool active) {
    if (mActive == active)
        return;
    mActive = active;
    emit activeChanged();
}

//------------------------------------------------------------------------------
// Image and view
//------------------------------------------------------------------------------
QSize CropController::imageSize() const {
    return mSelection.imageSize();
}

void CropController::setImageSize(QSize size) {
    const bool changed = size != imageSize();
    mSelection.setImageSize(size);
    if (changed)
        emit imageSizeChanged();
    if (!mActive)
        return;
    if (size.isEmpty()) {
        close();
        return;
    }
    resetSelection();
}

QRectF CropController::imageArea() const {
    return mImageArea;
}

void CropController::setImageArea(const QRectF &area) {
    if (mImageArea == area)
        return;
    mImageArea = area;
    emit geometryChanged();
}

void CropController::setDevicePixelRatio(qreal ratio) {
    if (qFuzzyCompare(mDevicePixelRatio, ratio))
        return;
    mDevicePixelRatio = ratio;
    emit geometryChanged();
}

void CropController::setScreenSize(QSize size) {
    mScreenSize = size;
}

//------------------------------------------------------------------------------
// Selection
//------------------------------------------------------------------------------
QRect CropController::selection() const {
    return mSelection.rect();
}

bool CropController::hasSelection() const {
    return mSelection.hasSelection();
}

QRectF CropController::selectionArea() const {
    const qreal scale = viewScale();
    if (!hasSelection() || scale <= 0)
        return {};
    const QRect rect = mSelection.rect();
    return QRectF(mImageArea.topLeft() + QPointF(rect.topLeft()) * scale,
                  QSizeF(rect.size()) * scale);
}

QList<QRectF> CropController::handles() const {
    const QRectF area = selectionArea();
    if (area.isEmpty())
        return {};
    const QSizeF size(kHandleSize, kHandleSize);
    const qreal half = kHandleSize / 2;
    const qreal right = area.right() - kHandleSize;
    const qreal bottom = area.bottom() - kHandleSize;
    const QPointF center = area.center();
    return {
        QRectF(QPointF(area.left(), area.top()), size),
        QRectF(QPointF(right, area.top()), size),
        QRectF(QPointF(area.left(), bottom), size),
        QRectF(QPointF(right, bottom), size),
        QRectF(QPointF(area.left(), center.y() - half), size),
        QRectF(QPointF(right, center.y() - half), size),
        QRectF(QPointF(center.x() - half, area.top()), size),
        QRectF(QPointF(center.x() - half, bottom), size),
    };
}

bool CropController::handlesVisible() const {
    const QRectF area = selectionArea();
    return !mSelection.isDragging() &&
           area.width() * mDevicePixelRatio >= kHandlesMinSelectionDevicePx &&
           area.height() * mDevicePixelRatio >= kHandlesMinSelectionDevicePx;
}

Qt::CursorShape CropController::cursorShape() const {
    return mCursorShape;
}

//------------------------------------------------------------------------------
// Aspect ratio and panel
//------------------------------------------------------------------------------
CropController::AspectPreset CropController::aspectPreset() const {
    return mPreset;
}

double CropController::aspectWidth() const {
    return mAspectWidth;
}

double CropController::aspectHeight() const {
    return mAspectHeight;
}

SettingsEnums::CropAction CropController::defaultAction() const {
    return mDefaultAction;
}

void CropController::setSelectionValues(int x, int y, int width, int height) {
    if (!mActive)
        return;
    mSelection.place(QRect(x, y, width, height));
    // Always notified, so inputs holding a value that was moved into the
    // image show the stored one again.
    notifySelection();
}

void CropController::selectAspectPreset(AspectPreset preset) {
    mPreset = preset;
    applyPresetRatio();
    emit aspectChanged();
}

void CropController::setCustomAspect(double width, double height) {
    mPreset = AspectPreset::Custom;
    setAspectValues(width, height);
    if (mSelection.setAspectRatio(QPointF(width, height)))
        notifySelection();
    emit aspectChanged();
}

void CropController::swapAspect() {
    setCustomAspect(mAspectHeight, mAspectWidth);
}

void CropController::reset() {
    resetSelection();
}

void CropController::selectAll() {
    if (!mActive)
        return;
    mSelection.selectAll();
    notifySelection();
}

void CropController::crop() {
    const QRect rect = mSelection.rect();
    const bool accepted = acceptsCrop();
    close();
    if (accepted)
        emit cropRequested(rect);
}

void CropController::cropAndSave() {
    const QRect rect = mSelection.rect();
    const bool accepted = acceptsCrop();
    close();
    if (accepted)
        emit cropAndSaveRequested(rect);
}

void CropController::cropDefault() {
    if (mDefaultAction == SettingsEnums::CropAction::CropAndSave)
        cropAndSave();
    else
        crop();
}

void CropController::cancel() {
    close();
}

void CropController::chooseDefaultAction(SettingsEnums::CropAction action) {
    if (mDefaultAction != action) {
        mDefaultAction = action;
        emit defaultActionChanged();
    }
    emit defaultActionChosen(action);
}

void CropController::resetSelection() {
    mPreset = AspectPreset::Free;
    applyPresetRatio();
    mSelection.fitToAspectRatio();
    notifySelection();
    emit aspectChanged();
}

void CropController::applyPresetRatio() {
    const QSize image = imageSize();
    QPointF ratio;
    switch (mPreset) {
    case AspectPreset::Free:
        mSelection.setAspectLocked(false);
        if (!image.isEmpty())
            setAspectValues(ratioOf(image).x(), ratioOf(image).y());
        return;
    case AspectPreset::Custom: ratio = QPointF(mAspectWidth, mAspectHeight); break;
    case AspectPreset::CurrentImage:
        if (image.isEmpty())
            return;
        ratio = ratioOf(image);
        break;
    case AspectPreset::Screen:
        if (mScreenSize.isEmpty()) {
            qWarning() << "CropController: the screen size is unknown; the ratio stays"
                       << mAspectWidth << ":" << mAspectHeight;
            ratio = QPointF(mAspectWidth, mAspectHeight);
            break;
        }
        ratio = ratioOf(mScreenSize);
        break;
    case AspectPreset::Square: ratio = kSquareRatio; break;
    case AspectPreset::FourByThree: ratio = kFourByThreeRatio; break;
    case AspectPreset::SixteenByNine: ratio = kSixteenByNineRatio; break;
    case AspectPreset::SixteenByTen: ratio = kSixteenByTenRatio; break;
    }
    setAspectValues(ratio.x(), ratio.y());
    if (mSelection.setAspectRatio(ratio))
        notifySelection();
}

void CropController::setAspectValues(double width, double height) {
    mAspectWidth = width;
    mAspectHeight = height;
}

bool CropController::acceptsCrop() const {
    return mActive && hasSelection() && mSelection.rect().size() != imageSize();
}

//------------------------------------------------------------------------------
// Pointer
//------------------------------------------------------------------------------
qreal CropController::viewScale() const {
    const QSize image = imageSize();
    if (image.isEmpty())
        return 0;
    return mImageArea.width() / image.width();
}

QPoint CropController::imagePointAt(QPointF position) const {
    const qreal scale = viewScale();
    const QSize image = imageSize();
    if (scale <= 0)
        return {};
    const qreal x = qBound(mImageArea.left(), position.x(), mImageArea.right());
    const qreal y = qBound(mImageArea.top(), position.y(), mImageArea.bottom());
    const int imageX = static_cast<int>(std::floor((x - mImageArea.left()) / scale));
    const int imageY = static_cast<int>(std::floor((y - mImageArea.top()) / scale));
    return QPoint(qBound(0, imageX, image.width() - 1), qBound(0, imageY, image.height() - 1));
}

CropHandle CropController::handleAt(QPointF position) const {
    if (!hasSelection())
        return CropHandle::None;
    const QList<QRectF> squares = handles();
    for (qsizetype i = 0; i < squares.size(); ++i) {
        if (squares.at(i).contains(position))
            return kHandleOrder[i];
    }
    return selectionArea().contains(position) ? CropHandle::Move : CropHandle::None;
}

void CropController::pointerPressed(QPointF position, int button) {
    if (!mActive)
        return;
    if (button == Qt::RightButton) {
        mSelection.endDrag();
        mSelection.clear();
        notifySelection();
        showHoverCursor(position);
        return;
    }
    if (button != Qt::LeftButton)
        return;
    mPressPosition = position;
    if (!hasSelection()) {
        mSelection.beginNewSelection(imagePointAt(position));
    } else {
        const CropHandle handle = handleAt(position);
        mSelection.beginDrag(handle);
        setCursorShape(cursorFor(handle));
    }
    notifySelection();
}

void CropController::pointerMoved(QPointF position, int buttons) {
    if (!mActive)
        return;
    const qreal scale = viewScale();
    if (!(buttons & Qt::LeftButton) || !mSelection.isDragging() || scale <= 0) {
        showHoverCursor(position);
        return;
    }
    const CropHandle handle = mSelection.dragBy((position - mPressPosition) / scale);
    if (handle == CropHandle::Move)
        setCursorShape(Qt::ClosedHandCursor);
    else if (handle != CropHandle::None)
        setCursorShape(cursorFor(handle));
    notifySelection();
}

void CropController::pointerReleased(QPointF position) {
    if (!mActive)
        return;
    if (mSelection.isDragging()) {
        mSelection.endDrag();
        notifySelection();
    }
    showHoverCursor(position);
}

void CropController::setCursorShape(Qt::CursorShape shape) {
    if (mCursorShape == shape)
        return;
    mCursorShape = shape;
    emit cursorShapeChanged();
}

void CropController::showHoverCursor(QPointF position) {
    setCursorShape(cursorFor(handleAt(position)));
}

void CropController::notifySelection() {
    emit selectionChanged();
    emit geometryChanged();
}
