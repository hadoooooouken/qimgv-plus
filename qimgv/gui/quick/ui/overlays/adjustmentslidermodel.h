#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

// How a slider value is shown next to the slider.
enum class SliderValueFormat {
    // "42%"
    Percent,
    // "42" and a degree sign
    Degrees,
    // value / 100 with two decimals and a sign: "+0.50", "-1.25"
    SignedHundredths,
    // value / 100 with two decimals: "0.50"
    Hundredths,
    // "42"
    Integer,
};

// One slider row: integer range and the value a double click restores.
struct SliderSpec {
    QString label;
    int minimum = 0;
    int maximum = 0;
    int defaultValue = 0;
    SliderValueFormat format = SliderValueFormat::Integer;
};

// Text of value in format.
[[nodiscard]] QString sliderValueText(int value, SliderValueFormat format);

// Rows of labelled sliders of an adjustment overlay (DraggableSliderOverlay):
// each row has an integer range, a default value and a formatted value text.
// Roles: label, minimum, maximum, defaultValue, value, valueText. Subclasses
// define the rows and react to edits in valuesEdited().
//
// GUI thread only.
class AdjustmentSliderModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")

public:
    enum Role {
        LabelRole = Qt::UserRole + 1,
        MinimumRole,
        MaximumRole,
        DefaultValueRole,
        ValueRole,
        ValueTextRole,
    };
    Q_ENUM(Role)

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int value(int row) const;
    // Sets the value of row (clamped to its range); an edit by the person.
    Q_INVOKABLE void setValue(int row, int value);
    // Restores the default value of row (double click on the slider).
    Q_INVOKABLE void resetValue(int row);
    // Restores every default value as one edit.
    Q_INVOKABLE void resetAll();

protected:
    AdjustmentSliderModel(QList<SliderSpec> specs, QObject *parent);

    // Sets the values row by row (clamped) without an edit notification.
    void loadValues(const QList<int> &values);
    // Called after the person changed one or more values.
    virtual void valuesEdited() = 0;

private:
    [[nodiscard]] bool isValidRow(int row) const;
    bool storeValue(int row, int value);

    QList<SliderSpec> mSpecs;
    QList<int> mValues;
};
