#pragma once

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QString>

namespace FormatRegistry {

// Returns every extension that qimgv can decode or render as an image,
// normalized to lowercase and without a leading dot.
[[nodiscard]] QList<QByteArray> supportedExtensions();

// Set form used by directory scanners and other extension lookups.
[[nodiscard]] QSet<QString> supportedExtensionSet();

} // namespace FormatRegistry
