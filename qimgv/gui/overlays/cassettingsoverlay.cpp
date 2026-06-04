#include "cassettingsoverlay.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/viewers/viewerwidget.h"
#include "settings.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

CasSettingsOverlay::CasSettingsOverlay(FloatingWidgetContainer *parent)
    : OverlayWidget(parent) {
  setAccessibleName("CasSettingsOverlay");

  // Main layout
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(12, 8, 12, 12);
  mainLayout->setSpacing(8);

  // Header row: icon + title + close button
  auto *headerLayout = new QHBoxLayout();
  headerLayout->setContentsMargins(0, 0, 0, 0);

  auto *headerIcon = new IconWidget(this);
  headerIcon->setMinimumSize(16, 16);
  headerIcon->setMaximumSize(16, 16);
  headerIcon->setIconPath(":/res/icons/common/settings/appearance32.png");

  auto *headerLabel = new QLabel(tr("CAS Settings"), this);
  headerLabel->setStyleSheet("font-weight: bold;");

  auto *closeButton = new IconButton(this);
  closeButton->setMinimumSize(16, 16);
  closeButton->setMaximumSize(16, 16);
  closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
  connect(closeButton, &IconButton::clicked, this, &CasSettingsOverlay::hide);

  headerLayout->addWidget(headerIcon);
  headerLayout->addWidget(headerLabel);
  headerLayout->addStretch(1);
  headerLayout->addWidget(closeButton);
  mainLayout->addLayout(headerLayout);

  // --- CAS Sharpen slider row ---
  auto *sharpenLayout = new QHBoxLayout();
  sharpenLayout->setSpacing(6);

  auto *sharpenLabel = new QLabel(tr("Sharpening"), this);
  sharpenLabel->setMinimumWidth(80);

  auto *sharpenMinus = new IconButton(this);
  sharpenMinus->setMinimumSize(18, 18);
  sharpenMinus->setMaximumSize(18, 18);
  sharpenMinus->setIconPath(
      ":/res/icons/common/buttons/contextmenu/zoom-out18.png");

  sharpenSlider = new QSlider(Qt::Horizontal, this);
  sharpenSlider->setMinimum(0);
  sharpenSlider->setMaximum(100);
  sharpenSlider->setValue(qRound(settings->casSharpening() * 100.0f));

  auto *sharpenPlus = new IconButton(this);
  sharpenPlus->setMinimumSize(18, 18);
  sharpenPlus->setMaximumSize(18, 18);
  sharpenPlus->setIconPath(
      ":/res/icons/common/buttons/contextmenu/zoom-in18.png");

  sharpenValLabel = new QLabel(this);
  sharpenValLabel->setMinimumWidth(40);
  sharpenValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  sharpenValLabel->setText(QString::number(sharpenSlider->value() / 100.f, 'f', 2));

  sharpenLayout->addWidget(sharpenLabel);
  sharpenLayout->addWidget(sharpenMinus);
  sharpenLayout->addWidget(sharpenSlider);
  sharpenLayout->addWidget(sharpenPlus);
  sharpenLayout->addWidget(sharpenValLabel);
  mainLayout->addLayout(sharpenLayout);

  // --- CAS Contrast slider row ---
  auto *contrastLayout = new QHBoxLayout();
  contrastLayout->setSpacing(6);

  auto *contrastLabel = new QLabel(tr("Contrast"), this);
  contrastLabel->setMinimumWidth(80);

  auto *contrastMinus = new IconButton(this);
  contrastMinus->setMinimumSize(18, 18);
  contrastMinus->setMaximumSize(18, 18);
  contrastMinus->setIconPath(
      ":/res/icons/common/buttons/contextmenu/zoom-out18.png");

  contrastSlider = new QSlider(Qt::Horizontal, this);
  contrastSlider->setMinimum(0);
  contrastSlider->setMaximum(100);
  contrastSlider->setValue(qRound(settings->casContrast() * 100.0f));

  auto *contrastPlus = new IconButton(this);
  contrastPlus->setMinimumSize(18, 18);
  contrastPlus->setMaximumSize(18, 18);
  contrastPlus->setIconPath(
      ":/res/icons/common/buttons/contextmenu/zoom-in18.png");

  contrastValLabel = new QLabel(this);
  contrastValLabel->setMinimumWidth(40);
  contrastValLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  contrastValLabel->setText(QString::number(contrastSlider->value() / 100.f, 'f', 2));

  contrastLayout->addWidget(contrastLabel);
  contrastLayout->addWidget(contrastMinus);
  contrastLayout->addWidget(contrastSlider);
  contrastLayout->addWidget(contrastPlus);
  contrastLayout->addWidget(contrastValLabel);
  mainLayout->addLayout(contrastLayout);

  // --- Reset button ---
  auto *buttonLayout = new QHBoxLayout();
  buttonLayout->setContentsMargins(0, 4, 0, 0);
  auto *resetButton = new QPushButton(tr("Reset"), this);
  resetButton->setAccessibleName("Button");
  buttonLayout->addStretch(1);
  buttonLayout->addWidget(resetButton);
  mainLayout->addLayout(buttonLayout);

  // Event filters for double-click reset
  sharpenSlider->installEventFilter(this);
  contrastSlider->installEventFilter(this);

  // Connect slider signals
  connect(sharpenSlider, &QSlider::valueChanged, this,
          &CasSettingsOverlay::onSliderValueChanged);
  connect(contrastSlider, &QSlider::valueChanged, this,
          &CasSettingsOverlay::onSliderValueChanged);

  // Connect +/- buttons
  connect(sharpenMinus, &IconButton::clicked, this,
          [this]() { sharpenSlider->setValue(sharpenSlider->value() - 5); });
  connect(sharpenPlus, &IconButton::clicked, this,
          [this]() { sharpenSlider->setValue(sharpenSlider->value() + 5); });
  connect(contrastMinus, &IconButton::clicked, this,
          [this]() { contrastSlider->setValue(contrastSlider->value() - 5); });
  connect(contrastPlus, &IconButton::clicked, this,
          [this]() { contrastSlider->setValue(contrastSlider->value() + 5); });

  // Connect reset
  connect(resetButton, &QPushButton::clicked, this,
          &CasSettingsOverlay::onResetClicked);

  setMinimumSize(320, 150);
  setMaximumSize(320, 150);

  if (parent) {
    setContainerSize(parent->size());
  }
}

CasSettingsOverlay::~CasSettingsOverlay() {}

void CasSettingsOverlay::setCustomPosition(const QPoint &globalPos) {
  customGlobalPos = globalPos;
  hasCustomPos = true;
  recalculateGeometry();
}

void CasSettingsOverlay::recalculateGeometry() {
  if (hasCustomPos) {
    QWidget *p = parentWidget();
    if (p) {
      QPoint localPos = p->mapFromGlobal(customGlobalPos);
      QRect parentRect = p->rect();
      QSize sz = minimumSize();

      int x = qBound(0, localPos.x(), parentRect.width() - sz.width());
      int y = qBound(0, localPos.y(), parentRect.height() - sz.height());

      setGeometry(x, y, sz.width(), sz.height());
      return;
    }
  }
  OverlayWidget::recalculateGeometry();
}

void CasSettingsOverlay::show() {
  // Sync sliders with current settings values
  sharpenSlider->blockSignals(true);
  sharpenSlider->setValue(qRound(settings->casSharpening() * 100.0f));
  sharpenSlider->blockSignals(false);
  sharpenValLabel->setText(QString::number(sharpenSlider->value() / 100.f, 'f', 2));

  contrastSlider->blockSignals(true);
  contrastSlider->setValue(qRound(settings->casContrast() * 100.0f));
  contrastSlider->blockSignals(false);
  contrastValLabel->setText(QString::number(contrastSlider->value() / 100.f, 'f', 2));

  OverlayWidget::show();
  adjustSize();
  recalculateGeometry();
}

void CasSettingsOverlay::hide() { OverlayWidget::hide(); }

void CasSettingsOverlay::onSliderValueChanged() {
  sharpenValLabel->setText(QString::number(sharpenSlider->value() / 100.f, 'f', 2));
  contrastValLabel->setText(QString::number(contrastSlider->value() / 100.f, 'f', 2));

  float sharpening = sharpenSlider->value() / 100.0f;
  float contrast = contrastSlider->value() / 100.0f;

  settings->setCasSharpening(sharpening);
  settings->setCasContrast(contrast);

  emit casSettingsChanged(sharpening, contrast);
}

void CasSettingsOverlay::onResetClicked() {
  sharpenSlider->blockSignals(true);
  sharpenSlider->setValue(100);
  sharpenSlider->blockSignals(false);
  sharpenValLabel->setText("1.00");

  contrastSlider->blockSignals(true);
  contrastSlider->setValue(0);
  contrastSlider->blockSignals(false);
  contrastValLabel->setText("0.00");

  settings->setCasSharpening(1.0f);
  settings->setCasContrast(0.0f);

  emit casSettingsChanged(1.0f, 0.0f);
}

void CasSettingsOverlay::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    dragStartPosition = event->globalPosition().toPoint();
    dragStartWidgetPosition = this->pos();
    isDragging = true;
    event->accept();
  } else {
    OverlayWidget::mousePressEvent(event);
  }
}

void CasSettingsOverlay::mouseMoveEvent(QMouseEvent *event) {
  if (isDragging && (event->buttons() & Qt::LeftButton)) {
    QPoint delta = event->globalPosition().toPoint() - dragStartPosition;
    QPoint newPos = dragStartWidgetPosition + delta;

    QWidget *p = parentWidget();
    if (p) {
      QRect parentRect = p->rect();
      QSize sz = sizeHint();

      newPos.setX(qBound(0, newPos.x(), parentRect.width() - sz.width()));
      newPos.setY(qBound(0, newPos.y(), parentRect.height() - sz.height()));

      move(newPos);
      customGlobalPos = p->mapToGlobal(newPos);
    }
    event->accept();
  } else {
    OverlayWidget::mouseMoveEvent(event);
  }
}

void CasSettingsOverlay::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    isDragging = false;
    event->accept();
  } else {
    OverlayWidget::mouseReleaseEvent(event);
  }
}

bool CasSettingsOverlay::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::MouseButtonDblClick) {
    if (watched == sharpenSlider) {
      sharpenSlider->setValue(100); // default sharpening = 1.0
      return true;
    } else if (watched == contrastSlider) {
      contrastSlider->setValue(0); // default contrast = 0.0
      return true;
    }
  }
  return QWidget::eventFilter(watched, event);
}
