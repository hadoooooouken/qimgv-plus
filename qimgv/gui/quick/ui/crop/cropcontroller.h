#pragma once

#include <QList>
#include <QObject>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/settingsenums.h"
#include "gui/quick/bridges/uisettings.h"
#include "gui/quick/ui/crop/cropselection.h"

// The crop mode of the Qt Quick UI: the side panel (CropPanel.qml) and the
// selection overlay (CropOverlay.qml), with the rules of the widget MW,
// CropPanel and CropOverlay:
// - the mode opens only in the document view while an image is shown, and
//   the folder view closes it;
// - opening and every new image start in the free ratio with the whole
//   image selected;
// - the panel's inputs show the selection; edited values move into the
//   image;
// - the aspect presets: free (unlocked; the inputs show the image's ratio),
//   custom (the inputs), the current image, the screen, 1:1, 4:3, 16:9 and
//   16:10; picking one other than free locks and fits the selection; editing
//   or swapping the inputs switches to custom;
// - crop and crop-and-save close the mode and are requested only for a
//   selection that is not empty and not the whole image size, otherwise they
//   just close it; the default action (Enter) is the one picked with a right
//   click on its button and stored in the settings;
// - pointer: a press without a selection starts a new one, a press on a
//   handle or inside the selection resizes or moves it, the right button
//   clears it; handles are drawn while nothing is dragged and the selection
//   is at least 90 device pixels on both sides.
// The selection itself follows CropSelection. The overlay reports pointer
// positions in viewport coordinates (logical pixels); imageArea is where the
// image is drawn there.
//
// Owned by the Quick UI host; the application side (QuickCropActions) runs
// the side effects of opening and closing on the viewer. GUI thread only.
class CropController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY imageSizeChanged FINAL)
    Q_PROPERTY(QRect selection READ selection NOTIFY selectionChanged FINAL)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QRectF imageArea READ imageArea NOTIFY geometryChanged FINAL)
    Q_PROPERTY(QRectF selectionArea READ selectionArea NOTIFY geometryChanged FINAL)
    Q_PROPERTY(QList<QRectF> handles READ handles NOTIFY geometryChanged FINAL)
    Q_PROPERTY(bool handlesVisible READ handlesVisible NOTIFY geometryChanged FINAL)
    Q_PROPERTY(Qt::CursorShape cursorShape READ cursorShape NOTIFY cursorShapeChanged FINAL)
    Q_PROPERTY(AspectPreset aspectPreset READ aspectPreset NOTIFY aspectChanged FINAL)
    Q_PROPERTY(double aspectWidth READ aspectWidth NOTIFY aspectChanged FINAL)
    Q_PROPERTY(double aspectHeight READ aspectHeight NOTIFY aspectChanged FINAL)
    Q_PROPERTY(SettingsEnums::CropAction defaultAction READ defaultAction NOTIFY defaultActionChanged FINAL)

public:
    // In the order of the panel's preset list.
    enum class AspectPreset {
        Free,
        Custom,
        CurrentImage,
        Screen,
        Square,
        FourByThree,
        SixteenByNine,
        SixteenByTen,
    };
    Q_ENUM(AspectPreset)

    // Edge of a handle square, logical pixels (8 device pixels at 2x in the
    // widget overlay, which counted twice that).
    static constexpr qreal kHandleSize = 16.0;
    // Handles are drawn only for selections at least this large on screen.
    static constexpr qreal kHandlesMinSelectionDevicePx = 90.0;

    // settings: the initial UI settings (applySettings()).
    explicit CropController(const UiSettingsSnapshot &settings, QObject *parent = nullptr);

    // --- mode -------------------------------------------------------------
    [[nodiscard]] bool isActive() const;
    // Opens the mode (in the document view, with an image) or closes it.
    void toggle();
    void close();
    void setFolderViewActive(bool active);
    void applySettings(const UiSettingsSnapshot &settings);

    // --- image and view ---------------------------------------------------
    [[nodiscard]] QSize imageSize() const;
    // The shown image's size in pixels; an empty size means no image. While
    // open, a new image resets the preset to free and selects the whole
    // image.
    void setImageSize(QSize size);
    [[nodiscard]] QRectF imageArea() const;
    // Where the image is drawn, viewport coordinates (logical pixels).
    void setImageArea(const QRectF &area);
    void setDevicePixelRatio(qreal ratio);
    // Size of the screen the window is on (the "This Screen" preset).
    void setScreenSize(QSize size);

    // --- selection --------------------------------------------------------
    [[nodiscard]] QRect selection() const;
    [[nodiscard]] bool hasSelection() const;
    [[nodiscard]] QRectF selectionArea() const;
    // Handle squares, viewport coordinates: top left, top right, bottom
    // left, bottom right, left, right, top, bottom.
    [[nodiscard]] QList<QRectF> handles() const;
    [[nodiscard]] bool handlesVisible() const;
    [[nodiscard]] Qt::CursorShape cursorShape() const;

    // --- aspect ratio -----------------------------------------------------
    [[nodiscard]] AspectPreset aspectPreset() const;
    [[nodiscard]] double aspectWidth() const;
    [[nodiscard]] double aspectHeight() const;
    [[nodiscard]] SettingsEnums::CropAction defaultAction() const;

    // --- panel (QML) ------------------------------------------------------
    Q_INVOKABLE void setSelectionValues(int x, int y, int width, int height);
    Q_INVOKABLE void selectAspectPreset(AspectPreset preset);
    Q_INVOKABLE void setCustomAspect(double width, double height);
    Q_INVOKABLE void swapAspect();
    // Back to the free ratio with the whole image selected.
    Q_INVOKABLE void reset();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void crop();
    Q_INVOKABLE void cropAndSave();
    // Runs the default action.
    Q_INVOKABLE void cropDefault();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void chooseDefaultAction(SettingsEnums::CropAction action);

    // --- overlay pointer (QML), viewport coordinates ----------------------
    Q_INVOKABLE void pointerPressed(QPointF position, int button);
    Q_INVOKABLE void pointerMoved(QPointF position, int buttons);
    Q_INVOKABLE void pointerReleased(QPointF position);

signals:
    void activeChanged();
    void imageSizeChanged();
    void selectionChanged();
    void geometryChanged();
    void cursorShapeChanged();
    void aspectChanged();
    void defaultActionChanged();
    void cropRequested(const QRect &rect);
    void cropAndSaveRequested(const QRect &rect);
    // The user picked the default action; the settings store it.
    void defaultActionChosen(SettingsEnums::CropAction action);

private:
    void setActive(bool active);
    // Free preset, whole image (opening, new image, reset).
    void resetSelection();
    void applyPresetRatio();
    void setAspectValues(double width, double height);
    [[nodiscard]] bool acceptsCrop() const;
    // Logical pixels per image pixel.
    [[nodiscard]] qreal viewScale() const;
    [[nodiscard]] QPoint imagePointAt(QPointF position) const;
    [[nodiscard]] CropHandle handleAt(QPointF position) const;
    void setCursorShape(Qt::CursorShape shape);
    void showHoverCursor(QPointF position);
    void notifySelection();

    CropSelection mSelection;
    bool mActive = false;
    bool mFolderViewActive = false;
    QRectF mImageArea;
    qreal mDevicePixelRatio = 1.0;
    QSize mScreenSize;
    AspectPreset mPreset = AspectPreset::Free;
    double mAspectWidth = 1.0;
    double mAspectHeight = 1.0;
    SettingsEnums::CropAction mDefaultAction = SettingsEnums::CropAction::Crop;
    Qt::CursorShape mCursorShape = Qt::ArrowCursor;
    QPointF mPressPosition;
};
