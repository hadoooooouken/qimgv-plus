#pragma once

#include <QGraphicsWidget>
#include <QGraphicsItem>
#include <QGraphicsLayoutItem>
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QPaintEngine>
#include <cmath>
#include "sourcecontainers/thumbnail.h"
#include "utils/imagelib.h"
#include "sharedresources.h"

enum ThumbnailStyle {
    THUMB_SIMPLE,
    THUMB_NORMAL,
    THUMB_NORMAL_CENTERED
};

class QTimeLine;

class ThumbnailWidget : public QGraphicsWidget {
    Q_OBJECT
    Q_PROPERTY(qreal hoverOpacity READ getHoverOpacity WRITE setHoverOpacity)

public:
    ThumbnailWidget(QGraphicsItem *parent = nullptr);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    bool isLoaded;
    void setThumbnail(std::shared_ptr<Thumbnail> _thumbnail);

    void setHighlighted(bool mode);
    bool isHighlighted();
    void setUseThumbPanelColors(bool mode) { mUseThumbPanelColors = mode; }
    bool isUseThumbPanelColors() const { return mUseThumbPanelColors; }
    void setDropHovered(bool mode);
    bool isDropHovered();

    virtual QRectF boundingRect() const override;

    qreal width();
    qreal height();
    void setThumbnailSize(int size);

    void setGeometry(const QRectF &rect) override;

    virtual QRectF geometry() const;
    QSizeF effectiveSizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;
    void setThumbStyle(ThumbnailStyle _style);
    void setPadding(int _padding);
    void setMargins(int _marginX, int _marginY);
    int thumbnailSize();
    void reset();
    void unsetThumbnail();

protected:
    void setupTextLayout();
    void drawThumbnail(QPainter* painter, const QPixmap *pixmap);
    void drawIcon(QPainter *painter, const QPixmap *pixmap);
    void drawHighlight(QPainter *painter);
    void drawHoverBg(QPainter *painter);
    void drawHoverHighlight(QPainter *painter);
    void drawLabel(QPainter *painter);
    void drawDropHover(QPainter *painter);
    void drawSingleLineText(QPainter *painter, QRect rect, QString text, const QColor &color);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item, QWidget *widget) override;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    void updateGeometry() override;
    void setHovered(bool);
    bool isHovered();
    qreal getHoverOpacity() const { return hoverOpacity; }
    void setHoverOpacity(qreal opacity);
    void updateBackgroundRect();
    void updateThumbnailDrawPosition();
    void updateDpr(qreal newDpr);

    std::shared_ptr<Thumbnail> thumbnail;
    bool highlighted, hovered, dropHovered;
    int mThumbnailSize, padding, marginX, marginY, labelSpacing, textHeight;
    QRectF bgRect, mBoundingRect;
    QFont font, fontInfo;
    QRect drawRectCentered, nameRect, infoRect;
    bool mUseThumbPanelColors = false;
    qreal dpr = 1.0;
    qreal hoverOpacity = 0.0;
    QTimeLine *hoverTimeline = nullptr;
    void updateBoundingRect();
    ThumbnailStyle thumbStyle;
};
