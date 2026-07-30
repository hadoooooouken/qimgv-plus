#pragma once

#include <QApplication>
#include <QLabel>
#include <QBoxLayout>
#include <QScrollArea>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QPainter>
#include <QResizeEvent>
#include <cmath>

#include "gui/customwidgets/thumbnailview.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "sourcecontainers/thumbnail.h"

class ThumbnailStrip : public ThumbnailView
{
    Q_OBJECT
public:
    explicit ThumbnailStrip(QWidget *parent = nullptr);
    QSize itemSize() const;
    void readSettings();

private:
    int lastThumbnailResolution = 256;
    const int thumbPadding = 9;
    int thumbMarginX = 2, thumbMarginY = 4;
    void setupLayout();
    ThumbnailStyle mCurrentStyle = THUMB_SIMPLE;
    mutable QSize cachedItemSize;

public slots:
    virtual void focusOn(int index);
    virtual void focusOnSelection();

protected:
    virtual void resizeEvent(QResizeEvent *event);
    virtual void updateScrollbarIndicator();
    std::unique_ptr<ThumbnailWidget> createThumbnailWidget() override;
    QRectF itemGeometry(int index) const override;
    QSizeF contentSize() const override;
    QPair<int, int> itemRangeForRect(const QRectF &rect) const override;
    int widgetPoolCapacity() const override;
};
