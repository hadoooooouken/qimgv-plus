#include "draggableslideroverlay.h"
#include "settings.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/customwidgets/iconwidget.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSlider>

DraggableSliderOverlay::DraggableSliderOverlay(FloatingWidgetContainer *parent)
    : OverlayWidget(parent)
{
}

DraggableSliderOverlay::~DraggableSliderOverlay() = default;

QWidget *DraggableSliderOverlay::createHeader(const QString &title, FluentIcon icon)
{
    QWidget *headerWidget = new QWidget(this);
    headerWidget->setAccessibleName("OverlayHeaderWidget");

    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setSpacing(0);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    IconWidget *headerIcon = new IconWidget(headerWidget);
    headerIcon->setAccessibleName("OverlayHeaderIcon");
    headerIcon->setIcon(icon, kHeaderIconSizePx);

    QLabel *titleLabel = new QLabel(title, headerWidget);
    titleLabel->setAccessibleName("OverlayHeaderLabel");
    titleLabel->setStyleSheet("font-weight: bold;");

    IconButton *closeButton = new IconButton(headerWidget);
    closeButton->setAccessibleName("OverlayHeaderButton");
    closeButton->setIcon(FluentIcon::Dismiss16, kCloseIconSizePx);
    connect(closeButton, &IconButton::clicked, this, &DraggableSliderOverlay::hide);

    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);

    return headerWidget;
}

void DraggableSliderOverlay::addSliderRow(QFormLayout *formLayout,
                                          const QString &labelText,
                                          QSlider *&slider,
                                          QLabel *&valLabel,
                                          int min, int max, int defaultValue)
{
    QHBoxLayout *rowLayout = new QHBoxLayout();
    rowLayout->setSpacing(6);

    slider = new QSlider(Qt::Horizontal, this);
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setValue(defaultValue);
    rowLayout->addWidget(slider);

    valLabel = new QLabel(this);
    valLabel->setMinimumWidth(45);
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(valLabel);

    QLabel *label = new QLabel(labelText, this);
    formLayout->addRow(label, rowLayout);
}

void DraggableSliderOverlay::setCustomPosition(const QPoint &globalPos)
{
    customGlobalPos = globalPos;
    hasCustomPos = true;
    recalculateGeometry();
}

void DraggableSliderOverlay::recalculateGeometry()
{
    if (hasCustomPos) {
        QWidget *p = parentWidget();
        if (p) {
            QPoint localPos = p->mapFromGlobal(customGlobalPos);
            QRect parentRect = p->rect();
            QSize sz = sizeHint();

            int x = qBound(0, localPos.x(), parentRect.width() - sz.width());
            int y = qBound(0, localPos.y(), parentRect.height() - sz.height());

            setGeometry(x, y, sz.width(), sz.height());
            return;
        }
    }
    OverlayWidget::recalculateGeometry();
}

void DraggableSliderOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragStartPosition = event->globalPosition().toPoint();
        dragStartWidgetPosition = pos();
        isDragging = true;
        event->accept();
    } else {
        OverlayWidget::mousePressEvent(event);
    }
}

void DraggableSliderOverlay::mouseMoveEvent(QMouseEvent *event)
{
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

void DraggableSliderOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    } else {
        OverlayWidget::mouseReleaseEvent(event);
    }
}
