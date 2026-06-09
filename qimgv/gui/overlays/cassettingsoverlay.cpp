#include "cassettingsoverlay.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/customwidgets/iconwidget.h"
#include "settings.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

CasSettingsOverlay::CasSettingsOverlay(FloatingWidgetContainer *parent)
    : OverlayWidget(parent)
{
    setupUi();

    connect(m_sharpenSlider, &QSlider::valueChanged, this, &CasSettingsOverlay::onSliderValueChanged);
    connect(m_contrastSlider, &QSlider::valueChanged, this, &CasSettingsOverlay::onSliderValueChanged);

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

    // Header (same as ColorAdjustmentsOverlay / CasSettingsOverlay original style)
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    IconWidget *headerIcon = new IconWidget(this);
    headerIcon->setFixedSize(16, 16);
    headerIcon->setIconPath(":/res/icons/common/settings/appearance32.png");

    QLabel *titleLabel = new QLabel(tr("CAS Settings"), this);
    titleLabel->setStyleSheet("font-weight: bold;");

    IconButton *closeButton = new IconButton(this);
    closeButton->setFixedSize(16, 16);
    closeButton->setIconPath(":res/icons/common/overlay/close-dim16.png");
    connect(closeButton, &IconButton::clicked, this, &CasSettingsOverlay::hide);

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

    addSliderRow(tr("Sharpening"), m_sharpenSlider, m_sharpenValLabel, 0, 100,
                 qRound(settings->casSharpening() * 100.0f));
    addSliderRow(tr("Contrast"),   m_contrastSlider, m_contrastValLabel, 0, 100,
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

    OverlayWidget::show();
    adjustSize();
    recalculateGeometry();
}

void CasSettingsOverlay::hide()
{
    OverlayWidget::hide();
}

void CasSettingsOverlay::onSliderValueChanged()
{
    updateValueLabels();

    float sharpening = m_sharpenSlider->value() / 100.0f;
    float contrast = m_contrastSlider->value() / 100.0f;

    settings->setCasSharpening(sharpening);
    settings->setCasContrast(contrast);

    emit casSettingsChanged(sharpening, contrast);
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

void CasSettingsOverlay::setCustomPosition(const QPoint &globalPos)
{
    customGlobalPos = globalPos;
    hasCustomPos = true;
    recalculateGeometry();
}

void CasSettingsOverlay::recalculateGeometry()
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

void CasSettingsOverlay::mousePressEvent(QMouseEvent *event)
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

void CasSettingsOverlay::mouseMoveEvent(QMouseEvent *event)
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

void CasSettingsOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    } else {
        OverlayWidget::mouseReleaseEvent(event);
    }
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
