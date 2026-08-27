#include "formatfiltercombobox.h"

#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QSet>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

#include "utils/formatgroups.h"

FormatFilterComboBox::FormatFilterComboBox(QWidget *parent)
    : StyledComboBox(parent),
      mPopup(nullptr),
      mAllFormatsCheckBox(nullptr)
{
    setCurrentIndex(-1);
    createPopup();
    setProperty("active", false);
    updateDisplayLabel();
}

QStringList FormatFilterComboBox::checkedExtensions() const {
    if (mAllFormatsCheckBox->isChecked())
        return {};

    QStringList result;
    for (int i = 0; i < mFormatCheckBoxes.size(); ++i) {
        if (mFormatCheckBoxes.at(i)->isChecked())
            result << mFormatExtensions.at(i);
    }
    return result;
}

void FormatFilterComboBox::setCheckedExtensions(QStringList extensions) {
    QSet<QString> wanted;
    for (const QString &ext : extensions)
        wanted.insert(ext.toLower());

    if (wanted.isEmpty()) {
        resetToAllFormats();
    } else {
        mAllFormatsCheckBox->setChecked(false);
        bool matchedAny = false;
        for (int i = 0; i < mFormatCheckBoxes.size(); ++i) {
            bool checked = false;
            for (const QString &ext : mFormatExtensions.at(i)) {
                if (wanted.contains(ext)) {
                    checked = true;
                    break;
                }
            }
            mFormatCheckBoxes.at(i)->setChecked(checked);
            matchedAny = matchedAny || checked;
        }
        if (!matchedAny)
            resetToAllFormats();
    }

    updateSelectionDisplay();
}

QSize FormatFilterComboBox::minimumSizeHint() const {
    QSize result = StyledComboBox::minimumSizeHint();
    result.setWidth(qMax(result.width(), requiredControlWidth()));
    return result;
}

bool FormatFilterComboBox::anyFormatChecked() const {
    for (const QCheckBox *checkBox : mFormatCheckBoxes) {
        if (checkBox->isChecked())
            return true;
    }
    return false;
}

void FormatFilterComboBox::createPopup() {
    mPopup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    mPopup->setFrameShape(QFrame::StyledPanel);
    mPopup->setFrameShadow(QFrame::Plain);

    auto *layout = new QVBoxLayout(mPopup);
    layout->setContentsMargins(kPopupMarginPx, kPopupMarginPx,
                               kPopupMarginPx, kPopupMarginPx);
    layout->setSpacing(kPopupSpacingPx);

    mAllFormatsCheckBox = new QCheckBox(tr("All formats"), mPopup);
    mAllFormatsCheckBox->setChecked(true);
    {
        QFont f = mAllFormatsCheckBox->font();
        f.setBold(true);
        f.setPointSize(QGuiApplication::font().pointSize() + 1);
        mAllFormatsCheckBox->setFont(f);
    }
    layout->addWidget(mAllFormatsCheckBox);
    connect(mAllFormatsCheckBox, &QCheckBox::clicked, this,
            [this](bool) { handleAllFormatsClicked(); });

    auto *separator = new QFrame(mPopup);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    const QVector<FormatCategory> &categories = allFormatCategories();
    for (int categoryIndex = 0; categoryIndex < categories.size(); ++categoryIndex) {
        const FormatCategory &category = categories.at(categoryIndex);
        auto *categoryCheckBox = new QCheckBox(category.label, mPopup);
        QFont categoryFont = categoryCheckBox->font();
        categoryFont.setBold(true);
        categoryFont.setPointSize(QGuiApplication::font().pointSize() + 1);
        categoryCheckBox->setFont(categoryFont);
        categoryCheckBox->setTristate(true);
        layout->addWidget(categoryCheckBox);
        mCategoryCheckBoxes.append(categoryCheckBox);
        mCategoryFormatIndexes.append(QVector<int>{});
        connect(categoryCheckBox, &QCheckBox::clicked, this,
                [this, categoryIndex](bool checked) {
                    handleCategoryClicked(categoryIndex, checked);
                });

        auto *grid = new QGridLayout;
        grid->setContentsMargins(kFormatGridIndentPx, 0, 0, 0);
        grid->setHorizontalSpacing(kPopupSpacingPx);
        grid->setVerticalSpacing(kPopupSpacingPx);
        for (int column = 0; column < kFormatColumns; ++column)
            grid->setColumnMinimumWidth(column, kFormatColumnWidthPx);

        for (int groupIndex = 0; groupIndex < category.groups.size(); ++groupIndex) {
            const FormatGroup &group = category.groups.at(groupIndex);
            const int formatIndex = mFormatCheckBoxes.size();
            auto *formatCheckBox = new QCheckBox(group.label, mPopup);
            formatCheckBox->setFixedWidth(kFormatColumnWidthPx);
            mFormatCheckBoxes.append(formatCheckBox);
            mFormatExtensions.append(group.extensions);
            mCategoryFormatIndexes.last().append(formatIndex);
            connect(formatCheckBox, &QCheckBox::clicked, this,
                    [this, formatIndex](bool checked) {
                        handleFormatClicked(formatIndex, checked);
                    });

            const int row = groupIndex / kFormatColumns;
            const int column = groupIndex % kFormatColumns;
            grid->addWidget(formatCheckBox, row, column);
        }

        const int usedColumns = category.groups.size() % kFormatColumns;
        if (usedColumns != 0) {
            const int finalRow = category.groups.size() / kFormatColumns;
            for (int column = usedColumns; column < kFormatColumns; ++column) {
                auto *emptyCell = new QWidget(mPopup);
                QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                sizePolicy.setRetainSizeWhenHidden(true);
                emptyCell->setSizePolicy(sizePolicy);
                emptyCell->setFixedWidth(kFormatColumnWidthPx);
                emptyCell->hide();
                grid->addWidget(emptyCell, finalRow, column);
            }
        }

        layout->addLayout(grid);
    }
}

void FormatFilterComboBox::resetToAllFormats() {
    mAllFormatsCheckBox->setChecked(true);
    for (QCheckBox *checkBox : mFormatCheckBoxes)
        checkBox->setChecked(false);
}

void FormatFilterComboBox::updateCategoryCheckStates() {
    for (int categoryIndex = 0; categoryIndex < mCategoryCheckBoxes.size(); ++categoryIndex) {
        const QVector<int> &formatIndexes = mCategoryFormatIndexes.at(categoryIndex);
        int checkedCount = 0;
        for (int formatIndex : formatIndexes) {
            if (mFormatCheckBoxes.at(formatIndex)->isChecked())
                ++checkedCount;
        }

        QCheckBox *categoryCheckBox = mCategoryCheckBoxes.at(categoryIndex);
        if (checkedCount == 0)
            categoryCheckBox->setCheckState(Qt::Unchecked);
        else if (checkedCount == formatIndexes.size())
            categoryCheckBox->setCheckState(Qt::Checked);
        else
            categoryCheckBox->setCheckState(Qt::PartiallyChecked);
    }
}

void FormatFilterComboBox::updateSelectionDisplay() {
    updateCategoryCheckStates();
    updateDisplayLabel();
}

void FormatFilterComboBox::applySelection() {
    updateSelectionDisplay();
    emit formatSelectionChanged(checkedExtensions());
}

void FormatFilterComboBox::handleAllFormatsClicked() {
    resetToAllFormats();
    applySelection();
}

void FormatFilterComboBox::handleFormatClicked(int formatIndex, bool checked) {
    if (mAllFormatsCheckBox->isChecked()) {
        mAllFormatsCheckBox->setChecked(false);
        for (int index = 0; index < mFormatCheckBoxes.size(); ++index)
            mFormatCheckBoxes.at(index)->setChecked(index == formatIndex && checked);
    }

    if (!anyFormatChecked())
        resetToAllFormats();

    applySelection();
}

void FormatFilterComboBox::handleCategoryClicked(int categoryIndex, bool checked) {
    mAllFormatsCheckBox->setChecked(false);
    for (int formatIndex : mCategoryFormatIndexes.at(categoryIndex))
        mFormatCheckBoxes.at(formatIndex)->setChecked(checked);

    if (!anyFormatChecked())
        resetToAllFormats();

    applySelection();
}

void FormatFilterComboBox::updateDisplayLabel() {
    int checkedFormats = 0;
    int lastCheckedIndex = -1;
    for (int i = 0; i < mFormatCheckBoxes.size(); ++i) {
        if (mFormatCheckBoxes.at(i)->isChecked()) {
            ++checkedFormats;
            lastCheckedIndex = i;
        }
    }

    bool active;
    if (checkedFormats == 0) {
        mDisplayText = tr("All formats");
        active = false;
    } else if (checkedFormats == 1) {
        mDisplayText = mFormatCheckBoxes.at(lastCheckedIndex)->text();
        active = true;
    } else {
        mDisplayText = tr("Custom");
        active = true;
    }

    setProperty("active", active);
    style()->unpolish(this);
    style()->polish(this);
    refreshIcon();
    updateGeometry();
    update();
}

void FormatFilterComboBox::paintEvent(QPaintEvent *e) {
    StyledComboBox::paintEvent(e);

    QPainter painter(this);
    const int trailingWidth = iconAreaWidth() + kTextIconSpacingPx;
    QRect textRect = rect().adjusted(kTextLeftPadding, 0, -trailingWidth, 0);
    painter.setPen(palette().color(QPalette::ButtonText));
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                     fontMetrics().elidedText(mDisplayText, Qt::ElideRight, textRect.width()));
}

void FormatFilterComboBox::showPopup() {
    if (mPopup->isVisible()) {
        mPopup->hide();
        return;
    }

    mPopup->adjustSize();
    QSize popupSize = mPopup->sizeHint();
    popupSize.setWidth(qMax(popupSize.width(), width()));

    QScreen *popupScreen = QGuiApplication::screenAt(mapToGlobal(rect().center()));
    if (!popupScreen)
        popupScreen = screen();
    if (!popupScreen)
        popupScreen = QGuiApplication::primaryScreen();
    if (!popupScreen) {
        qWarning("Format filter popup could not find an available screen");
        return;
    }

    const QRect availableGeometry = popupScreen->availableGeometry();
    if (popupSize.width() > availableGeometry.width()
        || popupSize.height() > availableGeometry.height()) {
        qWarning("Format filter popup does not fit within the available screen geometry");
        return;
    }

    const QPoint comboTopLeft = mapToGlobal(QPoint(0, 0));
    const QPoint belowCombo = mapToGlobal(QPoint(0, height()));
    const int maximumX = availableGeometry.right() - popupSize.width() + 1;
    const int maximumY = availableGeometry.bottom() - popupSize.height() + 1;
    const int preferredY = availableGeometry.bottom() - belowCombo.y() + 1 >= popupSize.height()
        ? belowCombo.y()
        : comboTopLeft.y() - popupSize.height();
    const int popupX = qBound(availableGeometry.left(), belowCombo.x(), maximumX);
    const int popupY = qBound(availableGeometry.top(), preferredY, maximumY);

    mPopup->resize(popupSize);
    mPopup->move(popupX, popupY);
    mPopup->show();
}

void FormatFilterComboBox::hidePopup() {
    if (mPopup)
        mPopup->hide();
    StyledComboBox::hidePopup();
}

QColor FormatFilterComboBox::iconColor() const {
    if (property("active").toBool())
        return applyEnabledState(QColor(Qt::white));
    return StyledComboBox::iconColor();
}

int FormatFilterComboBox::widestDisplayTextWidth() const {
    int widestWidth = fontMetrics().horizontalAdvance(tr("Custom"));
    widestWidth = qMax(widestWidth, fontMetrics().horizontalAdvance(tr("All formats")));
    for (const QCheckBox *checkBox : mFormatCheckBoxes)
        widestWidth = qMax(widestWidth, fontMetrics().horizontalAdvance(checkBox->text()));
    return widestWidth;
}

int FormatFilterComboBox::requiredControlWidth() const {
    const int frameWidth = style()->pixelMetric(
        QStyle::PM_ComboBoxFrameWidth, nullptr, this);
    return kTextLeftPadding + widestDisplayTextWidth() + kTextIconSpacingPx
        + iconAreaWidth() + frameWidth * 2;
}
