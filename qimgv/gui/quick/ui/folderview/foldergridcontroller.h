#pragma once

#include <QColor>
#include <QFont>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include "gui/quick/bridges/uisettings.h"
#include "gui/quick/ui/folderview/foldergridlayout.h"
#include "gui/quick/ui/thumbnails/thumbnaillistmodel.h"

// The thumbnail grid of the folder view (FolderGridView in the widget UI):
// lays the items of a ThumbnailListModel out in rows, configures the model
// (request size, crop, unloading, scrolling) and owns the grid's input
// rules, so that QML only draws and reports:
// - pointer: selection on press and activation by double click (the model's
//   rules), the rubber band over the background (Ctrl toggles against the
//   selection held at its start), the context menu on a right click that
//   was no scroll gesture, Ctrl + wheel zoom;
// - keyboard: arrows (Up / Down keep the column a short last row moved
//   away from), Page Up / Down by four rows, Home / End, Shift ranges,
//   Ctrl+A, Enter activates, Backspace goes up, printable text goes to the
//   type-ahead; everything else is left to the action shortcuts;
// - icon size: 128 - 512, the zoom slider on a log2 scale snapping to 256,
//   stored when the slider is released;
// - drops: copy and move onto items or the background;
// - the number of selected images for the batch conversion button.
// Requests leave as signals; the Quick UI host connects them to the
// directory view, Settings and the actions. GUI thread only.
class FolderGridController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(ThumbnailListModel *model READ model CONSTANT FINAL)
    Q_PROPERTY(FolderGridLayout layout READ layout NOTIFY layoutChanged FINAL)
    Q_PROPERTY(int iconSize READ iconSize NOTIFY iconSizeChanged FINAL)
    Q_PROPERTY(int zoomSliderValue READ zoomSliderValue NOTIFY iconSizeChanged FINAL)
    Q_PROPERTY(int zoomSliderMinimum READ zoomSliderMinimum CONSTANT FINAL)
    Q_PROPERTY(int zoomSliderMaximum READ zoomSliderMaximum CONSTANT FINAL)
    Q_PROPERTY(QRectF rubberBand READ rubberBand NOTIFY rubberBandChanged FINAL)
    Q_PROPERTY(int selectedImageCount READ selectedImageCount NOTIFY selectedImageCountChanged FINAL)

public:
    // Distance beyond the visible rows in which thumbnails are loaded
    // (offscreenPreloadArea of the widget grid).
    static constexpr int kPreloadDistance = 2300;
    // Rows that Page Up / Page Down move.
    static constexpr int kPageRows = 4;
    // Zoom slider: hundredths of log2 of the icon size; values within
    // kSliderSnapRange of the snap point give 256 px.
    static constexpr int kSliderScale = 100;
    static constexpr int kSliderSnapValue = 800;
    static constexpr int kSliderSnapRange = 8;

    // model must outlive the controller; settings: the initial UI settings.
    FolderGridController(ThumbnailListModel &model, const UiSettingsSnapshot &settings,
                         QObject *parent = nullptr);

    [[nodiscard]] ThumbnailListModel *model() const;
    [[nodiscard]] FolderGridLayout layout() const;
    [[nodiscard]] int iconSize() const;
    [[nodiscard]] int zoomSliderValue() const;
    [[nodiscard]] static int zoomSliderMinimum();
    [[nodiscard]] static int zoomSliderMaximum();
    // The rubber band in view coordinates; empty while none is dragged.
    [[nodiscard]] QRectF rubberBand() const;
    [[nodiscard]] int selectedImageCount() const;

    // --- configuration (Quick UI host) -----------------------------------
    void applySettings(const UiSettingsSnapshot &settings);
    void setLabelFont(const QFont &font);
    // Thumbnails are requested at the icon size times this ratio.
    void setDevicePixelRatio(qreal ratio);

    // --- view (QML) ------------------------------------------------------
    // Width of the grid area without the scroll bar.
    Q_INVOKABLE void setViewWidth(qreal width);
    Q_INVOKABLE void setIconSize(int size);
    Q_INVOKABLE void setZoomSliderValue(int value);
    // The icon size is stored when the slider is released.
    Q_INVOKABLE void setZoomSliderPressed(bool pressed);
    // Pointer events of the view: index is the item under the pointer (-1
    // for none), button a Qt::MouseButton, buttons Qt::MouseButtons,
    // modifiers Qt::KeyboardModifiers, position in view coordinates.
    Q_INVOKABLE void press(int index, int button, int modifiers, QPointF position);
    Q_INVOKABLE void move(QPointF position, int buttons, int modifiers);
    Q_INVOKABLE void release(int index, int button, QPointF position);
    Q_INVOKABLE void doubleClick(int index, int button);
    Q_INVOKABLE void wheel(QPoint angleDelta, QPoint pixelDelta, int modifiers);
    // A key press (Qt::Key, Qt::KeyboardModifiers, the typed text); returns
    // false when the action shortcuts should handle it.
    Q_INVOKABLE bool keyPressed(int key, int modifiers, const QString &text);
    Q_INVOKABLE void keyReleased(int key);
    // The grid lost the keyboard focus or was hidden.
    Q_INVOKABLE void focusLost();
    // Drag and drop over the grid: index is the item under the pointer,
    // action a Qt::DropAction.
    [[nodiscard]] Q_INVOKABLE static bool acceptsDropAction(int action);
    Q_INVOKABLE void dragMoved(int index);
    Q_INVOKABLE void dragLeft();
    Q_INVOKABLE void drop(const QList<QUrl> &urls, QObject *source, int index, int action);
    // Context menu rows.
    Q_INVOKABLE void openSelected();
    Q_INVOKABLE void requestBatchConversion();
    // Black or white, whichever reads on background (unselected labels).
    [[nodiscard]] Q_INVOKABLE static QColor labelTextColor(const QColor &background);

    // Selects every item; the current item stays current.
    void selectAll();

signals:
    void layoutChanged();
    void iconSizeChanged();
    void rubberBandChanged();
    void selectedImageCountChanged();
    // The icon size changed by the user; to be stored.
    void iconSizeCommitted(int size);
    // An application action to run ("goUp").
    void actionRequested(const QString &name);
    void typeAheadRequested(const QString &text);
    // A right click that was no gesture, at position (view coordinates).
    void contextMenuRequested(QPointF position);
    void draggedOver(int index);
    void urlsDropped(const QList<QUrl> &urls, QObject *source, int index, Qt::DropAction action);
    void openSelectedRequested();
    void batchConversionRequested();

private:
    [[nodiscard]] bool checkRange(int index) const;
    [[nodiscard]] int current() const;
    [[nodiscard]] int itemAbove(int index) const;
    [[nodiscard]] int itemBelow(int index) const;
    [[nodiscard]] int columnOf(int index) const;
    [[nodiscard]] bool sameRow(int one, int two) const;

    void selectOrExtend(int index);
    void selectAbove();
    void selectBelow();
    void selectNext();
    void selectPrevious();
    void pageUp();
    void pageDown();
    void selectFirst();
    void selectLast();

    void updateRubberBand(QPointF position, Qt::KeyboardModifiers modifiers);
    void endRubberBand();

    void updateLayout();
    void configureModel();
    void updateSelectedImageCount();

    ThumbnailListModel &mModel;
    FolderViewSettings mFolderView;
    PanelSettings mPanel;
    ViewerSettings mViewer;
    FolderGridLayout mLayout;
    QFont mLabelFont;
    qreal mDevicePixelRatio = 1.0;
    int mViewWidth = 0;
    int mIconSize = FolderGridLayout::kMinimumIconSize;
    bool mSliderPressed = false;
    // Column of the item before Down moved into a shorter last row.
    int mShiftedColumn = -1;
    int mSelectedImageCount = 0;
    // Item under the pointer of the drag in progress, -1 for none.
    int mDragTarget = -1;

    bool mRubberBandActive = false;
    // Start of the rubber band in content coordinates.
    QPointF mRubberBandOrigin;
    QRectF mRubberBand;
    QList<int> mRubberBandStartSelection;
};
