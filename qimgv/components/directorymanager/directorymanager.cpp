#include "directorymanager.h"
#include "settings.h"

#include <QThreadPool>
#include <QRunnable>
#include <QPointer>
#include <QSet>
#include <atomic>
#include <memory>

namespace fs = std::filesystem;

class DirectoryScanner : public QRunnable {
public:
    DirectoryScanner(QString directoryPath,
                     bool recursive,
                     bool showHiddenFiles,
                     QRegularExpression regex,
                     QPointer<DirectoryManager> manager,
                     std::shared_ptr<std::atomic<bool>> cancelled)
        : directoryPath(directoryPath),
          recursive(recursive),
          showHiddenFiles(showHiddenFiles),
          regex(regex),
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
        auto stdPath = toStdString(directoryPath);

        if (recursive) {
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
#ifdef Q_OS_WIN32
                    if (!showHiddenFiles) {
                        DWORD attributes = GetFileAttributes(entry.path().c_str());
                        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
                            it.disable_recursion_pending();
                        }
                    }
#else
                    if (!showHiddenFiles && name.startsWith(".")) {
                        it.disable_recursion_pending();
                    }
#endif
                    it.increment(ec);
                    continue;
                }

                auto match = regex.match(name);
                if (match.hasMatch()) {
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

#ifndef Q_OS_WIN32
                if (!showHiddenFiles && name.startsWith(".")) {
                    it.increment(ec);
                    continue;
                }
#else
                DWORD attributes = GetFileAttributes(entry.path().c_str());
                if (!showHiddenFiles && attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN)) {
                    it.increment(ec);
                    continue;
                }
#endif

                bool isDir = false;
                try {
                    isDir = entry.is_directory();
                } catch (...) {
                    it.increment(ec);
                    continue;
                }

                if (isDir) {
                    FSEntry newEntry;
                    newEntry.name = name;
                    newEntry.path = path;
                    newEntry.isDirectory = true;
                    dirs.emplace_back(newEntry);
                } else {
                    auto match = regex.match(name);
                    if (match.hasMatch()) {
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
                it.increment(ec);
            }
        }

        if (cancelled && cancelled->load()) return;

        QPointer<DirectoryManager> mgr = manager;
        QMetaObject::invokeMethod(mgr, [mgr, path = directoryPath, files = std::move(files), dirs = std::move(dirs), cancelled = this->cancelled]() mutable {
            if (mgr && (!cancelled || !cancelled->load())) {
                mgr->handleScanFinished(path, std::move(files), std::move(dirs));
            }
        });
    }

private:
    QString directoryPath;
    bool recursive;
    bool showHiddenFiles;
    QRegularExpression regex;
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
    regex.setPattern(settings->supportedFormatsRegex());
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

    auto scanner = new DirectoryScanner(
        dirPath,
        false, // recursive
        settings->showHiddenFiles(),
        regex,
        this,
        currentScanCancelled
    );
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

    auto scanner = new DirectoryScanner(
        dirPath,
        true, // recursive
        settings->showHiddenFiles(),
        regex,
        this,
        currentScanCancelled
    );
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
    return ( isFile(path) && regex.match(path).hasMatch() );
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

    for(const auto & entry : fs::directory_iterator(stdPath, ec)) {
        if(ec) break;
        QString name = QString::fromStdWString(entry.path().filename().generic_wstring());
#ifndef Q_OS_WIN32
        // ignore hidden files
        if(!settings->showHiddenFiles() && name.startsWith("."))
            continue;
#else
        DWORD attributes = GetFileAttributes(entry.path().c_str());
        if(!settings->showHiddenFiles() && attributes & FILE_ATTRIBUTE_HIDDEN)
            continue;
#endif
        QString path = QString::fromStdWString(entry.path().generic_wstring());
        match = regex.match(name);
        if(entry.is_directory()) { // this can still throw std::bad_alloc ..
            FSEntry newEntry;
            try {
                newEntry.name = name;
                newEntry.path = path;
                newEntry.isDirectory = true;
                //newEntry.size = entry.file_size();
                //newEntry.modifyTime = entry.last_write_time();
            } catch (const std::filesystem::filesystem_error &err) {
                qWarning() << "[DirectoryManager]" << err.what();
                continue;
            }
            dirEntryVec.emplace_back(newEntry);
        } else if (match.hasMatch()) {
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

void DirectoryManager::addEntriesFromDirectoryRecursive(std::vector<FSEntry> &entryVec, QString directoryPath) {
    QRegularExpressionMatch match;
    std::error_code ec;
    auto stdPath = toStdString(directoryPath);
    if(!fs::exists(stdPath, ec) || !fs::is_directory(stdPath, ec)) {
        return;
    }

    for(const auto & entry : fs::recursive_directory_iterator(stdPath, ec)) {
        if(ec) break;
        QString name = QString::fromStdWString(entry.path().filename().generic_wstring());
        QString path = QString::fromStdWString(entry.path().generic_wstring());
        match = regex.match(name);
        if(!entry.is_directory() && match.hasMatch()) {
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

void DirectoryManager::sortEntryLists() {
    if(settings->sortFolders())
        std::sort(dirEntryVec.begin(), dirEntryVec.end(), std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
    else
        std::sort(dirEntryVec.begin(), dirEntryVec.end(), std::bind(&DirectoryManager::path_entry_compare, this, std::placeholders::_1, std::placeholders::_2));
    std::sort(fileEntryVec.begin(), fileEntryVec.end(), std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
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
    insert_sorted(fileEntryVec, FSEntry, std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
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
    int index = indexOfFile(filePath);
    if(fileEntryVec.at(index).modifyTime != newEntry.modifyTime)
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
    insert_sorted(fileEntryVec, FSEntry, std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
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
    insert_sorted(dirEntryVec, FSEntry, std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
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
    insert_sorted(dirEntryVec, FSEntry, std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
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

//----------------------------------------------------------------------------
// fs watcher events  ( onFile___External() )
// these take file NAMES, not paths
void DirectoryManager::onFileRemovedExternal(QString fileName) {
    QString fullPath = watcher->watchPath() + "/" + fileName;
    removeDirEntry(fullPath);
    removeFileEntry(fullPath);
}

void DirectoryManager::onFileAddedExternal(QString fileName) {
    QString fullPath = watcher->watchPath() + "/" + fileName;
    if(isDir(fullPath))
        insertDirEntry(fullPath);
    else
        insertFileEntry(fullPath);
}

void DirectoryManager::onFileRenamedExternal(QString oldName, QString newName) {
    QString oldPath = watcher->watchPath() + "/" + oldName;
    QString newPath = watcher->watchPath() + "/" + newName;
    if(isDir(newPath))
        renameDirEntry(oldPath, newName);
    else
        renameFileEntry(oldPath, newName);
}

void DirectoryManager::onFileModifiedExternal(QString fileName) {
    updateFileEntry(watcher->watchPath() + "/" + fileName);
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
    std::sort(fileEntryVec.begin(), fileEntryVec.end(), std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
    rebuildFileLookupMap();

    // Sort directories
    if (settings->sortFolders()) {
        std::sort(dirEntryVec.begin(), dirEntryVec.end(), std::bind(compareFunction(), this, std::placeholders::_1, std::placeholders::_2));
    } else {
        std::sort(dirEntryVec.begin(), dirEntryVec.end(), std::bind(&DirectoryManager::path_entry_compare, this, std::placeholders::_1, std::placeholders::_2));
    }
    rebuildDirLookupMap();

    isScanning = false;
    emit loaded(path);

    if (mListSource == SOURCE_DIRECTORY) {
        startFileWatcher(path);
    }
}
