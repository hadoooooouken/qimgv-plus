#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct FormatGroup {
    QString label;          // display label, e.g. "HEIF/HEIC"
    QStringList extensions; // lowercase extensions without the dot, e.g. {"heif", "heic"}
};

struct FormatCategory {
    QString label;
    QVector<FormatGroup> groups;
};

// Fixed, ordered format categories shown in the format filter popup.
const QVector<FormatCategory> &allFormatCategories();
