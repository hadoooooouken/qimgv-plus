#pragma once

#include <QCoreApplication>
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

// Returns the ordered format categories with translated labels.
// Called once at popup-creation time; the cost is negligible.
QVector<FormatCategory> allFormatCategories();
