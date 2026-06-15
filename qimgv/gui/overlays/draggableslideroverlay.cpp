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

QHBoxLayout *DraggableSliderOverlay::createHeader(const QString &title)
{
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    IconWidget *headerIcon = new IconWidget(this);
    headerIcon->setFixedSize(16, 16);
    headerIcon->setIconPath(":/res/icons/common/settings/appearance32.png");

    QLabel *titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet("font-weight: bold;");

    IconButton *closeButton = new IconButton(this);
    closeButton->setFixedSize(16, 16);
    closeButton->setIconPath(":/res/icons/common/overlay/close-dim16.png");
    connect(closeButton, &IconButton::clicked, this, &DraggableSliderOverlay::hide);

    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);

    return headerLayout;
}

void DraggableSliderOverlay::addSliderRow(QFormLayout *formLayout,
                                          const QString &labelText,
                                          QSlider *&slider,
                                          QLabel *&valLabel,
                                          int min, int max, int defaultValue)
{
    QHBoxLayout *rowLayout = new QHBoxLayout();
    rowLayout->setSpacing(6);

    IconButton *minusBtn = new IconButton(this);
    minusBtn->setFixedSize(18, 18);
    minusBtn->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    rowLayout->addWidget(minusBtn);

    slider = new QSlider(Qt::Horizontal, this);
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setValue(defaultValue);
    rowLayout->addWidget(slider);

    IconButton *plusBtn = new IconButton(this);
    plusBtn->setFixedSize(18, 18);
    plusBtn->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    rowLayout->addWidget(plusBtn);

    valLabel = new QLabel(this);
    valLabel->setMinimumWidth(45);
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rowLayout->addWidget(valLabel);

    connect(minusBtn, &IconButton::clicked, this, [slider]() {
        slider->setValue(slider->value() - 5);
    });
    connect(plusBtn, &IconButton::clicked, this, [slider]() {
        slider->setValue(slider->value() + 5);
    });

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
