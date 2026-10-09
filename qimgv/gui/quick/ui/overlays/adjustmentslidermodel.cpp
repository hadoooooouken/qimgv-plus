#include "adjustmentslidermodel.h"

#include <QDebug>

#include <utility>

namespace {
using namespace Qt::StringLiterals;

constexpr QChar kDegreeSign = u'°';
// Slider units per displayed unit of the hundredths formats.
constexpr double kHundredths = 100.0;
constexpr int kHundredthsDecimals = 2;
} // namespace

QString sliderValueText(int value, SliderValueFormat format) {
    switch (format) {
    case SliderValueFormat::Percent:
        return QString::number(value) + u'%';
    case SliderValueFormat::Degrees:
        return QString::number(value) + kDegreeSign;
    case SliderValueFormat::SignedHundredths:
        return (value >= 0 ? u"+"_s : QString()) +
               QString::number(value / kHundredths, 'f', kHundredthsDecimals);
    case SliderValueFormat::Hundredths:
        return QString::number(value / kHundredths, 'f', kHundredthsDecimals);
    case SliderValueFormat::Integer:
        break;
    }
    return QString::number(value);
}

//------------------------------------------------------------------------------
AdjustmentSliderModel::AdjustmentSliderModel(QList<SliderSpec> specs, QObject *parent)
    : QAbstractListModel(parent), mSpecs(std::move(specs)) {
    mValues.reserve(mSpecs.size());
    for (const SliderSpec &spec : std::as_const(mSpecs))
        mValues << spec.defaultValue;
}

int AdjustmentSliderModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(mSpecs.size());
}

QVariant AdjustmentSliderModel::data(const QModelIndex &index, int role) const {
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const SliderSpec &spec = mSpecs.at(index.row());
    const int value = mValues.at(index.row());
    switch (role) {
    case LabelRole:
        return spec.label;
    case MinimumRole:
        return spec.minimum;
    case MaximumRole:
        return spec.maximum;
    case DefaultValueRole:
        return spec.defaultValue;
    case ValueRole:
        return value;
    case ValueTextRole:
        return sliderValueText(value, spec.format);
    default:
        return {};
    }
}

QHash<int, QByteArray> AdjustmentSliderModel::roleNames() const {
    return {{LabelRole, "label"},
            {MinimumRole, "minimum"},
            {MaximumRole, "maximum"},
            {DefaultValueRole, "defaultValue"},
            {ValueRole, "value"},
            {ValueTextRole, "valueText"}};
}

int AdjustmentSliderModel::value(int row) const {
    if (!isValidRow(row)) {
        qWarning() << "AdjustmentSliderModel::value: no row" << row;
        return 0;
    }
    return mValues.at(row);
}

void AdjustmentSliderModel::setValue(int row, int value) {
    if (!isValidRow(row)) {
        qWarning() << "AdjustmentSliderModel::setValue: no row" << row;
        return;
    }
    if (storeValue(row, value))
        valuesEdited();
}

void AdjustmentSliderModel::resetValue(int row) {
    if (!isValidRow(row)) {
        qWarning() << "AdjustmentSliderModel::resetValue: no row" << row;
        return;
    }
    setValue(row, mSpecs.at(row).defaultValue);
}

void AdjustmentSliderModel::resetAll() {
    for (int row = 0; row < mSpecs.size(); ++row)
        storeValue(row, mSpecs.at(row).defaultValue);
    valuesEdited();
}

void AdjustmentSliderModel::loadValues(const QList<int> &values) {
    if (values.size() != mSpecs.size()) {
        qWarning() << "AdjustmentSliderModel::loadValues:" << values.size()
                   << "values for" << mSpecs.size() << "rows";
        return;
    }
    for (int row = 0; row < values.size(); ++row)
        storeValue(row, values.at(row));
}

bool AdjustmentSliderModel::isValidRow(int row) const {
    return row >= 0 && row < mSpecs.size();
}

bool AdjustmentSliderModel::storeValue(int row, int value) {
    const SliderSpec &spec = mSpecs.at(row);
    const int clamped = qBound(spec.minimum, value, spec.maximum);
    if (mValues.at(row) == clamped)
        return false;
    mValues[row] = clamped;
    const QModelIndex changed = index(row);
    emit dataChanged(changed, changed, {ValueRole, ValueTextRole});
    return true;
}
