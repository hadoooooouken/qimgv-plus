#pragma once

#include <QString>
#include <filesystem>
#include <optional>

struct ThumbnailSourceStamp {
    QString normalizedPath;
    qint64 modifiedTimeTicks = 0;
    qint64 size = 0;

    [[nodiscard]] static std::optional<ThumbnailSourceStamp>
    fromMetadata(const QString &path, std::uintmax_t fileSize,
                 std::filesystem::file_time_type modifiedTime);
    [[nodiscard]] static std::optional<ThumbnailSourceStamp>
    fromPath(const QString &path);
};
