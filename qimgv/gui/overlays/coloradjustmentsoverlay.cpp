#include "coloradjustmentsoverlay.h"
#include "ui_coloradjustmentsoverlay.h"
#include <QMouseEvent>

ColorAdjustmentsOverlay::ColorAdjustmentsOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::ColorAdjustmentsOverlay)
{
    ui->setupUi(this);
    ui->closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    ui->headerIcon->setIconPath(":/res/icons/common/settings/appearance32.png");

    // Set icon paths for +/- step buttons
    ui->brightnessMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->brightnessPlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->contrastMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->contrastPlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->saturationMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->saturationPlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->hueMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->huePlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->exposureMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->exposurePlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->temperatureMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->temperaturePlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");
    ui->tintMinus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-out18.png");
    ui->tintPlus->setIconPath(":/res/icons/common/buttons/contextmenu/zoom-in18.png");

    connect(ui->closeButton, &IconButton::clicked, this, &ColorAdjustmentsOverlay::hide);
    connect(ui->resetButton, &QPushButton::clicked, this, &ColorAdjustmentsOverlay::resetAdjustments);
    connect(ui->applyButton, &QPushButton::clicked, this, [this]() {
        emit applyRequested(brightness(), contrast(), saturation(), hue(), exposure(), temperature(), tint());
        resetAdjustments();
    });

    connect(ui->brightnessSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(ui->contrastSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(ui->saturationSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(ui->hueSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(ui->exposureSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(ui->temperatureSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(ui->tintSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);

    // Connect clicked signals for +/- step buttons
    connect(ui->brightnessMinus, &IconButton::clicked, this, [this]() {
        ui->brightnessSlider->setValue(ui->brightnessSlider->value() - 5);
    });
    connect(ui->brightnessPlus, &IconButton::clicked, this, [this]() {
        ui->brightnessSlider->setValue(ui->brightnessSlider->value() + 5);
    });
    connect(ui->contrastMinus, &IconButton::clicked, this, [this]() {
        ui->contrastSlider->setValue(ui->contrastSlider->value() - 5);
    });
    connect(ui->contrastPlus, &IconButton::clicked, this, [this]() {
        ui->contrastSlider->setValue(ui->contrastSlider->value() + 5);
    });
    connect(ui->saturationMinus, &IconButton::clicked, this, [this]() {
        ui->saturationSlider->setValue(ui->saturationSlider->value() - 5);
    });
    connect(ui->saturationPlus, &IconButton::clicked, this, [this]() {
        ui->saturationSlider->setValue(ui->saturationSlider->value() + 5);
    });
    connect(ui->hueMinus, &IconButton::clicked, this, [this]() {
        ui->hueSlider->setValue(ui->hueSlider->value() - 10);
    });
    connect(ui->huePlus, &IconButton::clicked, this, [this]() {
        ui->hueSlider->setValue(ui->hueSlider->value() + 10);
    });
    connect(ui->exposureMinus, &IconButton::clicked, this, [this]() {
        ui->exposureSlider->setValue(ui->exposureSlider->value() - 10);
    });
    connect(ui->exposurePlus, &IconButton::clicked, this, [this]() {
        ui->exposureSlider->setValue(ui->exposureSlider->value() + 10);
    });
    connect(ui->temperatureMinus, &IconButton::clicked, this, [this]() {
        ui->temperatureSlider->setValue(ui->temperatureSlider->value() - 5);
    });
    connect(ui->temperaturePlus, &IconButton::clicked, this, [this]() {
        ui->temperatureSlider->setValue(ui->temperatureSlider->value() + 5);
    });
    connect(ui->tintMinus, &IconButton::clicked, this, [this]() {
        ui->tintSlider->setValue(ui->tintSlider->value() - 5);
    });
    connect(ui->tintPlus, &IconButton::clicked, this, [this]() {
        ui->tintSlider->setValue(ui->tintSlider->value() + 5);
    });

    if (parent) {
        setContainerSize(parent->size());
    }
}

ColorAdjustmentsOverlay::~ColorAdjustmentsOverlay() {
    delete ui;
}

void ColorAdjustmentsOverlay::setCustomPosition(const QPoint &globalPos) {
    customGlobalPos = globalPos;
    hasCustomPos = true;
    recalculateGeometry();
}

void ColorAdjustmentsOverlay::recalculateGeometry() {
    if (hasCustomPos) {
        QWidget *p = parentWidget();
        if (p) {
            QPoint localPos = p->mapFromGlobal(customGlobalPos);
            QRect parentRect = p->rect();
            QSize size = minimumSize();

            // Adjust position so it fits fully within the parent bounds
            int x = qBound(0, localPos.x(), parentRect.width() - size.width());
            int y = qBound(0, localPos.y(), parentRect.height() - size.height());

            setGeometry(x, y, size.width(), size.height());
            return;
        }
    }
    OverlayWidget::recalculateGeometry();
}

float ColorAdjustmentsOverlay::brightness() const {
    return ui->brightnessSlider->value() / 100.0f;
}

float ColorAdjustmentsOverlay::contrast() const {
    return ui->contrastSlider->value() / 100.0f;
}

float ColorAdjustmentsOverlay::saturation() const {
    return ui->saturationSlider->value() / 100.0f;
}

float ColorAdjustmentsOverlay::hue() const {
    return ui->hueSlider->value();
}

float ColorAdjustmentsOverlay::exposure() const {
    return ui->exposureSlider->value() / 100.0f;
}

float ColorAdjustmentsOverlay::temperature() const {
    return ui->temperatureSlider->value() / 100.0f;
}

float ColorAdjustmentsOverlay::tint() const {
    return ui->tintSlider->value() / 100.0f;
}

void ColorAdjustmentsOverlay::show() {
    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void ColorAdjustmentsOverlay::hide() {
    OverlayWidget::hide();
}

void ColorAdjustmentsOverlay::resetAdjustments() {
    ui->brightnessSlider->blockSignals(true);
    ui->contrastSlider->blockSignals(true);
    ui->saturationSlider->blockSignals(true);
    ui->hueSlider->blockSignals(true);
    ui->exposureSlider->blockSignals(true);
    ui->temperatureSlider->blockSignals(true);
    ui->tintSlider->blockSignals(true);

    ui->brightnessSlider->setValue(0);
    ui->contrastSlider->setValue(100);
    ui->saturationSlider->setValue(100);
    ui->hueSlider->setValue(0);
    ui->exposureSlider->setValue(0);
    ui->temperatureSlider->setValue(0);
    ui->tintSlider->setValue(0);

    ui->brightnessSlider->blockSignals(false);
    ui->contrastSlider->blockSignals(false);
    ui->saturationSlider->blockSignals(false);
    ui->hueSlider->blockSignals(false);
    ui->exposureSlider->blockSignals(false);
    ui->temperatureSlider->blockSignals(false);
    ui->tintSlider->blockSignals(false);

    ui->brightnessValLabel->setText("0%");
    ui->contrastValLabel->setText("100%");
    ui->saturationValLabel->setText("100%");
    ui->hueValLabel->setText("0°");
    ui->exposureValLabel->setText("+0.00");
    ui->temperatureValLabel->setText("0");
    ui->tintValLabel->setText("0");

    emit adjustmentsChanged(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

void ColorAdjustmentsOverlay::onSliderValueChanged() {
    ui->brightnessValLabel->setText(QString::number(ui->brightnessSlider->value()) + "%");
    ui->contrastValLabel->setText(QString::number(ui->contrastSlider->value()) + "%");
    ui->saturationValLabel->setText(QString::number(ui->saturationSlider->value()) + "%");
    ui->hueValLabel->setText(QString::number(ui->hueSlider->value()) + "°");

    float expVal = ui->exposureSlider->value() / 100.0f;
    QString expStr = (expVal >= 0.0f ? "+" : "") + QString::number(expVal, 'f', 2);
    ui->exposureValLabel->setText(expStr);

    ui->temperatureValLabel->setText(QString::number(ui->temperatureSlider->value()));
    ui->tintValLabel->setText(QString::number(ui->tintSlider->value()));

    emit adjustmentsChanged(brightness(), contrast(), saturation(), hue(), exposure(), temperature(), tint());
}

void ColorAdjustmentsOverlay::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragStartPosition = event->globalPosition().toPoint();
        dragStartWidgetPosition = this->pos();
        isDragging = true;
        event->accept();
    } else {
        OverlayWidget::mousePressEvent(event);
    }
}

void ColorAdjustmentsOverlay::mouseMoveEvent(QMouseEvent *event) {
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->globalPosition().toPoint() - dragStartPosition;
        QPoint newPos = dragStartWidgetPosition + delta;

        QWidget *p = parentWidget();
        if (p) {
            QRect parentRect = p->rect();
            QSize size = sizeHint();

            // Adjust position so it fits fully within the parent bounds
            newPos.setX(qBound(0, newPos.x(), parentRect.width() - size.width()));
            newPos.setY(qBound(0, newPos.y(), parentRect.height() - size.height()));

            move(newPos);
            customGlobalPos = p->mapToGlobal(newPos);
        }
        event->accept();
    } else {
        OverlayWidget::mouseMoveEvent(event);
    }
}

void ColorAdjustmentsOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    } else {
        OverlayWidget::mouseReleaseEvent(event);
    }
}

// Force rebuild to trigger UIC for coloradjustmentsoverlay.ui

