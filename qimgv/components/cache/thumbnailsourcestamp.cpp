#include "thumbnailsourcestamp.h"

#include "utils/stuff.h"

#include <QDir>
#include <QFileInfo>
#include <utility>

std::optional<ThumbnailSourceStamp> ThumbnailSourceStamp::fromMetadata(
    const QString &path, std::uintmax_t fileSize,
    std::filesystem::file_time_type modifiedTime)
{
    const auto modifiedTimeTicks = modifiedTime.time_since_epoch().count();
    if (path.isEmpty() || !std::in_range<qint64>(fileSize) ||
        !std::in_range<qint64>(modifiedTimeTicks)) {
        return std::nullopt;
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    const QString normalizedPath =
        QDir::cleanPath(QDir::fromNativeSeparators(absolutePath))
            .toCaseFolded();
    if (normalizedPath.isEmpty())
        return std::nullopt;

    return ThumbnailSourceStamp{
        normalizedPath,
        static_cast<qint64>(modifiedTimeTicks),
        static_cast<qint64>(fileSize)};
}

std::optional<ThumbnailSourceStamp>
ThumbnailSourceStamp::fromPath(const QString &path)
{
    std::error_code error;
    const std::filesystem::directory_entry entry(toStdString(path), error);
    if (error)
        return std::nullopt;
    if (!entry.is_regular_file(error) || error)
        return std::nullopt;

    const std::uintmax_t fileSize = entry.file_size(error);
    if (error)
        return std::nullopt;

    const std::filesystem::file_time_type modifiedTime =
        entry.last_write_time(error);
    if (error)
        return std::nullopt;

    return fromMetadata(path, fileSize, modifiedTime);
}
