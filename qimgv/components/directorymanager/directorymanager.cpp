#include "directorymanager.h"
#include "settings.h"
#include "utils/fileoperations.h"

#include <QImageReader>
#include <QThreadPool>
#include <QRunnable>
#include <QPointer>
#include <QSet>
#include <atomic>
#include <memory>

namespace fs = std::filesystem;

namespace {
bool isCandidateFileSupported(const QString &name, const QString &path) {
    if (name.endsWith(u".zip", Qt::CaseInsensitive) || name.endsWith(u".cbz", Qt::CaseInsensitive)) {
        QImageReader reader(path, name.endsWith(u".zip", Qt::CaseInsensitive) ? "zip" : "cbz");
        return reader.canRead();
    }
    return true;
}

bool matchesNameFilter(const QString &name, const QString &nameFilter) {
    return nameFilter.isEmpty() || name.contains(nameFilter, Qt::CaseInsensitive);
}

struct DirectoryScanRequest {
    QString directoryPath;
    bool recursive = false;
    bool showHiddenFiles = false;
    QRegularExpression formatRegex;
    QString nameFilter;
};
}

class DirectoryScanner : public QRunnable {
public:
    DirectoryScanner(DirectoryScanRequest request,
                     QPointer<DirectoryManager> manager,
                     std::shared_ptr<std::atomic<bool>> cancelled)
        : request(std::move(request)),
          manager(manager),
          cancelled(cancelled) {
        setAutoDelete(true);
    }

    void run() override {
        if (!manager || (cancelled && cancelled->load())) {
            return;
        }

        std::vector<FSEntry> files;
        std::vector<FSEntry> dirs;
        std::error_code ec;
        auto stdPath = toStdString(request.directoryPath);

        if (request.recursive) {
            fs::recursive_directory_iterator it(stdPath, ec);
            fs::recursive_directory_iterator end;
            while (it != end) {
                if (cancelled && cancelled->load()) return;
                if (ec) break;

                const auto& entry = *it;
                QString name = QString::fromStdWString(entry.path().filename().generic_wstring());
                QString path = QString::fromStdWString(entry.path().generic_wstring());

                bool isDir = false;
                try {
                    isDir = entry.is_directory();
                } catch (...) {
                    it.increment(ec);
                    continue;
                }

                if (isDir) {
                    if (!request.showHiddenFiles) {
                        DWORD attributes = GetFileAttributes(entry.path().c_str());
                        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
                            it.disable_recursion_pending();
                        }
                    }
                    it.increment(ec);
                    continue;
                }

                auto match = request.formatRegex.match(name);
                if (match.hasMatch() && matchesNameFilter(name, request.nameFilter) &&
                    isCandidateFileSupported(name, path)) {
                    FSEntry newEntry;
                    try {
                        newEntry.name = name;
                        newEntry.path = path;
                        newEntry.isDirectory = false;
                        newEntry.size = entry.file_size();
                        newEntry.modifyTime = entry.last_write_time();
                    } catch (...) {
                        it.increment(ec);
                        continue;
                    }
                    files.emplace_back(newEntry);
                }
                it.increment(ec);
            }
        } else {
            fs::directory_iterator it(stdPath, ec);
            fs::directory_iterator end;
            while (it != end) {
                if (cancelled && cancelled->load()) return;
                if (ec) break;

                const auto& entry = *it;
                QString name = QString::fromStdWString(entry.path().filename().generic_wstring());
                QString path = QString::fromStdWString(entry.path().generic_wstring());

                bool isDir = false;
                try {
                    isDir = entry.is_directory();
                } catch (...) {
                    it.increment(ec);
                    continue;
                }

                if (isDir) {
                    if (!request.showHiddenFiles) {
                        DWORD attributes = GetFileAttributes(entry.path().c_str());
                        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
                            it.increment(ec);
                            continue;
                        }
                    }
                    FSEntry newEntry;
                    newEntry.name = name;
                    newEntry.path = path;
                    newEntry.isDirectory = true;
                    dirs.emplace_back(newEntry);
                } else {
                    auto match = request.formatRegex.match(name);
                    if (match.hasMatch() && matchesNameFilter(name, request.nameFilter)) {
                        if (!request.showHiddenFiles) {
                            DWORD attributes = GetFileAttributes(entry.path().c_str());
                            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
                                it.increment(ec);
                                continue;
                            }
                        }
                        if (isCandidateFileSupported(name, path)) {
                            FSEntry newEntry;
                            try {
                                newEntry.name = name;
                                newEntry.path = path;
                                newEntry.isDirectory = false;
                                newEntry.size = entry.file_size();
                                newEntry.modifyTime = entry.last_write_time();
                            } catch (...) {
                                it.increment(ec);
                                continue;
                            }
                            files.emplace_back(newEntry);
                        }
                    }
                }
                it.increment(ec);
            }
        }

        if (cancelled && cancelled->load()) return;

        QPointer<DirectoryManager> mgr = manager;
        QMetaObject::invokeMethod(mgr, [mgr, path = request.directoryPath, files = std::move(files), dirs = std::move(dirs), cancelled = this->cancelled]() mutable {
            if (mgr && (!cancelled || !cancelled->load())) {
                mgr->handleScanFinished(path, std::move(files), std::move(dirs));
            }
        });
    }

private:
    DirectoryScanRequest request;
    QPointer<DirectoryManager> manager;
    std::shared_ptr<std::atomic<bool>> cancelled;
};

DirectoryManager::DirectoryManager() :
    watcher(nullptr),
    mSortingMode(SORT_NAME)
{
    regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    collator.setNumericMode(true);

    readSettings();
    setSortingMode(settings->sortingMode());
    connect(settings, &Settings::settingsChanged, this, &DirectoryManager::readSettings);
}

DirectoryManager::~DirectoryManager() {
    if (currentScanCancelled) {
        currentScanCancelled->store(true);
    }
}

template< typename T, typename Pred >
typename std::vector<T>::iterator
insert_sorted(std::vector<T> & vec, T const& item, Pred pred) {
    return vec.insert(std::upper_bound(vec.begin(), vec.end(), item, pred), item);
}

bool DirectoryManager::path_entry_compare(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.path, e2.path) < 0;
};

bool DirectoryManager::path_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.path, e2.path) > 0;
};

bool DirectoryManager::name_entry_compare(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.name, e2.name) < 0;
};

bool DirectoryManager::name_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.name, e2.name) > 0;
};

bool DirectoryManager::date_entry_compare(const FSEntry& e1, const FSEntry& e2) const {
    return e1.modifyTime < e2.modifyTime;
}

bool DirectoryManager::date_entry_compare_reverse(const FSEntry& e1, const FSEntry& e2) const {
    return e1.modifyTime > e2.modifyTime;
}

bool DirectoryManager::size_entry_compare(const FSEntry& e1, const FSEntry& e2) const {
    return e1.size < e2.size;
}

bool DirectoryManager::size_entry_compare_reverse(const FSEntry& e1, const FSEntry& e2) const {
    return e1.size > e2.size;
}

CompareFunction DirectoryManager::compareFunction() {
    CompareFunction cmpFn = &DirectoryManager::path_entry_compare;
    if(mSortingMode == SortingMode::SORT_NAME_DESC)
        cmpFn = &DirectoryManager::path_entry_compare_reverse;
    if(mSortingMode == SortingMode::SORT_TIME)
        cmpFn = &DirectoryManager::date_entry_compare;
    if(mSortingMode == SortingMode::SORT_TIME_DESC)
        cmpFn = &DirectoryManager::date_entry_compare_reverse;
    if(mSortingMode == SortingMode::SORT_SIZE)
        cmpFn = &DirectoryManager::size_entry_compare;
    if(mSortingMode == SortingMode::SORT_SIZE_DESC)
        cmpFn = &DirectoryManager::size_entry_compare_reverse;
    return cmpFn;
}

void DirectoryManager::startFileWatcher(QString directoryPath) {
    if(directoryPath == "")
        return;

    if(watcher && watcher->isObserving() && watcher->watchPath() == directoryPath)
        return; // already watching this exact path, avoid a redundant setWatchPath() cycle

    if(!watcher)
        watcher = DirectoryWatcher::newInstance();

    connect(watcher, &DirectoryWatcher::fileCreated,  this, &DirectoryManager::onFileAddedExternal,    Qt::UniqueConnection);
    connect(watcher, &DirectoryWatcher::fileDeleted,  this, &DirectoryManager::onFileRemovedExternal,  Qt::UniqueConnection);
    connect(watcher, &DirectoryWatcher::fileModified, this, &DirectoryManager::onFileModifiedExternal, Qt::UniqueConnection);
    connect(watcher, &DirectoryWatcher::fileRenamed,  this, &DirectoryManager::onFileRenamedExternal,  Qt::UniqueConnection);

    watcher->setWatchPath(directoryPath);
    watcher->observe();
}

void DirectoryManager::stopFileWatcher() {
    if(!watcher)
        return;

    watcher->stopObserving();

    disconnect(watcher, &DirectoryWatcher::fileCreated,  this, &DirectoryManager::onFileAddedExternal);
    disconnect(watcher, &DirectoryWatcher::fileDeleted,  this, &DirectoryManager::onFileRemovedExternal);
    disconnect(watcher, &DirectoryWatcher::fileModified, this, &DirectoryManager::onFileModifiedExternal);
    disconnect(watcher, &DirectoryWatcher::fileRenamed,  this, &DirectoryManager::onFileRenamedExternal);
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

void DirectoryManager::readSettings() {
    mFormatFilter = settings->formatFilter();
    rebuildRegex();
}

void DirectoryManager::setFormatFilter(QStringList extensions) {
    mFormatFilter = extensions;
    rebuildRegex();
}

void DirectoryManager::setNameFilter(QString nameFilter) {
    mNameFilter = std::move(nameFilter);
}

void DirectoryManager::rebuildRegex() {
    QString pattern;
    if (mFormatFilter.isEmpty()) {
        pattern = settings->supportedFormatsRegex();
    } else {
        QSet<QString> supported;
        for (const QByteArray &format : settings->supportedFormats())
            supported.insert(QString(format).toLower());

        QStringList active;
        for (const QString &ext : mFormatFilter) {
            QString lower = ext.toLower();
            if (supported.contains(lower))
                active << lower;
        }

        if (active.isEmpty())
            pattern = settings->supportedFormatsRegex();
        else
            pattern = ".*\\.(" + active.join("|") + ")$";
    }
    regex.setPattern(pattern);
}

bool DirectoryManager::setDirectory(QString dirPath) {
    if (currentScanCancelled) {
        currentScanCancelled->store(true);
        isScanning = false;
    }

    if(dirPath.isEmpty()) {
        fileEntryVec.clear();
        dirEntryVec.clear();
        fileLookupMap.clear();
        dirLookupMap.clear();
        mDirectoryPath = "";
        stopFileWatcher();
        return true;
    }
    if(!std::filesystem::exists(toStdString(dirPath))) {
        qWarning() << "[DirectoryManager] Error - path does not exist.";
        return false;
    }
    if(!std::filesystem::is_directory(toStdString(dirPath))) {
        qWarning() << "[DirectoryManager] Error - path is not a directory.";
        return false;
    }
    QDir dir(dirPath);
    if(!dir.isReadable()) {
        qWarning() << "[DirectoryManager] Error - cannot read directory.";
        return false;
    }
    mListSource = SOURCE_DIRECTORY;
    mDirectoryPath = dirPath;

    fileEntryVec.clear();
    dirEntryVec.clear();
    fileLookupMap.clear();
    dirLookupMap.clear();

    currentScanCancelled = std::make_shared<std::atomic<bool>>(false);
    isScanning = true;

    DirectoryScanRequest scanRequest{dirPath, false, settings->showHiddenFiles(), regex,
                                     mNameFilter};
    auto scanner = new DirectoryScanner(scanRequest, this, currentScanCancelled);
    QThreadPool::globalInstance()->start(scanner);

    return true;
}

bool DirectoryManager::setDirectoryRecursive(QString dirPath) {
    if (currentScanCancelled) {
        currentScanCancelled->store(true);
        isScanning = false;
    }

    if(dirPath.isEmpty()) {
        return false;
    }
    if(!std::filesystem::exists(toStdString(dirPath))) {
        qWarning() << "[DirectoryManager] Error - path does not exist.";
        return false;
    }
    if(!std::filesystem::is_directory(toStdString(dirPath))) {
        qWarning() << "[DirectoryManager] Error - path is not a directory.";
        return false;
    }
    stopFileWatcher();
    mListSource = SOURCE_DIRECTORY_RECURSIVE;
    mDirectoryPath = dirPath;

    fileEntryVec.clear();
    dirEntryVec.clear();
    fileLookupMap.clear();
    dirLookupMap.clear();

    currentScanCancelled = std::make_shared<std::atomic<bool>>(false);
    isScanning = true;

    DirectoryScanRequest scanRequest{dirPath, true, settings->showHiddenFiles(), regex,
                                     mNameFilter};
    auto scanner = new DirectoryScanner(scanRequest, this, currentScanCancelled);
    QThreadPool::globalInstance()->start(scanner);

    return true;
}

bool DirectoryManager::setFileList(const QStringList &filePaths) {
    if (currentScanCancelled) {
        currentScanCancelled->store(true);
        isScanning = false;
    }
    if(filePaths.isEmpty()) {
        return false;
    }
    stopFileWatcher();
    fileEntryVec.clear();
    dirEntryVec.clear();
    fileLookupMap.clear();
    dirLookupMap.clear();
    mListSource = SOURCE_LIST;
    mDirectoryPath = "";
    
    std::error_code ec;
    for(const QString& path : filePaths) {
        if(isFile(path)) {
            fs::path stdPath(toStdString(path));
            QString fileName = QString::fromStdWString(stdPath.filename().wstring());
            FSEntry entry(path, fileName, fs::file_size(stdPath, ec), fs::last_write_time(stdPath, ec), false);
            fileEntryVec.emplace_back(entry);
        }
    }
    sortEntryLists();
    emit loaded("");
    return true;
}

QString DirectoryManager::directoryPath() const {
    if(mListSource == SOURCE_DIRECTORY || mListSource == SOURCE_DIRECTORY_RECURSIVE)
        return mDirectoryPath;
    else
        return "";
}

int DirectoryManager::indexOfFile(QString filePath) const {
    auto it = fileLookupMap.find(lookupKey(filePath));
    if(it != fileLookupMap.end())
        return it.value();
    return -1;
}

int DirectoryManager::indexOfDir(QString dirPath) const {
    auto it = dirLookupMap.find(lookupKey(dirPath));
    if(it != dirLookupMap.end())
        return it.value();
    return -1;
}

QString DirectoryManager::filePathAt(int index) const {
    return checkFileRange(index) ? fileEntryVec.at(index).path : "";
}

QString DirectoryManager::fileNameAt(int index) const {
    return checkFileRange(index) ? fileEntryVec.at(index).name : "";
}

QString DirectoryManager::dirPathAt(int index) const {
    return checkDirRange(index) ? dirEntryVec.at(index).path : "";
}

QString DirectoryManager::dirNameAt(int index) const {
    return checkDirRange(index) ? dirEntryVec.at(index).name : "";
}

QString DirectoryManager::firstFile() const {
    QString filePath = "";
    if(fileEntryVec.size())
        filePath = fileEntryVec.front().path;
    return filePath;
}

QString DirectoryManager::lastFile() const {
    QString filePath = "";
    if(fileEntryVec.size())
        filePath = fileEntryVec.back().path;
    return filePath;
}

QString DirectoryManager::prevOfFile(QString filePath) const {
    QString prevFilePath = "";
    int currentIndex = indexOfFile(filePath);
    if(currentIndex > 0)
        prevFilePath = fileEntryVec.at(currentIndex - 1).path;
    return prevFilePath;
}

QString DirectoryManager::nextOfFile(QString filePath) const {
    QString nextFilePath = "";
    int currentIndex = indexOfFile(filePath);
    if(currentIndex >= 0 && currentIndex < fileEntryVec.size() - 1)
        nextFilePath = fileEntryVec.at(currentIndex + 1).path;
    return nextFilePath;
}

QString DirectoryManager::prevOfDir(QString dirPath) const {
    QString prevDirectoryPath = "";
    int currentIndex = indexOfDir(dirPath);
    if(currentIndex > 0)
        prevDirectoryPath = dirEntryVec.at(currentIndex - 1).path;
    return prevDirectoryPath;
}

QString DirectoryManager::nextOfDir(QString dirPath) const {
    QString nextDirectoryPath = "";
    int currentIndex = indexOfDir(dirPath);
    if(currentIndex >= 0 && currentIndex < dirEntryVec.size() - 1)
        nextDirectoryPath = dirEntryVec.at(currentIndex + 1).path;
    return nextDirectoryPath;
}

bool DirectoryManager::checkFileRange(int index) const {
    return index >= 0 && index < (int)fileEntryVec.size();
}

bool DirectoryManager::checkDirRange(int index) const {
    return index >= 0 && index < (int)dirEntryVec.size();
}

unsigned long DirectoryManager::totalCount() const {
    return fileCount() + dirCount();
}

unsigned long DirectoryManager::fileCount() const {
    return (unsigned long)fileEntryVec.size();
}

unsigned long DirectoryManager::dirCount() const {
    return (unsigned long)dirEntryVec.size();
}

const FSEntry &DirectoryManager::fileEntryAt(int index) const {
    if(checkFileRange(index))
        return fileEntryVec.at(index);
    else
        return defaultEntry;
}

QDateTime DirectoryManager::lastModified(QString filePath) const {
    QFileInfo info;
    if(containsFile(filePath))
        info.setFile(filePath);
    return info.lastModified();
}

inline
bool DirectoryManager::isSupportedFile(QString path) const {
    const QString fileName = QFileInfo(path).fileName();
    if (!isFile(path) || !regex.match(fileName).hasMatch() ||
        !matchesNameFilter(fileName, mNameFilter))
        return false;
    return isCandidateFileSupported(path, path);
}

bool DirectoryManager::isFile(QString path) const {
    std::error_code ec;
    auto stdPath = toStdString(path);
    return fs::exists(stdPath, ec) && fs::is_regular_file(stdPath, ec);
}

bool DirectoryManager::isDir(QString path) const {
    std::error_code ec;
    auto stdPath = toStdString(path);
    return fs::exists(stdPath, ec) && fs::is_directory(stdPath, ec);
}

bool DirectoryManager::isEmpty() const {
    return fileEntryVec.empty();
}

bool DirectoryManager::containsFile(QString filePath) const {
    return fileLookupMap.contains(lookupKey(filePath));
}

bool DirectoryManager::containsDir(QString dirPath) const {
    return dirLookupMap.contains(lookupKey(dirPath));
}

// ##############################################################
// ###################### PRIVATE METHODS #######################
// ##############################################################
void DirectoryManager::loadEntryList(QString directoryPath, bool recursive) {
    dirEntryVec.clear();
    fileEntryVec.clear();
    if(recursive) { // load files only
        addEntriesFromDirectoryRecursive(fileEntryVec, directoryPath);
    } else { // load dirs & files
        addEntriesFromDirectory(fileEntryVec, directoryPath);
    }
}

// both directories & files
void DirectoryManager::addEntriesFromDirectory(std::vector<FSEntry> &entryVec, QString directoryPath) {
    QRegularExpressionMatch match;
    std::error_code ec;
    auto stdPath = toStdString(directoryPath);
    if(!fs::exists(stdPath, ec) || !fs::is_directory(stdPath, ec)) {
        return;
    }

    bool showHiddenFiles = settings->showHiddenFiles();
    for(const auto & entry : fs::directory_iterator(stdPath, ec)) {
        if(ec) break;
        QString name = QString::fromStdWString(entry.path().filename().generic_wstring());

        bool isDir = false;
        try {
            isDir = entry.is_directory();
        } catch (...) {
            continue;
        }

        QString path = QString::fromStdWString(entry.path().generic_wstring());

        if(isDir) {
            if(!showHiddenFiles) {
                DWORD attributes = GetFileAttributes(entry.path().c_str());
                if(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN))
                    continue;
            }
            FSEntry newEntry;
            try {
                newEntry.name = name;
                newEntry.path = path;
                newEntry.isDirectory = true;
            } catch (const std::filesystem::filesystem_error &err) {
                qWarning() << "[DirectoryManager]" << err.what();
                continue;
            }
            dirEntryVec.emplace_back(newEntry);
        } else {
            match = regex.match(name);
            if(match.hasMatch()) {
                if(!showHiddenFiles) {
                    DWORD attributes = GetFileAttributes(entry.path().c_str());
                    if(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN))
                        continue;
                }
                if(isCandidateFileSupported(name, path)) {
                    FSEntry newEntry;
                    try {
                        newEntry.name = name;
                        newEntry.path = path;
                        newEntry.isDirectory = false;
                        newEntry.size = entry.file_size();
                        newEntry.modifyTime = entry.last_write_time();
                    } catch (const std::filesystem::filesystem_error &err) {
                        qWarning() << "[DirectoryManager]" << err.what();
                        continue;
                    }
                    entryVec.emplace_back(newEntry);
                }
            }
        }
    }
}

void DirectoryManager::addEntriesFromDirectoryRecursive(std::vector<FSEntry> &entryVec, QString directoryPath) {
    QRegularExpressionMatch match;
    std::error_code ec;
    auto stdPath = toStdString(directoryPath);
    if(!fs::exists(stdPath, ec) || !fs::is_directory(stdPath, ec)) {
        return;
    }

    for(const auto & entry : fs::recursive_directory_iterator(stdPath, ec)) {
        if(ec) break;
        bool isDir = false;
        try {
            isDir = entry.is_directory();
        } catch (...) {
            continue;
        }

        if(isDir) {
            continue;
        }

        QString name = QString::fromStdWString(entry.path().filename().generic_wstring());
        match = regex.match(name);
        if(match.hasMatch()) {
            QString path = QString::fromStdWString(entry.path().generic_wstring());
            if(isCandidateFileSupported(name, path)) {
                FSEntry newEntry;
                try {
                    newEntry.name = name;
                    newEntry.path = path;
                    newEntry.isDirectory = false;
                    newEntry.size = entry.file_size();
                    newEntry.modifyTime = entry.last_write_time();
                } catch (const std::filesystem::filesystem_error &err) {
                    qWarning() << "[DirectoryManager]" << err.what();
                    continue;
                }
                entryVec.emplace_back(newEntry);
            }
        }
    }
}

void DirectoryManager::sortEntryLists() {
    if(settings->sortFolders())
        std::ranges::sort(dirEntryVec, comparator());
    else
        std::ranges::sort(dirEntryVec, pathComparator());
    std::ranges::sort(fileEntryVec, comparator());
    rebuildFileLookupMap();
    rebuildDirLookupMap();
}

void DirectoryManager::setSortingMode(SortingMode mode) {
    if(mode != mSortingMode) {
        mSortingMode = mode;
        if(fileEntryVec.size() > 1 || dirEntryVec.size() > 1) {
            sortEntryLists();
            emit sortingChanged();
        }
    }
}

SortingMode DirectoryManager::sortingMode() const {
    return mSortingMode;
}

// Entry management

bool DirectoryManager::insertFileEntry(const QString &filePath) {
    if(!isSupportedFile(filePath))
        return false;
    return forceInsertFileEntry(filePath);
}

// skips filename regex check
bool DirectoryManager::forceInsertFileEntry(const QString &filePath) {
    if(!this->isFile(filePath) || containsFile(filePath))
        return false;
    std::error_code ec;
    fs::path stdPath(toStdString(filePath));
    QString fileName = QString::fromStdWString(stdPath.filename().wstring());
    
    FSEntry FSEntry(filePath, fileName, fs::file_size(stdPath, ec), fs::last_write_time(stdPath, ec), false);
    insert_sorted(fileEntryVec, FSEntry, comparator());
    rebuildFileLookupMap();
    if(!directoryPath().isEmpty()) {
        qDebug() << "fileIns" << filePath << directoryPath();
        emit fileAdded(filePath);
    }
    return true;
}

void DirectoryManager::removeFileEntry(const QString &filePath) {
    if(!containsFile(filePath))
        return;
    int index = indexOfFile(filePath);
    fileEntryVec.erase(fileEntryVec.begin() + index);
    rebuildFileLookupMap();
    qDebug() << "fileRem" << filePath;
    emit fileRemoved(filePath, index);
}

void DirectoryManager::updateFileEntry(const QString &filePath) {
    if(!containsFile(filePath))
        return;
    FSEntry newEntry(filePath);
    if(newEntry.path.isEmpty()) {
        qWarning() << "[DirectoryManager] Cannot refresh file metadata:"
                   << filePath;
        return;
    }
    int index = indexOfFile(filePath);
    if(fileEntryVec.at(index).modifyTime != newEntry.modifyTime ||
       fileEntryVec.at(index).size != newEntry.size)
        fileEntryVec.at(index) = newEntry;
    qDebug() << "fileMod" << filePath;
    emit fileModified(filePath);
}

void DirectoryManager::renameFileEntry(const QString &oldFilePath, const QString &newFileName) {
    QFileInfo fi(oldFilePath);
    QString newFilePath = fi.absolutePath() + "/" + newFileName;
    if(!containsFile(oldFilePath)) {
        if(containsFile(newFilePath))
            updateFileEntry(newFilePath);
        else
            insertFileEntry(newFilePath);
        return;
    }
    if(!isSupportedFile(newFilePath)) {
        removeFileEntry(oldFilePath);
        return;
    }
    if(containsFile(newFilePath)) {
        int replaceIndex = indexOfFile(newFilePath);
        fileEntryVec.erase(fileEntryVec.begin() + replaceIndex);
        rebuildFileLookupMap();
        emit fileRemoved(newFilePath, replaceIndex);
    }
    // remove the old one
    int oldIndex = indexOfFile(oldFilePath);
    fileEntryVec.erase(fileEntryVec.begin() + oldIndex);
    rebuildFileLookupMap();
    // insert
    std::error_code ec;
    fs::path stdPath(toStdString(newFilePath));
    FSEntry FSEntry(newFilePath, newFileName, fs::file_size(stdPath, ec), fs::last_write_time(stdPath, ec), false);
    insert_sorted(fileEntryVec, FSEntry, comparator());
    rebuildFileLookupMap();
    qDebug() << "fileRen" << oldFilePath << newFilePath;
    emit fileRenamed(oldFilePath, oldIndex, newFilePath, indexOfFile(newFilePath));
}

// ---- dir entries

bool DirectoryManager::insertDirEntry(const QString &dirPath) {
    if(containsDir(dirPath))
        return false;
    QString dirName = QFileInfo(dirPath).fileName();
    FSEntry FSEntry;
    FSEntry.name = dirName;
    FSEntry.path = dirPath;
    FSEntry.isDirectory = true;
    insert_sorted(dirEntryVec, FSEntry, comparator());
    rebuildDirLookupMap();
    qDebug() << "dirIns" << dirPath;
    emit dirAdded(dirPath);
    return true;
}

void DirectoryManager::removeDirEntry(const QString &dirPath) {
    if(!containsDir(dirPath))
        return;
    int index = indexOfDir(dirPath);
    dirEntryVec.erase(dirEntryVec.begin() + index);
    rebuildDirLookupMap();
    qDebug() << "dirRem" << dirPath;
    emit dirRemoved(dirPath, index);
}

void DirectoryManager::renameDirEntry(const QString &oldDirPath, const QString &newDirName) {
    if(!containsDir(oldDirPath))
        return;
    QFileInfo fi(oldDirPath);
    QString newDirPath = fi.absolutePath() + "/" + newDirName;
    // remove the old one
    int oldIndex = indexOfDir(oldDirPath);
    dirEntryVec.erase(dirEntryVec.begin() + oldIndex);
    rebuildDirLookupMap();
    // insert
    FSEntry FSEntry;
    FSEntry.name = newDirName;
    FSEntry.path = newDirPath;
    FSEntry.isDirectory = true;
    insert_sorted(dirEntryVec, FSEntry, comparator());
    rebuildDirLookupMap();
    qDebug() << "dirRen" << oldDirPath << newDirPath;
    emit dirRenamed(oldDirPath, oldIndex, newDirPath, indexOfDir(newDirPath));
}


FileListSource DirectoryManager::source() const {
    return mListSource;
}

QStringList DirectoryManager::fileList() const {
    QStringList list;
    for(auto const& value : fileEntryVec)
        list << value.path;
    return list;
}

bool DirectoryManager::fileWatcherActive() {
    if(!watcher)
        return false;
    return watcher->isObserving();
}

void DirectoryManager::beginSelfWrite(const QString &filePath) {
    SelfWriteState &state = selfWriteStates[lookupKey(filePath)];
    ++state.refCount;
}

void DirectoryManager::scheduleEndSelfWrite(const QString &filePath, int firstEventTimeoutMs, int quietPeriodMs) {
    const QString key = lookupKey(filePath);
    auto it = selfWriteStates.find(key);
    if(it == selfWriteStates.end())
        return;
    SelfWriteState &state = it.value();
    ++state.pendingEnds;
    state.quietPeriodMs = quietPeriodMs;
    if(!state.timer) {
        state.timer = new QTimer(this);
        state.timer->setSingleShot(true);
        connect(state.timer, &QTimer::timeout, this, [this, key]() {
            finalizeSelfWrite(key);
        });
    }
    state.timer->start(firstEventTimeoutMs);
}

void DirectoryManager::noteSelfWriteActivity(const QString &filePath) {
    const QString key = lookupKey(filePath);
    auto it = selfWriteStates.find(key);
    if(it == selfWriteStates.end())
        return;
    SelfWriteState &state = it.value();
    // Only relevant once someone is actually waiting to end - if we're
    // still in the middle of the write itself (scheduleEndSelfWrite() not
    // called yet), there's no timer running to extend.
    if(state.pendingEnds > 0 && state.timer)
        state.timer->start(state.quietPeriodMs);
}

void DirectoryManager::finalizeSelfWrite(const QString &key) {
    auto it = selfWriteStates.find(key);
    if(it == selfWriteStates.end())
        return;
    SelfWriteState &state = it.value();
    state.refCount -= state.pendingEnds;
    state.pendingEnds = 0;
    if(state.refCount <= 0) {
        state.timer->deleteLater();
        selfWriteStates.erase(it);
    }
}

bool DirectoryManager::isSelfWrite(const QString &filePath) const {
    return selfWriteStates.contains(lookupKey(filePath));
}

//----------------------------------------------------------------------------
// fs watcher events  ( onFile___External() )
// these take file NAMES, not paths
void DirectoryManager::onFileRemovedExternal(QString fileName) {
    if (FileOperations::isInternalArtifact(fileName))
        return;
    QString fullPath = watcher->watchPath() + "/" + fileName;
    if (isSelfWrite(fullPath)) {
        noteSelfWriteActivity(fullPath);
        return;
    }
    removeDirEntry(fullPath);
    removeFileEntry(fullPath);
}

void DirectoryManager::onFileAddedExternal(QString fileName) {
    if (FileOperations::isInternalArtifact(fileName))
        return;
    QString fullPath = watcher->watchPath() + "/" + fileName;
    if (isSelfWrite(fullPath)) {
        noteSelfWriteActivity(fullPath);
        return;
    }
    if(isDir(fullPath))
        insertDirEntry(fullPath);
    else
        insertFileEntry(fullPath);
}

void DirectoryManager::onFileRenamedExternal(QString oldName, QString newName) {
    // FileOperations' atomic save can rename/replace the destination file
    // through a temp-write + backup-and-replace sequence. Depending on the
    // filesystem/OS, that can surface here as ordinary renames or as direct
    // add/remove of the real destination name - either way, without these
    // guards it can look like "the current file changed identity", which
    // removes it from the list and makes Core auto-advance to the next
    // image, even though nothing actually changed from the user's
    // perspective. isInternalArtifact() catches the temp/backup names
    // themselves; isSelfWrite() catches the real destination path for
    // whatever raw shape the OS reports while we're the one writing it.
    if (FileOperations::isInternalArtifact(oldName) ||
        FileOperations::isInternalArtifact(newName)) {
        return;
    }
    QString oldPath = watcher->watchPath() + "/" + oldName;
    QString newPath = watcher->watchPath() + "/" + newName;
    if (isSelfWrite(oldPath) || isSelfWrite(newPath)) {
        noteSelfWriteActivity(oldPath);
        noteSelfWriteActivity(newPath);
        return;
    }
    if(isDir(newPath))
        renameDirEntry(oldPath, newName);
    else
        renameFileEntry(oldPath, newName);
}

void DirectoryManager::onFileModifiedExternal(QString fileName) {
    if (FileOperations::isInternalArtifact(fileName))
        return;
    QString fullPath = watcher->watchPath() + "/" + fileName;
    if (isSelfWrite(fullPath)) {
        noteSelfWriteActivity(fullPath);
        return;
    }
    updateFileEntry(fullPath);
}

void DirectoryManager::rebuildFileLookupMap() {
    fileLookupMap.clear();
    fileLookupMap.reserve(fileEntryVec.size());
    for(int i = 0; i < (int)fileEntryVec.size(); ++i) {
        fileLookupMap.insert(lookupKey(fileEntryVec[i].path), i);
    }
}

void DirectoryManager::rebuildDirLookupMap() {
    dirLookupMap.clear();
    dirLookupMap.reserve(dirEntryVec.size());
    for(int i = 0; i < (int)dirEntryVec.size(); ++i) {
        dirLookupMap.insert(lookupKey(dirEntryVec[i].path), i);
    }
}

void DirectoryManager::handleScanFinished(const QString &path, std::vector<FSEntry> files, std::vector<FSEntry> dirs) {
    fileEntryVec = std::move(files);
    dirEntryVec = std::move(dirs);

    // Sort files
    std::ranges::sort(fileEntryVec, comparator());
    rebuildFileLookupMap();

    // Sort directories
    if (settings->sortFolders()) {
        std::ranges::sort(dirEntryVec, comparator());
    } else {
        std::ranges::sort(dirEntryVec, pathComparator());
    }
    rebuildDirLookupMap();

    isScanning = false;
    emit loaded(path);

    if (mListSource == SOURCE_DIRECTORY) {
        startFileWatcher(path);
    }
}
