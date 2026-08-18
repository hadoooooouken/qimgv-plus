#include "foldercoverresolver.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QPointer>
#include <QRunnable>
#include <QThread>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>

namespace {

namespace fs = std::filesystem;

// Folder cover discovery enumerates a directory and stats its entries,
// which is I/O-bound rather than CPU-bound. Unlike Thumbnailer's decode
// pool (which halves the ideal thread count because decoding saturates
// CPU cores), this pool can use the full ideal thread count so that
// resolving covers for many sibling folders does not fall behind file
// thumbnail generation. It is still clamped so pathological hardware
// reports do not open an unbounded number of directory handles at once.
constexpr int kMinFolderCoverWorkers = 2;
constexpr int kMaxFolderCoverWorkers = 16;

int folderCoverWorkerCount()
{
    const int idealThreadCount = QThread::idealThreadCount();
    const int candidate =
        idealThreadCount > 0 ? idealThreadCount : kMinFolderCoverWorkers;
    return std::clamp(candidate, kMinFolderCoverWorkers, kMaxFolderCoverWorkers);
}

constexpr auto kAdditionalImageExtensions = std::to_array<const char *>({
    "jfif",
    "tga",
    "webp",
    "ttf",
    "otf",
    "ttc"
});

QString normalizedPath(const QString &path)
{
    if (path.isEmpty())
        return {};
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString errorText(const std::error_code &error)
{
    return QString::fromStdString(error.message());
}

bool isSupportedSortingMode(SortingMode sortingMode)
{
    switch (sortingMode) {
    case SORT_NAME:
    case SORT_NAME_DESC:
    case SORT_SIZE:
    case SORT_SIZE_DESC:
    case SORT_TIME:
    case SORT_TIME_DESC:
        return true;
    }
    return false;
}

FolderCoverResult readError(FolderCoverRequest request,
                            const QString &diagnostic)
{
    return {
        std::move(request),
        FolderCoverStatus::ReadError,
        {},
        diagnostic,
        false
    };
}

FolderCoverResult findFolderCover(
    FolderCoverRequest request,
    const std::shared_ptr<const QSet<QString>> &supportedExtensions)
{
    if (!supportedExtensions) {
        return readError(
            std::move(request),
            QStringLiteral("Folder cover format filters are unavailable."));
    }

    const fs::path folderPath(request.folderPath.toStdWString());
    const QString nativeFolderPath =
        QDir::toNativeSeparators(request.folderPath);
    std::error_code error;
    const fs::file_status folderStatus = fs::status(folderPath, error);
    if (error) {
        return readError(
            std::move(request),
            QStringLiteral("Could not inspect folder \"%1\": %2")
                .arg(nativeFolderPath, errorText(error)));
    }
    if (!fs::exists(folderStatus)) {
        return readError(
            std::move(request),
            QStringLiteral("Folder does not exist: \"%1\"")
                .arg(nativeFolderPath));
    }
    if (!fs::is_directory(folderStatus)) {
        return readError(
            std::move(request),
            QStringLiteral("Folder cover path is not a directory: \"%1\"")
                .arg(nativeFolderPath));
    }

    fs::directory_iterator iterator(folderPath, error);
    if (error) {
        return readError(
            std::move(request),
            QStringLiteral("Could not enumerate folder \"%1\": %2")
                .arg(nativeFolderPath, errorText(error)));
    }

    QString bestPath;
    QString bestFileName;
    std::uintmax_t bestSize = 0;
    fs::file_time_type bestTime;
    bool hasBestSize = false;
    bool hasBestTime = false;
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    const fs::directory_iterator end;
    while (iterator != end) {
        const fs::directory_entry entry = *iterator;
        const bool isRegularFile = entry.is_regular_file(error);
        if (error) {
            return readError(
                std::move(request),
                QStringLiteral("Could not inspect an entry in folder \"%1\": %2")
                    .arg(nativeFolderPath, errorText(error)));
        }

        if (isRegularFile) {
            QString extension =
                QString::fromStdWString(entry.path().extension().wstring());
            if (extension.startsWith(QLatin1Char('.')))
                extension.remove(0, 1);
            extension = extension.toLower();

            if (supportedExtensions->contains(extension)) {
                const QString fileName =
                    QString::fromStdWString(entry.path().filename().wstring());
                const QString absolutePath = normalizedPath(
                    QString::fromStdWString(entry.path().wstring()));
                if (!QFileInfo(absolutePath).isHidden()) {
                    switch (request.sortingMode) {
                    case SORT_NAME:
                    case SORT_NAME_DESC: {
                        const int comparison =
                            bestPath.isEmpty()
                                ? 0
                                : collator.compare(fileName, bestFileName);
                        const bool better =
                            bestPath.isEmpty() ||
                            (request.sortingMode == SORT_NAME
                                 ? comparison < 0
                                 : comparison > 0);
                        if (better) {
                            bestPath = absolutePath;
                            bestFileName = fileName;
                        }
                        break;
                    }
                    case SORT_SIZE:
                    case SORT_SIZE_DESC: {
                        const std::uintmax_t size = entry.file_size(error);
                        if (error) {
                            return readError(
                                std::move(request),
                                QStringLiteral(
                                    "Could not read the size of \"%1\": %2")
                                    .arg(QDir::toNativeSeparators(absolutePath),
                                         errorText(error)));
                        }
                        const bool better =
                            !hasBestSize ||
                            (request.sortingMode == SORT_SIZE
                                 ? size < bestSize
                                 : size > bestSize);
                        if (better) {
                            hasBestSize = true;
                            bestSize = size;
                            bestPath = absolutePath;
                        }
                        break;
                    }
                    case SORT_TIME:
                    case SORT_TIME_DESC: {
                        const fs::file_time_type modified =
                            entry.last_write_time(error);
                        if (error) {
                            return readError(
                                std::move(request),
                                QStringLiteral(
                                    "Could not read the modification time of "
                                    "\"%1\": %2")
                                    .arg(
                                        QDir::toNativeSeparators(absolutePath),
                                        errorText(error)));
                        }
                        const bool better =
                            !hasBestTime ||
                            (request.sortingMode == SORT_TIME
                                 ? modified < bestTime
                                 : modified > bestTime);
                        if (better) {
                            hasBestTime = true;
                            bestTime = modified;
                            bestPath = absolutePath;
                        }
                        break;
                    }
                    }
                }
            }
        }

        iterator.increment(error);
        if (error) {
            return readError(
                std::move(request),
                QStringLiteral("Could not continue enumerating folder \"%1\": %2")
                    .arg(nativeFolderPath, errorText(error)));
        }
    }

    return {
        std::move(request),
        bestPath.isEmpty() ? FolderCoverStatus::NoCover
                           : FolderCoverStatus::CoverFound,
        std::move(bestPath),
        {},
        false
    };
}

class FolderCoverTask final : public QRunnable {
public:
    FolderCoverTask(
        FolderCoverRequest request,
        std::shared_ptr<const QSet<QString>> supportedExtensions,
        std::function<void(FolderCoverResult)> resultHandler)
        : request(std::move(request)),
          supportedExtensions(std::move(supportedExtensions)),
          resultHandler(std::move(resultHandler))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        resultHandler(findFolderCover(request, supportedExtensions));
    }

private:
    FolderCoverRequest request;
    std::shared_ptr<const QSet<QString>> supportedExtensions;
    std::function<void(FolderCoverResult)> resultHandler;
};

} // namespace

FolderCoverResolver::FolderCoverResolver(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<FolderCoverResult>();
    threadPool.setMaxThreadCount(folderCoverWorkerCount());

    auto extensions = std::make_shared<QSet<QString>>();
    for (const QByteArray &format : QImageReader::supportedImageFormats())
        extensions->insert(QString::fromLatin1(format).toLower());
    for (const char *extension : kAdditionalImageExtensions)
        extensions->insert(QString::fromLatin1(extension));
    supportedExtensions = std::move(extensions);
}

FolderCoverResolver::~FolderCoverResolver()
{
    threadPool.clear();
    threadPool.waitForDone();
}

void FolderCoverResolver::resolve(FolderCoverRequest request)
{
    request.folderPath = normalizedPath(request.folderPath);
    if (request.folderPath.isEmpty()) {
        emitQueued(readError(
            std::move(request),
            QStringLiteral("Folder cover request has an empty path.")));
        return;
    }
    if (request.thumbnailSize <= 0) {
        emitQueued(readError(
            std::move(request),
            QStringLiteral("Folder cover request has an invalid thumbnail size.")));
        return;
    }
    if (!isSupportedSortingMode(request.sortingMode)) {
        emitQueued(readError(
            std::move(request),
            QStringLiteral("Folder cover request has an invalid sorting mode.")));
        return;
    }

    const CacheKey key = cacheKey(request.folderPath, request.sortingMode);
    auto cachedCover = coverCache.find(key);
    if (cachedCover != coverCache.end()) {
        if (QFileInfo(*cachedCover).isFile()) {
            FolderCoverResult result{
                std::move(request),
                FolderCoverStatus::CoverFound,
                *cachedCover,
                {},
                true
            };
            emitQueued(std::move(result));
            return;
        }
        coverCache.erase(cachedCover);
    }

    const quint64 folderRevision = folderRevisions.value(key.first);
    auto pending = pendingRequests.find(key);
    if (pending != pendingRequests.end() &&
        pending->folderRevision == folderRevision) {
        for (const FolderCoverRequest &existing :
             std::as_const(pending->requests)) {
            if (existing.thumbnailSize == request.thumbnailSize &&
                existing.generation == request.generation)
                return;
        }
        pending->requests.append(std::move(request));
        return;
    }

    pendingRequests.insert(
        key,
        PendingResolve{folderRevision, QList<FolderCoverRequest>{request}});
    const QPointer<FolderCoverResolver> resolver(this);
    auto resultHandler =
        [resolver, folderRevision](FolderCoverResult result) mutable {
            if (!resolver)
                return;
            QMetaObject::invokeMethod(
                resolver.get(),
                [resolver, result = std::move(result), folderRevision]() mutable {
                    if (resolver)
                        resolver->onSearchFinished(
                            std::move(result), folderRevision);
                },
                Qt::QueuedConnection);
        };
    auto task = std::make_unique<FolderCoverTask>(
        request, supportedExtensions, std::move(resultHandler));
    threadPool.start(task.release());
}

void FolderCoverResolver::invalidate(const QString &folderPath)
{
    const QString key = normalizedPath(folderPath).toCaseFolded();
    ++folderRevisions[key];
    for (auto it = coverCache.begin(); it != coverCache.end();) {
        if (it.key().first == key)
            it = coverCache.erase(it);
        else
            ++it;
    }
}

void FolderCoverResolver::onSearchFinished(FolderCoverResult result,
                                           quint64 folderRevision)
{
    const CacheKey key =
        cacheKey(result.request.folderPath, result.request.sortingMode);
    auto pending = pendingRequests.find(key);
    if (pending == pendingRequests.end() ||
        pending->folderRevision != folderRevision) {
        return;
    }

    if (folderRevisions.value(key.first) != folderRevision) {
        pendingRequests.erase(pending);
        return;
    }

    QList<FolderCoverRequest> requests = std::move(pending->requests);
    pendingRequests.erase(pending);

    if (result.status == FolderCoverStatus::CoverFound)
        coverCache.insert(key, result.coverPath);

    for (const FolderCoverRequest &request : requests) {
        FolderCoverResult requestResult = result;
        requestResult.request = request;
        emit resultReady(std::move(requestResult));
    }
}

FolderCoverResolver::CacheKey
FolderCoverResolver::cacheKey(const QString &folderPath,
                              SortingMode sortingMode)
{
    return {
        normalizedPath(folderPath).toCaseFolded(),
        static_cast<int>(sortingMode)
    };
}

void FolderCoverResolver::emitQueued(FolderCoverResult result)
{
    QMetaObject::invokeMethod(
        this,
        [this, result = std::move(result)]() mutable {
            emit resultReady(std::move(result));
        },
        Qt::QueuedConnection);
}
