#include "formatregistry.h"

#include <QImageReader>

#include <array>
#include <utility>

namespace {

// Formats handled by qimgv itself, plus aliases whose availability must not
// depend on whether a Qt image plugin advertises the extension.
constexpr auto kApplicationExtensions = std::to_array<const char *>({
    "jfif",
    "ai",
    "djvu",
    "djv",
    "blend",
    "ttf",
    "otf",
    "ttc",
    "tga",
    "webp"
});

} // namespace

QList<QByteArray> FormatRegistry::supportedExtensions()
{
    QList<QByteArray> extensions;
    for (QByteArray extension : QImageReader::supportedImageFormats()) {
        extension = extension.toLower();
        if (!extensions.contains(extension))
            extensions.append(std::move(extension));
    }

    for (const char *extension : kApplicationExtensions) {
        const QByteArray normalized(extension);
        if (!extensions.contains(normalized))
            extensions.append(normalized);
    }

    return extensions;
}

QSet<QString> FormatRegistry::supportedExtensionSet()
{
    const QList<QByteArray> extensions = supportedExtensions();
    QSet<QString> result;
    result.reserve(extensions.size());
    for (const QByteArray &extension : extensions)
        result.insert(QString::fromLatin1(extension));
    return result;
}
