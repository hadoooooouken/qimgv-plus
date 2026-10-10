#include "formatfiltermodel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace {
// The widget combo box's translation context.
constexpr char kTranslationContext[] = "FormatFilterComboBox";

QString allFormatsText() {
    return QCoreApplication::translate(kTranslationContext, "All formats");
}

QString customText() {
    return QCoreApplication::translate(kTranslationContext, "Custom");
}
} // namespace

FormatFilterModel::FormatFilterModel(QObject *parent)
    : FormatFilterModel(allFormatCategories(), parent) {}

FormatFilterModel::FormatFilterModel(QList<FormatCategory> categories, QObject *parent)
    : QAbstractListModel(parent), mCategories(std::move(categories)) {
    for (int category = 0; category < mCategories.size(); ++category) {
        for (const FormatGroup &group : std::as_const(mCategories.at(category).groups))
            mFormats.append({.label = group.label, .extensions = group.extensions, .category = category});
    }
    mChecked.fill(false, mFormats.size());
}

int FormatFilterModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mCategories.size());
}

QVariant FormatFilterModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= mCategories.size())
        return {};
    const int category = index.row();
    switch (role) {
    case LabelRole:
        return mCategories.at(category).label;
    case CheckStateRole:
        return static_cast<int>(categoryState(category));
    case FormatsRole: {
        QVariantList formats;
        for (int format = 0; format < mFormats.size(); ++format) {
            if (mFormats.at(format).category != category)
                continue;
            formats.append(QVariantMap{
                {QStringLiteral("label"), mFormats.at(format).label},
                {QStringLiteral("checked"), mChecked.at(format)},
                {QStringLiteral("index"), format},
            });
        }
        return formats;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> FormatFilterModel::roleNames() const {
    return {
        {LabelRole, "label"},
        {CheckStateRole, "checkState"},
        {FormatsRole, "formats"},
    };
}

bool FormatFilterModel::allFormats() const {
    return !anyFormatChecked();
}

// FormatFilterComboBox::updateDisplayLabel().
QString FormatFilterModel::displayText() const {
    const auto checkedCount = std::ranges::count(mChecked, true);
    if (checkedCount == 0)
        return allFormatsText();
    if (checkedCount > 1)
        return customText();
    const auto checked = std::ranges::find(mChecked, true);
    return mFormats.at(static_cast<int>(std::distance(mChecked.begin(), checked))).label;
}

QString FormatFilterModel::allFormatsLabel() {
    return allFormatsText();
}

QStringList FormatFilterModel::displayTexts() const {
    QStringList texts{allFormatsText(), customText()};
    for (const Format &format : mFormats)
        texts.append(format.label);
    return texts;
}

QStringList FormatFilterModel::checkedExtensions() const {
    QStringList extensions;
    for (int format = 0; format < mFormats.size(); ++format) {
        if (mChecked.at(format))
            extensions.append(mFormats.at(format).extensions);
    }
    return extensions;
}

// FormatFilterComboBox::setCheckedExtensions().
void FormatFilterModel::setCheckedExtensions(const QStringList &extensions) {
    QSet<QString> wanted;
    for (const QString &extension : extensions)
        wanted.insert(extension.toLower());
    for (int format = 0; format < mFormats.size(); ++format) {
        const QStringList &formatExtensions = mFormats.at(format).extensions;
        mChecked[format] = std::ranges::any_of(
            formatExtensions, [&wanted](const QString &extension) { return wanted.contains(extension); });
    }
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {CheckStateRole, FormatsRole});
    emit selectionChanged();
}

void FormatFilterModel::selectAllFormats() {
    resetToAllFormats();
    publish();
}

// FormatFilterComboBox::handleFormatClicked().
void FormatFilterModel::setFormatChecked(int formatIndex, bool checked) {
    if (formatIndex < 0 || formatIndex >= mFormats.size()) {
        qWarning() << "FormatFilterModel has no format" << formatIndex;
        return;
    }
    if (allFormats())
        mChecked.fill(false);
    mChecked[formatIndex] = checked;
    publish();
}

// FormatFilterComboBox::handleCategoryClicked().
void FormatFilterModel::setCategoryChecked(int categoryIndex, bool checked) {
    if (categoryIndex < 0 || categoryIndex >= mCategories.size()) {
        qWarning() << "FormatFilterModel has no category" << categoryIndex;
        return;
    }
    for (int format = 0; format < mFormats.size(); ++format) {
        if (mFormats.at(format).category == categoryIndex)
            mChecked[format] = checked;
    }
    publish();
}

bool FormatFilterModel::anyFormatChecked() const {
    return std::ranges::contains(mChecked, true);
}

Qt::CheckState FormatFilterModel::categoryState(int categoryIndex) const {
    int total = 0;
    int checked = 0;
    for (int format = 0; format < mFormats.size(); ++format) {
        if (mFormats.at(format).category != categoryIndex)
            continue;
        ++total;
        if (mChecked.at(format))
            ++checked;
    }
    if (checked == 0)
        return Qt::Unchecked;
    return checked == total ? Qt::Checked : Qt::PartiallyChecked;
}

void FormatFilterModel::resetToAllFormats() {
    mChecked.fill(false);
}

// An empty set falls back to all formats.
void FormatFilterModel::publish() {
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {CheckStateRole, FormatsRole});
    emit selectionChanged();
    emit filterSelected(checkedExtensions());
}
