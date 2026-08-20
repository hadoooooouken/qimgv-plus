#pragma once

#include <memory>
#include <QObject>
#include "cache/cache.h"
#include "directorymanager/directorymanager.h"
#include "scaler/scaler.h"
#include "loader/loader.h"
#include "utils/fileoperations.h"

class DirectoryModel : public QObject {
    Q_OBJECT
public:
    explicit DirectoryModel(QObject *parent = nullptr);
    ~DirectoryModel();

    void clearScaler();
    void requestScaled(const ScalerRequest &req);

    void load(QString filePath, bool asyncHint);
    void preload(QString filePath);

    int fileCount() const;
    int dirCount() const;
    int indexOfFile(QString filePath) const;
    int indexOfDir(QString filePath) const;
    QString fileNameAt(int index) const;
    bool containsFile(QString filePath) const;
    bool isEmpty() const;
    QString nextOf(QString filePath) const;
    QString prevOf(QString filePath) const;
    QString firstFile() const;
    QString lastFile() const;
    QDateTime lastModified(QString filePath) const;

    bool forceInsert(QString filePath);
    bool insertDir(const QString &dirPath);
    void copyFileTo(const QString &srcFile, const QString &destDirPath, bool force, FileOpResult &result);
    void moveFileTo(const QString &srcFile, const QString &destDirPath, bool force, FileOpResult &result);
    void renameEntry(const QString &oldFilePath, const QString &newName, bool force, FileOpResult &result);
    void removeFile(const QString &filePath, bool trash, FileOpResult &result);
    void removeDir(const QString &dirPath, bool trash, bool recursive, FileOpResult &result);

    bool setDirectory(QString);
    bool setFileList(const QStringList &filePaths);

    void unload(int index);

    bool loaderBusy() const;

    std::shared_ptr<Image> getImageAt(int index);
    std::shared_ptr<Image> getImage(QString filePath);

    void updateImage(QString filePath, std::shared_ptr<Image> img);

    void setSortingMode(SortingMode mode);
    SortingMode sortingMode() const;
    void setFormatFilter(QStringList extensions);

    QString directoryPath() const;
    void unload(QString filePath);
    bool isLoaded(int index) const;
    bool isLoaded(QString filePath) const;
    bool reload(QString filePath);
    QString filePathAt(int index) const;
    void unloadExcept(QString filePath, bool keepNearby);
    const FSEntry &fileEntryAt(int index) const;

    int totalCount() const;
    QString dirNameAt(int index) const;
    QString dirPathAt(int index) const;

    bool autoRefresh();

    [[nodiscard]] ImageSaveResult saveFile(const QString &filePath);
    [[nodiscard]] ImageSaveResult saveFile(const QString &filePath,
                                           const QString &destPath);

    bool containsDir(QString dirPath) const;
    FileListSource source();
signals:
    void fileRemoved(QString filePath, int index);
    void fileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void fileAdded(QString filePath);
    void fileModified(QString filePath);
    void dirRemoved(QString dirPath, int index);
    void dirRenamed(QString dirPath, int indexFrom, QString toPath, int indexTo);
    void dirAdded(QString dirPath);
    void loaded(QString filePath);
    void loadFailed(const QString &path);
    void sortingChanged(SortingMode);
    void indexChanged(int oldIndex, int index);
    void imageReady(std::shared_ptr<Image> img, const QString&);
    void imageUpdated(QString filePath);
    void scalingFinished(QImage result, ScalerRequest req);

private:
    DirectoryManager dirManager;
    Loader loader;
    Cache cache;
    std::unique_ptr<Scaler> scaler;
    FileListSource fileListSource;

    // Loads filePath from disk unconditionally, bypassing the cache-hit
    // check in load(). Callers must ensure any stale cache entry for
    // filePath is already removed. Returns true if the image was loaded
    // (synchronous path) or the async task was scheduled successfully.
    bool forceLoad(QString filePath, bool asyncHint);
    static void synchronizePageOverride(const std::shared_ptr<Image> &img);

private slots:
    void onImageReady(std::shared_ptr<Image> img, const QString &path);
    void onSortingChanged();
    void onFileAdded(QString filePath);
    void onFileRemoved(QString filePath, int index);
    void onFileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void onFileModified(QString filePath);
};
