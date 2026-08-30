#pragma once

#include <QObject>
#include <QHash>
#include <QCollator>
#include <QElapsedTimer>
#include <QString>
#include <QSize>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QPointer>
#include <QThreadPool>
#include <QRunnable>
#include <QTimer>
#include <atomic>
#include <memory>

#include <vector>
#include <string>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <ranges>

#include "settings_types.h"
#include "watchers/directorywatcher.h"
#include "utils/stuff.h"
#include "sourcecontainers/fsentry.h"

#ifdef Q_OS_WIN32
#include "windows.h"
#endif

enum FileListSource {
    SOURCE_DIRECTORY,
    SOURCE_DIRECTORY_RECURSIVE,
    SOURCE_LIST
};

class DirectoryManager;

typedef bool (DirectoryManager::*CompareFunction)(const FSEntry &e1, const FSEntry &e2) const;

class DirectoryManager : public QObject {
    Q_OBJECT
    friend class DirectoryScanner;
public:
    DirectoryManager();
    ~DirectoryManager();
    bool setDirectory(QString);
    bool setDirectoryRecursive(QString);
    bool setFileList(const QStringList &filePaths);
    QString directoryPath() const;
    int indexOfFile(QString filePath) const;
    int indexOfDir(QString dirPath) const;
    QString filePathAt(int index) const;
    unsigned long fileCount() const;
    unsigned long dirCount() const;
    inline bool isSupportedFile(QString filePath) const;
    bool isEmpty() const;
    bool containsFile(QString filePath) const;
    QString fileNameAt(int index) const;
    QString prevOfFile(QString filePath) const;
    QString nextOfFile(QString filePath) const;
    QString prevOfDir(QString filePath) const;
    QString nextOfDir(QString filePath) const;
    void sortEntryLists();
    QDateTime lastModified(QString filePath) const;

    QString firstFile() const;
    QString lastFile() const;
    void setSortingMode(SortingMode mode);
    SortingMode sortingMode() const;
    void setFormatFilter(QStringList extensions);
    void setNameFilter(QString nameFilter);
    bool isFile(QString path) const;
    bool isDir(QString path) const;

    unsigned long totalCount() const;
    bool containsDir(QString dirPath) const;
    const FSEntry &fileEntryAt(int index) const;
    QString dirPathAt(int index) const;
    QString dirNameAt(int index) const;
    bool fileWatcherActive();

    bool insertFileEntry(const QString &filePath);
    bool forceInsertFileEntry(const QString &filePath);
    void removeFileEntry(const QString &filePath);
    void updateFileEntry(const QString &filePath);
    void renameFileEntry(const QString &oldFilePath, const QString &newName);

    // Callers that write filePath themselves (DirectoryModel::saveFile(), via
    // FileOperations) call beginSelfWrite() before the write and
    // scheduleEndSelfWrite() once it's done. While active, ANY
    // filesystem-watcher event that touches filePath is ignored, regardless
    // of whether the OS reports the write as a plain modify, or as a
    // rename/delete/create sequence (as with FileOperations' temp-write +
    // backup-and-replace mechanism) - the caller already pushes the
    // authoritative update (updateFileEntry()/insertFileEntry() +
    // fileModified()) itself once the write succeeds.
    void beginSelfWrite(const QString &filePath);

    // Lifting suppression can't happen synchronously: the watcher's
    // notification for our own write is relayed from a background thread
    // via a queued connection, so it hasn't arrived yet when the caller's
    // write call returns (see DirectoryModel::saveFile()). Instead of
    // guessing a single fixed delay - which either races ahead of a
    // notification that's simply slow to arrive (busy disk, AV scanning the
    // file right after we wrote it, ...) or, on a multi-event sequence
    // (e.g. a rename pair, or a remove+add pair from the backup-and-replace
    // dance), stops listening after the first one and lets a later one
    // through - suppression is lifted based on actual observed activity:
    //   - firstEventTimeoutMs bounds how long we wait to see ANY matching
    //     event at all; nothing arriving in that window means the write
    //     apparently didn't produce a watcher event for this path, so
    //     there's nothing left to wait for.
    //   - Every time a matching event actually arrives and gets suppressed
    //     (see the onFile___External() handlers below, which call
    //     noteSelfWriteActivity() on a hit), the wait is shortened to just
    //     quietPeriodMs from that point - long enough to catch the rest of
    //     a multi-event sequence without waiting out the full timeout for
    //     each one.
    void scheduleEndSelfWrite(const QString &filePath,
                               int firstEventTimeoutMs = 800,
                               int quietPeriodMs = 150);
    bool isSelfWrite(const QString &filePath) const;

    bool insertDirEntry(const QString &dirPath);
    //bool forceInsertDirEntry(const QString &dirPath);
    void removeDirEntry(const QString &dirPath);
    //void updateDirEntry(const QString &dirPath);
    void renameDirEntry(const QString &oldDirPath, const QString &newName);

    FileListSource source() const;

    QStringList fileList() const;

private:
    QRegularExpression regex;
    QCollator collator;
    std::vector<FSEntry> fileEntryVec, dirEntryVec;
    const FSEntry defaultEntry;
    QString mDirectoryPath;

    QHash<QString, int> fileLookupMap;
    QHash<QString, int> dirLookupMap;
    void rebuildFileLookupMap();
    void rebuildDirLookupMap();
    inline QString lookupKey(const QString &path) const {
#if defined(_WIN32) || defined(Q_OS_WIN) || defined(Q_OS_WIN32)
        return path.toLower();
#else
        return path;
#endif
    }

    // Refcounted so overlapping beginSelfWrite() calls for the same path
    // (shouldn't normally happen with a single in-flight save, but is cheap
    // to make safe) don't let one resolved scheduleEndSelfWrite() lift
    // suppression the other still needs. pendingEnds counts
    // scheduleEndSelfWrite() calls that haven't fired yet; when the timer
    // fires it resolves all of them at once (they're all waiting on the
    // same underlying watcher activity for this path, so there's no reason
    // to stagger them). timer is owned by this DirectoryManager (parented
    // in its constructor) and created lazily on first use.
    struct SelfWriteState {
        int refCount = 0;
        int pendingEnds = 0;
        int quietPeriodMs = 150;
        QTimer *timer = nullptr;
    };
    QHash<QString, SelfWriteState> selfWriteStates;
    // Called from the onFile___External() handlers whenever they suppress
    // an event as belonging to an in-flight self-write, to extend the
    // pending scheduleEndSelfWrite() wait - see the comment on
    // scheduleEndSelfWrite() above.
    void noteSelfWriteActivity(const QString &filePath);
    void finalizeSelfWrite(const QString &key);

    DirectoryWatcher* watcher;
    void readSettings();
    void rebuildRegex();
    SortingMode mSortingMode;
    QStringList mFormatFilter;
    QString mNameFilter;
    FileListSource mListSource;
    void loadEntryList(QString directoryPath, bool recursive);

    bool path_entry_compare(const FSEntry &e1, const FSEntry &e2) const;
    bool path_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const;
    bool name_entry_compare(const FSEntry &e1, const FSEntry &e2) const;
    bool name_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const;
    bool date_entry_compare(const FSEntry &e1, const FSEntry &e2) const;
    bool date_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const;
    CompareFunction compareFunction();
    auto comparator() { auto fn = compareFunction(); return [this, fn](const FSEntry& a, const FSEntry& b) { return (this->*fn)(a, b); }; }
    auto pathComparator() { return [this](const FSEntry& a, const FSEntry& b) { return path_entry_compare(a, b); }; }
    bool size_entry_compare(const FSEntry &e1, const FSEntry &e2) const;
    bool size_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const;
    void startFileWatcher(QString directoryPath);
    void stopFileWatcher();

    void addEntriesFromDirectory(std::vector<FSEntry> &entryVec, QString directoryPath);
    void addEntriesFromDirectoryRecursive(std::vector<FSEntry> &entryVec, QString directoryPath);
    bool checkFileRange(int index) const;
    bool checkDirRange(int index) const;

    std::shared_ptr<std::atomic<bool>> currentScanCancelled;
    bool isScanning = false;
    void handleScanFinished(const QString &path, std::vector<FSEntry> files, std::vector<FSEntry> dirs);

private slots:
    void onFileAddedExternal(QString fileName);
    void onFileRemovedExternal(QString fileName);
    void onFileModifiedExternal(QString fileName);
    void onFileRenamedExternal(QString oldFileName, QString newFileName);

signals:
    void loaded(const QString &path);
    void sortingChanged();
    void fileRemoved(QString filePath, int);
    void fileModified(QString filePath);
    void fileAdded(QString filePath);
    void fileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);

    void dirRemoved(QString dirPath, int);
    void dirAdded(QString dirPath);
    void dirRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
};
