#ifndef WINDOWSWATCHERWORKER_H
#define WINDOWSWATCHERWORKER_H

#include <windows.h>
#include <vector>
#include <cstdint>
#include "../watcherworker.h"
#include <QDebug>

class WindowsWorker : public WatcherWorker {
    Q_OBJECT
public:
    WindowsWorker();
    ~WindowsWorker();

    // Called from the main thread. Posts the new handle to the worker's
    // IOCP; never touches worker-owned state directly, so there is no
    // cross-thread race on hDir/ovl anymore.
    void setDirectoryHandle(HANDLE hDir);

    void setRunning(bool running) override;
    virtual void run() override;

signals:
    void notifyEvent(int action, const QString &fileName);

private:
    // One watch "session" for a single directory. Only the worker thread
    // ever reads/writes the contents of this struct once it's been posted.
    struct DirWatchCtx {
        OVERLAPPED ovl{};
        HANDLE hDir = INVALID_HANDLE_VALUE;
        std::vector<BYTE> buffer;
        uint64_t generation = 0;
    };

    // Completion keys used to tell real ReadDirectoryChangesW completions
    // apart from our own synthetic "switch directory" / "quit" packets.
    static constexpr ULONG_PTR KEY_DIRWATCH = 1;
    static constexpr ULONG_PTR KEY_SWITCH   = 2;
    static constexpr ULONG_PTR KEY_QUIT     = 3;

    HANDLE hIOCP = nullptr;
    DirWatchCtx* currentCtx = nullptr;
    uint64_t currentGeneration = 0;

    void switchTo(DirWatchCtx* newCtx);
    void issueRead(DirWatchCtx* ctx);
    void processNotifications(const BYTE* buffer);
};

#endif // WINDOWSWATCHERWORKER_H
