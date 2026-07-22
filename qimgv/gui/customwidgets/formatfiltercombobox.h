#pragma once

#include <QPersistentModelIndex>
#include <QStandardItemModel>
#include <QString>
#include "gui/customwidgets/styledcombobox.h"

// A StyledComboBox variant with a checkable, multi-select popup that stays
// open while checkboxes are toggled. Row 0 is always "All formats"; the
// remaining rows are format groups (see utils/formatgroups.h). Selecting
// "All formats" clears every other row and vice versa; the selection can
// never end up empty (falls back to "All formats").
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
    bool eventFilter(QObject *watched, QEvent *event) override;
    QColor iconColor() const override;
    void showPopup() override;

private:
    QStandardItemModel *model;
    QString mDisplayText;
    QPersistentModelIndex mPressedIndex;

    // Left padding for the manually-drawn label. Not derived from any
    // style metric — StyledComboBox draws no left-side decoration, so
    // this is a pure visual-design choice for this widget specifically.
    static constexpr int kTextLeftPadding = 9;
    static constexpr int kTextIconSpacingPx = 4;

    void toggleItem(int row);
    bool anyFormatChecked() const;
    void updateDisplayLabel();
    int widestDisplayTextWidth() const;
    int requiredControlWidth() const;

signals:
    void formatSelectionChanged(QStringList extensions);
};
