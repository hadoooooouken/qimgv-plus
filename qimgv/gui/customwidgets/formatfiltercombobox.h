#pragma once

#include <QCheckBox>
#include <QFrame>
#include <QString>
#include <QStringList>
#include <QVector>

#include "gui/customwidgets/styledcombobox.h"

// A StyledComboBox variant with a categorized multi-select popup. An empty
// extension list represents the unfiltered "All formats" state.
class FormatFilterComboBox : public StyledComboBox
{
    Q_OBJECT
public:
    explicit FormatFilterComboBox(QWidget *parent = nullptr);

    // Empty list means "All formats".
    QStringList checkedExtensions() const;
    void setCheckedExtensions(QStringList extensions);
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;
    QColor iconColor() const override;
    void showPopup() override;
    void hidePopup() override;

private:
    QWidget *mPopup;
    QCheckBox *mAllFormatsCheckBox;
    QVector<QCheckBox *> mFormatCheckBoxes;
    QVector<QStringList> mFormatExtensions;
    QVector<QCheckBox *> mCategoryCheckBoxes;
    QVector<QVector<int>> mCategoryFormatIndexes;
    QString mDisplayText;

    static constexpr int kTextLeftPadding    = 9;
    static constexpr int kTextIconSpacingPx  = 4;
    static constexpr int kPopupMarginPx      = 10;
    static constexpr int kPopupSpacingPx     = 8;
    static constexpr int kFormatColumnWidthPx = 128;
    static constexpr int kFormatColumns      = 4;
    static constexpr int kFormatGridIndentPx = 20;
    static constexpr int kPopupTopGapPx      = 10;
    static constexpr int kPopupRightGapPx    = 20;

    bool anyFormatChecked() const;
    void createPopup();
    void resetToAllFormats();
    void updateCategoryCheckStates();
    void updateSelectionDisplay();
    void applySelection();
    void handleAllFormatsClicked();
    void handleFormatClicked(int formatIndex, bool checked);
    void handleCategoryClicked(int categoryIndex, bool checked);
    void updateDisplayLabel();
    int widestDisplayTextWidth() const;
    int requiredControlWidth() const;

signals:
    void formatSelectionChanged(QStringList extensions);
};
