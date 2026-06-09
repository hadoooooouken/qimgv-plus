#include "coloradjustmentsoverlay.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/customwidgets/iconwidget.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

ColorAdjustmentsOverlay::ColorAdjustmentsOverlay(FloatingWidgetContainer *parent)
    : OverlayWidget(parent)
{
    setupUi();

    connect(m_brightnessSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(m_contrastSlider,   &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(m_saturationSlider, &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(m_hueSlider,        &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(m_exposureSlider,   &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(m_temperatureSlider,&QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);
    connect(m_tintSlider,       &QSlider::valueChanged, this, &ColorAdjustmentsOverlay::onSliderValueChanged);

    if (parent) {
        setContainerSize(parent->size());
    }
}

ColorAdjustmentsOverlay::~ColorAdjustmentsOverlay() = default;

void ColorAdjustmentsOverlay::setupUi()
{
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setMinimumSize(420, 0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 12);
    mainLayout->setSpacing(8);

    // Header (same as CasSettingsOverlay)
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    IconWidget *headerIcon = new IconWidget(this);
    headerIcon->setFixedSize(16, 16);
    headerIcon->setIconPath(":/res/icons/common/settings/appearance32.png");

    QLabel *titleLabel = new QLabel(tr("Color adjustments"), this);
    titleLabel->setStyleSheet("font-weight: bold;");

    IconButton *closeButton = new IconButton(this);
    closeButton->setFixedSize(16, 16);
    closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    connect(closeButton, &IconButton::clicked, this, &ColorAdjustmentsOverlay::hide);

    headerLayout->addWidget(headerIcon);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    mainLayout->addLayout(headerLayout);

    // Content: use QFormLayout for auto-aligned labels
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto addSliderRow = [&](const QString &labelText,
                            QSlider *&slider,
                            QLabel *&valLabel,
                            int min, int max, int defaultValue) {
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
    };

    addSliderRow(tr("Brightness"),  m_brightnessSlider,  m_brightnessValLabel,  -100, 100, 0);
    addSliderRow(tr("Contrast"),    m_contrastSlider,    m_contrastValLabel,       0, 300, 100);
    addSliderRow(tr("Saturation"),  m_saturationSlider,  m_saturationValLabel,     0, 200, 100);
    addSliderRow(tr("Hue"),         m_hueSlider,         m_hueValLabel,          -180, 180, 0);
    addSliderRow(tr("Exposure"),    m_exposureSlider,    m_exposureValLabel,      -300, 300, 0);
    addSliderRow(tr("Temperature"), m_temperatureSlider, m_temperatureValLabel,   -50,  50, 0);
    addSliderRow(tr("Tint"),        m_tintSlider,        m_tintValLabel,          -50,  50, 0);

    mainLayout->addLayout(formLayout);

    // Button row
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    buttonLayout->setSpacing(6);

    QPushButton *compareButton = new QPushButton(tr("Compare"), this);
    compareButton->setFocusPolicy(Qt::NoFocus);
    compareButton->setAccessibleName("Button");

    QPushButton *applyButton = new QPushButton(tr("Apply"), this);
    applyButton->setFocusPolicy(Qt::NoFocus);
    applyButton->setAccessibleName("Button");

    QPushButton *resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setFocusPolicy(Qt::NoFocus);
    resetButton->setAccessibleName("Button");

    buttonLayout->addWidget(compareButton);
    buttonLayout->addWidget(applyButton);
    buttonLayout->addWidget(resetButton);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(resetButton, &QPushButton::clicked, this, &ColorAdjustmentsOverlay::resetAdjustments);
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        emit applyRequested(brightness(), contrast(), saturation(),
                            hue(), exposure(), temperature(), tint());
        resetAdjustments();
    });
    connect(compareButton, &QPushButton::pressed, this, [this]() {
        emit adjustmentsChanged(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    });
    connect(compareButton, &QPushButton::released, this, [this]() {
        emit adjustmentsChanged(brightness(), contrast(), saturation(),
                                hue(), exposure(), temperature(), tint());
    });

    // Double-click reset
    m_brightnessSlider->installEventFilter(this);
    m_contrastSlider->installEventFilter(this);
    m_saturationSlider->installEventFilter(this);
    m_hueSlider->installEventFilter(this);
    m_exposureSlider->installEventFilter(this);
    m_temperatureSlider->installEventFilter(this);
    m_tintSlider->installEventFilter(this);

    updateValueLabels();
}

void ColorAdjustmentsOverlay::updateValueLabels()
{
    m_brightnessValLabel->setText(QString::number(m_brightnessSlider->value()) + "%");
    m_contrastValLabel->setText(QString::number(m_contrastSlider->value()) + "%");
    m_saturationValLabel->setText(QString::number(m_saturationSlider->value()) + "%");
    m_hueValLabel->setText(QString::number(m_hueSlider->value()) + "°");

    float expVal = m_exposureSlider->value() / 100.0f;
    QString expStr = (expVal >= 0.0f ? "+" : "") + QString::number(expVal, 'f', 2);
    m_exposureValLabel->setText(expStr);

    m_temperatureValLabel->setText(QString::number(m_temperatureSlider->value()));
    m_tintValLabel->setText(QString::number(m_tintSlider->value()));
}

float ColorAdjustmentsOverlay::brightness() const { return m_brightnessSlider->value() / 100.0f; }
float ColorAdjustmentsOverlay::contrast()  const { return m_contrastSlider->value()   / 100.0f; }
float ColorAdjustmentsOverlay::saturation()const { return m_saturationSlider->value() / 100.0f; }
float ColorAdjustmentsOverlay::hue()       const { return m_hueSlider->value(); }
float ColorAdjustmentsOverlay::exposure()  const { return m_exposureSlider->value()  / 100.0f; }
float ColorAdjustmentsOverlay::temperature()const { return m_temperatureSlider->value() / 100.0f; }
float ColorAdjustmentsOverlay::tint()      const { return m_tintSlider->value() / 100.0f; }

void ColorAdjustmentsOverlay::show()
{
    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void ColorAdjustmentsOverlay::hide()
{
    OverlayWidget::hide();
}

void ColorAdjustmentsOverlay::resetAdjustments()
{
    m_brightnessSlider->blockSignals(true);
    m_contrastSlider->blockSignals(true);
    m_saturationSlider->blockSignals(true);
    m_hueSlider->blockSignals(true);
    m_exposureSlider->blockSignals(true);
    m_temperatureSlider->blockSignals(true);
    m_tintSlider->blockSignals(true);

    m_brightnessSlider->setValue(0);
    m_contrastSlider->setValue(100);
    m_saturationSlider->setValue(100);
    m_hueSlider->setValue(0);
    m_exposureSlider->setValue(0);
    m_temperatureSlider->setValue(0);
    m_tintSlider->setValue(0);

    m_brightnessSlider->blockSignals(false);
    m_contrastSlider->blockSignals(false);
    m_saturationSlider->blockSignals(false);
    m_hueSlider->blockSignals(false);
    m_exposureSlider->blockSignals(false);
    m_temperatureSlider->blockSignals(false);
    m_tintSlider->blockSignals(false);

    updateValueLabels();

    emit adjustmentsChanged(0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

void ColorAdjustmentsOverlay::onSliderValueChanged()
{
    updateValueLabels();
    emit adjustmentsChanged(brightness(), contrast(), saturation(),
                            hue(), exposure(), temperature(), tint());
}

void ColorAdjustmentsOverlay::setCustomPosition(const QPoint &globalPos)
{
    customGlobalPos = globalPos;
    hasCustomPos = true;
    recalculateGeometry();
}

void ColorAdjustmentsOverlay::recalculateGeometry()
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

void ColorAdjustmentsOverlay::mousePressEvent(QMouseEvent *event)
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

void ColorAdjustmentsOverlay::mouseMoveEvent(QMouseEvent *event)
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

void ColorAdjustmentsOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    } else {
        OverlayWidget::mouseReleaseEvent(event);
    }
}

bool ColorAdjustmentsOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == m_brightnessSlider)  m_brightnessSlider->setValue(0);
        else if (watched == m_contrastSlider) m_contrastSlider->setValue(100);
        else if (watched == m_saturationSlider) m_saturationSlider->setValue(100);
        else if (watched == m_hueSlider)    m_hueSlider->setValue(0);
        else if (watched == m_exposureSlider) m_exposureSlider->setValue(0);
        else if (watched == m_temperatureSlider) m_temperatureSlider->setValue(0);
        else if (watched == m_tintSlider)   m_tintSlider->setValue(0);
        else return QWidget::eventFilter(watched, event);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
