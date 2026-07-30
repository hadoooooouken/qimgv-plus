#pragma once

#include "components/actionmanager/actionmanager.h"
#include "gui/customwidgets/thumbnailview.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "utils/stuff.h"

class FolderGridView : public ThumbnailView {
  Q_OBJECT
public:
  explicit FolderGridView(QWidget *parent = nullptr);

  const int THUMBNAIL_SIZE_MIN = 128; // px
  const int THUMBNAIL_SIZE_MAX = 512; // these should be divisible by ZOOM_STEP
  const int ZOOM_STEP = 16;
  void selectAll();

public slots:
  void show();
  void hide();

  void selectFirst();
  void selectLast();
  void pageUp();
  void pageDown();
  void selectAbove();
  void selectBelow();
  void selectNext();
  void selectPrev();

  void zoomIn();
  void zoomOut();
  void setThumbnailSize(int newSize);
  void setShowLabels(bool mode);
  virtual void focusOn(int index) override;
  virtual void focusOnSelection() override;
  virtual void setDragHover(int index) override;

private:
  struct GridGeometry {
    QSizeF itemSize;
    int columns;
    int rows;
    qreal horizontalOffset;
  };

  int shiftedCol;
  void scrollToCurrent();
  GridGeometry gridGeometry() const;
  QSizeF thumbnailItemSize() const;
  int itemAbove(int index) const;
  int itemBelow(int index) const;
  int columnOf(int index) const;
  bool sameRow(int one, int two) const;
  int lastDragTarget = -1;
  ThumbnailStyle thumbnailStyle = THUMB_NORMAL;
  mutable QSizeF cachedItemSize;

private slots:
  void onitemSelected();

protected:
  void resizeEvent(QResizeEvent *event) override;
  virtual void updateScrollbarIndicator() override;
  void setupLayout();
  std::unique_ptr<ThumbnailWidget> createThumbnailWidget() override;
  QRectF itemGeometry(int index) const override;
  QSizeF contentSize() const override;
  QPair<int, int> itemRangeForRect(const QRectF &rect) const override;
  int widgetPoolCapacity() const override;

  void keyPressEvent(QKeyEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void dropEvent(QDropEvent *event) override;

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  bool focusNextPrevChild(bool) override;

signals:
  void thumbnailSizeChanged(int);
  void batchRequested();
  void openSelectedRequested();
};
