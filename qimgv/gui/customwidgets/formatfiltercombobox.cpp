#include "formatfiltercombobox.h"

#include <QAbstractItemView>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QStyle>

#include "utils/formatgroups.h"

FormatFilterComboBox::FormatFilterComboBox(QWidget *parent)
    : StyledComboBox(parent),
      model(new QStandardItemModel(this))
{
    auto *allItem = new QStandardItem(tr("All formats"));
    allItem->setCheckable(true);
    allItem->setCheckState(Qt::Checked);
    allItem->setData(QStringList(), Qt::UserRole);
    model->appendRow(allItem);

    for (const FormatGroup &group : allFormatGroups()) {
        auto *item = new QStandardItem(group.label);
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        item->setData(group.extensions, Qt::UserRole);
        model->appendRow(item);
    }

    setModel(model);
    setCurrentIndex(-1); // no single "current" item; label is drawn manually

    view()->viewport()->installEventFilter(this);

    setProperty("active", false);
    updateDisplayLabel();
}

QStringList FormatFilterComboBox::checkedExtensions() const {
    if (model->item(0)->checkState() == Qt::Checked)
        return {};

    QStringList result;
    for (int i = 1; i < model->rowCount(); i++) {
        if (model->item(i)->checkState() == Qt::Checked)
            result << model->item(i)->data(Qt::UserRole).toStringList();
    }
    return result;
}

void FormatFilterComboBox::setCheckedExtensions(QStringList extensions) {
    blockSignals(true);

    QSet<QString> wanted;
    for (const QString &ext : extensions)
        wanted.insert(ext.toLower());

    if (wanted.isEmpty()) {
        model->item(0)->setCheckState(Qt::Checked);
        for (int i = 1; i < model->rowCount(); i++)
            model->item(i)->setCheckState(Qt::Unchecked);
    } else {
        model->item(0)->setCheckState(Qt::Unchecked);
        bool matchedAny = false;
        for (int i = 1; i < model->rowCount(); i++) {
            QStringList groupExtensions = model->item(i)->data(Qt::UserRole).toStringList();
            bool checked = false;
            for (const QString &ext : groupExtensions) {
                if (wanted.contains(ext)) {
                    checked = true;
                    break;
                }
            }
            model->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            matchedAny = matchedAny || checked;
        }
        if (!matchedAny)
            model->item(0)->setCheckState(Qt::Checked); // nothing matched, fall back
    }

    updateDisplayLabel();
    blockSignals(false);
}

bool FormatFilterComboBox::anyFormatChecked() const {
    for (int i = 1; i < model->rowCount(); i++) {
        if (model->item(i)->checkState() == Qt::Checked)
            return true;
    }
    return false;
}

void FormatFilterComboBox::toggleItem(int row) {
    QStandardItem *item = model->item(row);
    if (!item)
        return;

    if (row == 0) {
        if (item->checkState() != Qt::Checked) {
            item->setCheckState(Qt::Checked);
            for (int i = 1; i < model->rowCount(); i++)
                model->item(i)->setCheckState(Qt::Unchecked);
        }
        // clicking "All formats" while it's already the only checked row is a no-op
    } else {
        bool newChecked = (item->checkState() != Qt::Checked);
        item->setCheckState(newChecked ? Qt::Checked : Qt::Unchecked);

        if (newChecked)
            model->item(0)->setCheckState(Qt::Unchecked);

        if (!anyFormatChecked())
            model->item(0)->setCheckState(Qt::Checked); // never allow an empty selection
    }

    updateDisplayLabel();
    emit formatSelectionChanged(checkedExtensions());
}

void FormatFilterComboBox::updateDisplayLabel() {
    int checkedFormats = 0;
    int lastCheckedRow = -1;
    for (int i = 1; i < model->rowCount(); i++) {
        if (model->item(i)->checkState() == Qt::Checked) {
            checkedFormats++;
            lastCheckedRow = i;
        }
    }

    bool active;
    if (checkedFormats == 0) {
        mDisplayText = tr("All formats");
        active = false;
    } else if (checkedFormats == 1) {
        mDisplayText = model->item(lastCheckedRow)->text();
        active = true;
    } else {
        mDisplayText = tr("Custom");
        active = true;
    }

    setProperty("active", active);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void FormatFilterComboBox::paintEvent(QPaintEvent *e) {
    StyledComboBox::paintEvent(e);

    QPainter p(this);
    QRect textRect = rect().adjusted(9, 0, -26, 0);
    p.setPen(palette().color(QPalette::ButtonText));
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
               fontMetrics().elidedText(mDisplayText, Qt::ElideRight, textRect.width()));
}

bool FormatFilterComboBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == view()->viewport() && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        QModelIndex index = view()->indexAt(mouseEvent->pos());
        if (index.isValid()) {
            toggleItem(index.row());
            return true; // keep the popup open
        }
    }
    return StyledComboBox::eventFilter(watched, event);
}
