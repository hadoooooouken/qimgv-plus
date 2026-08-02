#pragma once

/* This class manages the logical item count, a viewport-sized pool of
 * ThumbnailWidget instances, scrolling, geometry, and thumbnail state.
 *
 * Subclasses provide the mathematical geometry for their layout style.
 */

#include <QGraphicsView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QWheelEvent>
#include <QTimeLine>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QPair>
#include <QScreen>
#include <QSet>

#include <functional>
#include <memory>

#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/idirectoryview.h"
#include "shortcutbuilder.h"

enum ThumbnailSelectMode {
    ACTIVATE_BY_PRESS,
    ACTIVATE_BY_DOUBLECLICK
};

enum ScrollDirection {
    SCROLL_FORWARDS,
    SCROLL_BACKWARDS
};

enum ThumbInteractionState {
    THUMB_INTERACTION_NONE,
    THUMB_INTERACTION_GESTURE
};

class ThumbnailView : public QGraphicsView, public IDirectoryView {
    Q_OBJECT
    Q_INTERFACES(IDirectoryView)
public:
    ThumbnailView(Qt::Orientation orient, QWidget *parent = nullptr);
    virtual void setDirectoryPath(QString path) override;
    void select(QList<int>) override;
    void select(int) override;
    QList<int> selection() override;
    int itemCount() const;

    void setSelectMode(ThumbnailSelectMode mode);
    int lastSelected();
    void clearSelection();
    void deselect(int index);
    void unloadAllThumbnails();
    void setBlockThumbnailLoading(bool block);

public slots:
    void show();
    void showEvent(QShowEvent *event) override;
    void resetViewport();
    int thumbnailSize();
    void loadVisibleThumbnails();
    void loadVisibleThumbnailsDelayed();

    void addItem();

    virtual void focusOnSelection() = 0;
    virtual void populate(int count) override;
    virtual void setThumbnail(int pos, std::shared_ptr<Thumbnail> thumb) override;
    void setThumbnailUnavailable(int pos, int size) override;
    virtual void insertItem(int index) override;
    virtual void removeItem(int index) override;
    virtual void reloadItem(int index) override;
    virtual void setDragHover(int index) override;

signals:
    void itemActivated(int) override;
    void thumbnailsRequested(QList<int>, int, bool, bool) override;
    void visibleThumbnailsReady();
    void draggedOut() override;
    void draggedToBookmarks(QList<int>) override;
    void draggedOver(int) override;
    void droppedInto(const QMimeData*, QObject*, int, Qt::DropAction) override;
    void backRequested() override;
    void forwardRequested() override;
    void openSelectedRequested() override;
    void selectionChanged();

private:
    QTimer loadTimer;
    bool blockThumbnailLoading;

    int mDrawScrollbarIndicator, lastScrollFrameTime;
    QList<int> mSelection;

    bool mCropThumbnails, mouseReleaseSelect;
    ThumbnailSelectMode selectMode;
    QPoint dragStartPos;
    ThumbnailWidget* dragTarget;

    void createScrollTimeLine();
    QElapsedTimer scrollFrameTimer;
    std::function<void(int)> centerOn;
    QElapsedTimer lastTouchpadScroll;
    Qt::Orientation mOrientation = Qt::Horizontal;

protected:
    QGraphicsScene scene;
    // Widgets are scene-owned pool entries. Their model indices live in the
    // explicit binding maps below and must never be inferred from list order.
    QList<ThumbnailWidget*> thumbnails;
    QScrollBar *scrollBar;
    QTimeLine *scrollTimeLine;
    QPointF viewportCenter;
    int mThumbnailSize;
    int offscreenPreloadArea = 3000;

    QList<int> rangeSelectionSnapshot;
    QList<int> rubberBandStartSelection;
    bool rangeSelection; // true if shift is pressed
    bool wayland = false;

    QRect indicator;
    const int indicatorSize = 2;

    int scrollRefreshRate = 16;
    const int SCROLL_DURATION = 120;
    const float WHEEL_SCROLL_MULTIPLIER = 2.5f;
    const float SCROLL_ACCELERATION = 1.4f;
    const int SCROLL_ACCELERATION_THRESHOLD = 50;

    const uint LOAD_DELAY = 150;
    ScrollDirection lastScrollDirection = SCROLL_FORWARDS;
    ThumbInteractionState mouseInteraction = THUMB_INTERACTION_NONE;
    const int gestureThreshold = 40;

    bool atSceneStart();
    bool atSceneEnd();

    bool checkRange(int pos) const;

    virtual std::unique_ptr<ThumbnailWidget> createThumbnailWidget() = 0;
    virtual QRectF itemGeometry(int index) const = 0;
    virtual QSizeF contentSize() const = 0;
    virtual QPair<int, int> itemRangeForRect(const QRectF &rect) const = 0;
    virtual int widgetPoolCapacity() const = 0;
    virtual void updateScrollbarIndicator() = 0;

    void updateLayout();
    void fitSceneToContents();
    void refreshVisibleItems(bool clearBindings = false);
    ThumbnailWidget *widgetForIndex(int index) const;
    int indexForWidget(const ThumbnailWidget *widget) const;

    void setOrientation(Qt::Orientation _orientation);
    Qt::Orientation orientation() const;

    void setCropThumbnails(bool);
    void setDrawScrollbarIndicator(bool mode);

    void addSelectionRange(int indexTo);

    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;

    bool eventFilter(QObject *o, QEvent *ev) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollToItem(int index);
    void scrollToEdge(bool end);
    void scrollPrecise(int delta);
    void scrollByItem(int delta);
    void scrollSmooth(int delta);
    void scrollSmooth(int angleDelta, qreal multiplier, qreal acceleration);
    void scrollSmooth(int angleDelta, qreal multiplier, qreal acceleration, bool additive);
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    int mItemCount = 0;
    QHash<int, ThumbnailWidget*> boundWidgets;
    QHash<const ThumbnailWidget*, int> widgetIndices;
    QHash<int, std::shared_ptr<Thumbnail>> loadedThumbnails;
    QSet<int> pendingThumbnailRequests;
    QSet<int> unavailableThumbnails;
    bool layoutUpdateInProgress = false;
    bool visibleThumbnailsReadyReported = false;

    QRectF preloadRect() const;
    void bindWidget(ThumbnailWidget *widget, int index);
    void unbindWidget(ThumbnailWidget *widget);
    void trimWidgetPool();
    void shiftBoundItems(int firstIndex, int offset);
    void shiftCachedItems(int firstIndex, int offset);
    bool visibleThumbnailsLoaded() const;
    void notifyIfVisibleThumbnailsReady();
};
