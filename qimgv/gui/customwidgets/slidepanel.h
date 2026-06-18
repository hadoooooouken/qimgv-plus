#pragma once

#include "floatingwidget.h"
#include "settings_types.h"
#include <QBoxLayout>
#include <QDebug>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QTimeLine>
#include <QTimer>
#include <QtGlobal>
#include <ctime>
#include <memory>


class SlidePanel : public FloatingWidget {
  Q_OBJECT
public:
  explicit SlidePanel(FloatingWidgetContainer *parent);
  ~SlidePanel();
  bool hasWidget();
  void setWidget(std::shared_ptr<QWidget> w);
  // Use visibleGeometry instead of geometry() here.
  // If this is called mid-animation then geometry() will be all wrong.
  QRect triggerRect();
  // when this is set, the widget will not change geometry by itself
  // no pos() animations & no recalculateGeometry()
  bool layoutManaged();
  void setLayoutManaged(bool mode);

  void setPosition(PanelPosition);
  PanelPosition position();
  void hideAnimated();
  void showAnimated();

public slots:
  void show();
  void hide();

private slots:
  void onAnimationFinish();
  void animationUpdate(int frame);

protected:
  QHBoxLayout mLayout;
  QGraphicsOpacityEffect *fadeEffect;
  int panelSize, slideAmount;
  std::shared_ptr<QWidget> mWidget;
  QRect mTriggerRect;
  void setAnimationRange(QPoint start, QPoint end);
  void saveStaticGeometry(QRect geometry);
  QRect staticGeometry();
  QTimer timer;
  QTimeLine timeline;
  QEasingCurve outCurve;
  const int ANIMATION_DURATION = 300;
  bool mHiding = true;
  PanelPosition mPosition;
  void recalculateGeometry();
  virtual void updateTriggerRect();

private:
  void setOrientation();
  Qt::Orientation mOrientation;
  QRect mStaticGeometry;
  qreal panelVisibleOpacity = 1.0;
  QPoint startPosition, endPosition;
  bool mLayoutManaged = false;

signals:
  void animationStarted();
  void animationFinished();
};
