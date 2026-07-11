#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct FormatGroup {
    QString label;          // display label, e.g. "HEIF/HEIC"
    QStringList extensions; // lowercase extensions without the dot, e.g. {"heif", "heic"}
};

// Fixed, ordered list of format groups shown in the format filter dropdown.
const QVector<FormatGroup> &allFormatGroups();
