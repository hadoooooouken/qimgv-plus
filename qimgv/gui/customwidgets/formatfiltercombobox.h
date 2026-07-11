#pragma once

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

protected:
    void paintEvent(QPaintEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    QColor iconColor() const override;

private:
    QStandardItemModel *model;
    QString mDisplayText;

    void toggleItem(int row);
    bool anyFormatChecked() const;
    void updateDisplayLabel();

signals:
    void formatSelectionChanged(QStringList extensions);
};
