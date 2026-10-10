#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Name filters of the "Save File as..." dialog: one filter per writable image
// format the application offers (JPEG, PNG, WebP, JPEG-XL, AVIF, QOI, BMP,
// TIFF, in this order), and the filter matching the suggested file's suffix.
// Shared by both user interfaces.
struct SaveFileFilters {
    QStringList filters;
    // The filter of the suggested file: the first filter that contains its
    // lower-case suffix; the JPEG filter when none does.
    QString selected;

    // Position of selected in filters, 0 when it is not offered.
    [[nodiscard]] int selectedIndex() const;
};

// writerFormats: QImageWriter::supportedImageFormats().
[[nodiscard]] SaveFileFilters saveFileFiltersFor(const QList<QByteArray> &writerFormats,
                                                 const QString &suggestedPath);
