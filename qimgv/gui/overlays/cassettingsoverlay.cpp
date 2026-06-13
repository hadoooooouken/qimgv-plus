#include "cassettingsoverlay.h"
#include "settings.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

CasSettingsOverlay::CasSettingsOverlay(FloatingWidgetContainer *parent)
    : DraggableSliderOverlay(parent)
{
    setupUi();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(30);
    connect(m_updateTimer, &QTimer::timeout, this, &CasSettingsOverlay::onTimerTimeout);

    connect(m_sharpenSlider, &QSlider::valueChanged, this, &CasSettingsOverlay::onSliderValueChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &CasSettingsOverlay::onSliderValueChanged);

    connect(m_sharpenSlider, &QSlider::sliderReleased, this, &CasSettingsOverlay::onSliderReleased);
    connect(m_contrastSlider, &QSlider::sliderReleased, this, &CasSettingsOverlay::onSliderReleased);

    if (parent) {
        setContainerSize(parent->size());
    }
}

CasSettingsOverlay::~CasSettingsOverlay() = default;

void CasSettingsOverlay::setupUi()
{
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setMinimumSize(380, 0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 12);
    mainLayout->setSpacing(8);

    // Header
    mainLayout->addLayout(createHeader(tr("CAS Settings")));

    // Content: use QFormLayout for auto-aligned labels
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignLeft);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    addSliderRow(formLayout, tr("Sharpening"), m_sharpenSlider, m_sharpenValLabel, 0, 100,
                 qRound(settings->casSharpening() * 100.0f));
    addSliderRow(formLayout, tr("Contrast"),   m_contrastSlider, m_contrastValLabel, 0, 100,
                 qRound(settings->casContrast() * 100.0f));

    mainLayout->addLayout(formLayout);

    // Reset button (right-aligned, like original)
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 4, 0, 0);
    QPushButton *resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setFocusPolicy(Qt::NoFocus);
    resetButton->setAccessibleName("Button");
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(resetButton);
    mainLayout->addLayout(buttonLayout);

    connect(resetButton, &QPushButton::clicked, this, &CasSettingsOverlay::onResetClicked);

    // Double-click reset
    m_sharpenSlider->installEventFilter(this);
    m_contrastSlider->installEventFilter(this);

    updateValueLabels();
}

void CasSettingsOverlay::updateValueLabels()
{
    m_sharpenValLabel->setText(QString::number(m_sharpenSlider->value() / 100.0f, 'f', 2));
    m_contrastValLabel->setText(QString::number(m_contrastSlider->value() / 100.0f, 'f', 2));
}

void CasSettingsOverlay::show()
{
    // Sync sliders with current settings values
    m_sharpenSlider->blockSignals(true);
    m_sharpenSlider->setValue(qRound(settings->casSharpening() * 100.0f));
    m_sharpenSlider->blockSignals(false);

    m_contrastSlider->blockSignals(true);
    m_contrastSlider->setValue(qRound(settings->casContrast() * 100.0f));
    m_contrastSlider->blockSignals(false);

    updateValueLabels();

    DraggableSliderOverlay::show();
    adjustSize();
    recalculateGeometry();
}

void CasSettingsOverlay::hide()
{
    DraggableSliderOverlay::hide();
}

void CasSettingsOverlay::onSliderValueChanged()
{
    updateValueLabels();

    bool dragging = m_sharpenSlider->isSliderDown() ||
                    m_contrastSlider->isSliderDown();

    if (!dragging) {
        float sharpening = m_sharpenSlider->value() / 100.0f;
        float contrast = m_contrastSlider->value() / 100.0f;
        settings->setCasSharpening(sharpening);
        settings->setCasContrast(contrast);
        emit casSettingsChanged(sharpening, contrast);
        m_pendingUpdate = false;
        m_updateTimer->stop();
    } else {
        if (!m_updateTimer->isActive()) {
            float sharpening = m_sharpenSlider->value() / 100.0f;
            float contrast = m_contrastSlider->value() / 100.0f;
            settings->setCasSharpening(sharpening);
            settings->setCasContrast(contrast);
            emit casSettingsChanged(sharpening, contrast);
            m_updateTimer->start();
            m_pendingUpdate = false;
        } else {
            m_pendingUpdate = true;
        }
    }
}

void CasSettingsOverlay::onTimerTimeout()
{
    if (m_pendingUpdate) {
        float sharpening = m_sharpenSlider->value() / 100.0f;
        float contrast = m_contrastSlider->value() / 100.0f;
        settings->setCasSharpening(sharpening);
        settings->setCasContrast(contrast);
        emit casSettingsChanged(sharpening, contrast);
        m_pendingUpdate = false;
    }
}

void CasSettingsOverlay::onSliderReleased()
{
    if (m_pendingUpdate) {
        float sharpening = m_sharpenSlider->value() / 100.0f;
        float contrast = m_contrastSlider->value() / 100.0f;
        settings->setCasSharpening(sharpening);
        settings->setCasContrast(contrast);
        emit casSettingsChanged(sharpening, contrast);
        m_pendingUpdate = false;
    }
    m_updateTimer->stop();
}

void CasSettingsOverlay::onResetClicked()
{
    m_sharpenSlider->blockSignals(true);
    m_sharpenSlider->setValue(100);
    m_sharpenSlider->blockSignals(false);

    m_contrastSlider->blockSignals(true);
    m_contrastSlider->setValue(0);
    m_contrastSlider->blockSignals(false);

    updateValueLabels();

    settings->setCasSharpening(1.0f);
    settings->setCasContrast(0.0f);

    emit casSettingsChanged(1.0f, 0.0f);
}

bool CasSettingsOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == m_sharpenSlider) {
            m_sharpenSlider->setValue(100);
            return true;
        } else if (watched == m_contrastSlider) {
            m_contrastSlider->setValue(0);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
