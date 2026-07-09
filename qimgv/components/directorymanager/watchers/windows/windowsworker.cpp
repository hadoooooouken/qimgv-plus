#include "windowsworker.h"

WindowsWorker::WindowsWorker() : WatcherWorker() {
    // Created once, lives for the whole lifetime of the worker object —
    // independent of which directory is currently being watched, and
    // independent of whether the worker thread has started yet.
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (!hIOCP) {
        qCritical() << "[WindowsWorker] CreateIoCompletionPort failed:" << GetLastError();
    }
}

WindowsWorker::~WindowsWorker() {
    // Best-effort cleanup in case the worker thread was torn down without
    // going through a clean run() exit (should not normally happen if the
    // caller stops observing before destroying the watcher).
    if (currentCtx) {
        if (currentCtx->hDir != INVALID_HANDLE_VALUE) {
            CancelIoEx(currentCtx->hDir, &currentCtx->ovl);
            CloseHandle(currentCtx->hDir);
        }
        delete currentCtx;
        currentCtx = nullptr;
    }
    if (hIOCP) {
        CloseHandle(hIOCP);
        hIOCP = nullptr;
    }
}

void WindowsWorker::setRunning(bool running) {
    WatcherWorker::setRunning(running);
    if (!running && hIOCP) {
        // Wake the worker thread out of GetQueuedCompletionStatus(INFINITE).
        if (!PostQueuedCompletionStatus(hIOCP, 0, KEY_QUIT, nullptr)) {
            qWarning() << "[WindowsWorker] PostQueuedCompletionPort(quit) failed:" << GetLastError();
        }
    }
}

void WindowsWorker::setDirectoryHandle(HANDLE hNewDir) {
    // Called from the main thread. We never touch currentCtx/hDir here —
    // ownership stays entirely with the worker thread. We just hand the
    // new handle over via the completion port, which is a thread-safe
    // kernel object designed exactly for this kind of cross-thread wakeup.
    auto* ctx = new DirWatchCtx();
    ctx->hDir = hNewDir;
    ctx->buffer.resize(64 * 1024);

    if (!hIOCP || !PostQueuedCompletionStatus(hIOCP, 0, KEY_SWITCH, reinterpret_cast<LPOVERLAPPED>(ctx))) {
        qCritical() << "[WindowsWorker] PostQueuedCompletionPort(switch) failed:" << GetLastError();
        CloseHandle(hNewDir);
        delete ctx;
    }
}

void WindowsWorker::issueRead(DirWatchCtx* ctx) {
    ZeroMemory(&ctx->ovl, sizeof(OVERLAPPED));

    // With IOCP, lpBytesReturned and lpCompletionRoutine must be null —
    // the byte count comes back via GetQueuedCompletionStatus instead.
    BOOL ok = ReadDirectoryChangesW(
        ctx->hDir,
        ctx->buffer.data(),
        static_cast<DWORD>(ctx->buffer.size()),
        FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        nullptr,
        &ctx->ovl,
        nullptr);

    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            qCritical() << "[WindowsWorker] ReadDirectoryChangesW failed:" << err;
        }
    }
}

void WindowsWorker::switchTo(DirWatchCtx* newCtx) {
    if (currentCtx) {
        // Cancel the old read and close its handle. The cancelled I/O will
        // still complete asynchronously and arrive later as a completion
        // packet with a stale generation — it gets filtered and freed then,
        // not here. We must NOT delete currentCtx synchronously: the kernel
        // may still write into ctx->ovl until that cancellation completes.
        CancelIoEx(currentCtx->hDir, &currentCtx->ovl);
        CloseHandle(currentCtx->hDir);
        currentCtx->hDir = INVALID_HANDLE_VALUE;
    }

    currentGeneration++;
    newCtx->generation = currentGeneration;

    if (!CreateIoCompletionPort(newCtx->hDir, hIOCP, KEY_DIRWATCH, 0)) {
        qCritical() << "[WindowsWorker] CreateIoCompletionPort(associate) failed:" << GetLastError();
        CloseHandle(newCtx->hDir);
        delete newCtx;
        currentCtx = nullptr;
        return;
    }

    currentCtx = newCtx;
    issueRead(newCtx);
}

void WindowsWorker::processNotifications(const BYTE* buffer) {
    auto* fni = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer);
    for (;;) {
        if (fni->Action != 0) {
            const int len = fni->FileNameLength / sizeof(WCHAR);
            const QString name = QString::fromWCharArray(
                reinterpret_cast<const wchar_t*>(fni->FileName), len);
            emit notifyEvent(fni->Action, name);
        }
        if (fni->NextEntryOffset == 0)
            break;
        fni = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
            reinterpret_cast<const BYTE*>(fni) + fni->NextEntryOffset);
    }
}

void WindowsWorker::run() {
    isRunning = true;
    emit started(); // NB: previously never emitted — see note below.

    for (;;) {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED pOvl = nullptr;

        BOOL ok = GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &completionKey, &pOvl, INFINITE);
        DWORD err = ok ? ERROR_SUCCESS : GetLastError();

        if (completionKey == KEY_QUIT) {
            break;
        }

        if (completionKey == KEY_SWITCH) {
            // pOvl here is really our DirWatchCtx*, posted directly by
            // setDirectoryHandle() — not a genuine OVERLAPPED completion.
            auto* newCtx = reinterpret_cast<DirWatchCtx*>(pOvl);
            switchTo(newCtx);
            continue;
        }

        // completionKey == KEY_DIRWATCH: either a real notification or the
        // completion of a read we cancelled in switchTo().
        if (!pOvl) {
            qCritical() << "[WindowsWorker] GetQueuedCompletionStatus failed:" << err;
            break;
        }

        DirWatchCtx* ctx = CONTAINING_RECORD(pOvl, DirWatchCtx, ovl);

        if (ctx->generation != currentGeneration) {
            // Stale completion from a directory we've already switched
            // away from (or its cancellation). Safe to free now.
            delete ctx;
            continue;
        }

        if (!ok) {
            if (err == ERROR_OPERATION_ABORTED) {
                delete ctx;
                continue;
            }
            qCritical() << "[WindowsWorker] Directory watch failed:" << err;
            continue;
        }

        if (bytesTransferred == 0) {
            qWarning() << "[WindowsWorker] Notification buffer overflow, some changes were lost.";
        } else {
            processNotifications(ctx->buffer.data());
        }

        issueRead(ctx); // re-arm for the next batch on the same, still-current ctx
    }

    if (currentCtx) {
        CancelIoEx(currentCtx->hDir, &currentCtx->ovl);
        CloseHandle(currentCtx->hDir);
        currentCtx->hDir = INVALID_HANDLE_VALUE;

        // Drain completions until we see the one that actually belongs to
        // currentCtx's cancelled read. We must NOT just grab whatever the
        // queue hands us first: with rapid directory switches (A->B->C)
        // there can be stale, still-undelivered cancellation packets from
        // *previous* switchTo() calls sitting ahead of it in the queue
        // (out-of-order delivery is normal for IOCP, and standby/resume
        // widens this window further). Grabbing one of those and deleting
        // currentCtx unconditionally, as before, left the real cancellation
        // for currentCtx unconfirmed - the kernel could still write into
        // ctx->ovl / ctx->buffer after we'd already freed that memory,
        // corrupting the heap. Each packet we actually dequeue is safe to
        // free immediately (its completion is real by definition); we just
        // keep going until the one we free is currentCtx itself.
        const ULONGLONG deadline = GetTickCount64() + 2000;
        while (currentCtx) {
            ULONGLONG now = GetTickCount64();
            if (now >= deadline) {
                // Give up: accept a one-time leak of currentCtx rather than
                // free memory the kernel might still be about to write into.
                currentCtx = nullptr;
                break;
            }

            DWORD bytesTransferred = 0;
            ULONG_PTR key = 0;
            LPOVERLAPPED pOvl = nullptr;
            GetQueuedCompletionStatus(hIOCP, &bytesTransferred, &key, &pOvl,
                                       static_cast<DWORD>(deadline - now));

            if (!pOvl) {
                // Timeout or a fatal error with no packet at all - queue is
                // empty, nothing left to drain.
                currentCtx = nullptr;
                break;
            }

            if (key == KEY_SWITCH) {
                // A switchTo() request we posted to ourselves but never got
                // to process because KEY_QUIT arrived first.
                delete reinterpret_cast<DirWatchCtx*>(pOvl);
                continue;
            }

            DirWatchCtx* ctx = CONTAINING_RECORD(pOvl, DirWatchCtx, ovl);
            bool wasCurrent = (ctx == currentCtx);
            delete ctx; // this completion is genuinely for ctx, safe to free
            if (wasCurrent)
                currentCtx = nullptr;
            // else: stale ctx from an earlier switchTo(), keep waiting
        }
    }

    isRunning = false;
    emit finished(); // NB: previously never emitted — see note below.
}
