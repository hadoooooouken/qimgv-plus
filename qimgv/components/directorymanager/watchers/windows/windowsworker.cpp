#include "windowsworker.h"

WindowsWorker::WindowsWorker() : WatcherWorker(), hDir(INVALID_HANDLE_VALUE) {
}

WindowsWorker::~WindowsWorker() {
    freeHandle();
}

void WindowsWorker::setDirectoryHandle(HANDLE hDir) {
    freeHandle();
    this->hDir = hDir;
}

void WindowsWorker::freeHandle() {
    if (this->hDir != INVALID_HANDLE_VALUE) {
        CancelIoEx(this->hDir, nullptr);
        CloseHandle(this->hDir);
        this->hDir = INVALID_HANDLE_VALUE;
    }
}

void WindowsWorker::run() {
    isRunning = true;
    DWORD dwBytes = 0;
    std::vector<BYTE> buffer(1024 * 64);
    OVERLAPPED ovl = {0};

    ovl.hEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!ovl.hEvent) {
        qWarning() << "[WindowsWorker] CreateEvent failed";
        isRunning = false;
        return;
    }

    // Issue the first change notification request.
    if (!ReadDirectoryChangesW(hDir,
                               buffer.data(),
                               static_cast<DWORD>(buffer.size()),
                               FALSE,
                               FILE_NOTIFY_CHANGE_FILE_NAME |
                                   FILE_NOTIFY_CHANGE_DIR_NAME |
                                   FILE_NOTIFY_CHANGE_LAST_WRITE,
                               &dwBytes,
                               &ovl,
                               nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            qCritical() << "[WindowsWorker] Initial ReadDirectoryChangesW failed:" << err;
            CloseHandle(ovl.hEvent);
            isRunning = false;
            return;
        }
        // ERROR_IO_PENDING is the expected outcome for an asynchronous call.
    }

    while (isRunning) {
        // Wait for the I/O to complete, but wake up periodically to check the stop flag.
        DWORD waitResult = ::WaitForSingleObject(ovl.hEvent, POLL_RATE_MS);

        // If the thread was asked to stop, wait for the final (cancelled) I/O
        // to complete so the overlapped structure can be safely destroyed.
        if (!isRunning) {
            // freeHandle() should have already called CancelIoEx. Wait until
            // the pending operation finishes (the event will be signaled).
            ::WaitForSingleObject(ovl.hEvent, INFINITE);
            break; // clean up after the loop
        }

        if (waitResult == WAIT_TIMEOUT) {
            // No completion yet, just loop again and re-check isRunning.
            continue;
        }

        if (waitResult != WAIT_OBJECT_0) {
            qCritical() << "[WindowsWorker] WaitForSingleObject failed:" << GetLastError();
            break;
        }

        // I/O completed – retrieve the result.
        if (!GetOverlappedResult(hDir, &ovl, &dwBytes, FALSE)) {
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED) {
                // The operation was cancelled (e.g., during shutdown).
                break;
            }
            qCritical() << "[WindowsWorker] GetOverlappedResult failed:" << err;
            break;
        }

        // Process the received notifications.
        if (dwBytes > 0) {
            FILE_NOTIFY_INFORMATION *fni =
                reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
            do {
                if (fni->Action != 0) {
                    const int len = fni->FileNameLength / sizeof(WCHAR);
                    const QString name = QString::fromWCharArray(
                        reinterpret_cast<const wchar_t*>(fni->FileName), len);
                    emit notifyEvent(fni->Action, name);
                }
                if (fni->NextEntryOffset == 0)
                    break;
                fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<PCHAR>(fni) + fni->NextEntryOffset);
            } while (true);
        } else {
            // dwBytes == 0 means the buffer overflowed – some notifications were lost.
            qWarning() << "[WindowsWorker] Buffer overflow, notifications lost";
        }

        // Re-issue the change notification for the next batch of events.
        ::ResetEvent(ovl.hEvent);
        if (!ReadDirectoryChangesW(hDir,
                                   buffer.data(),
                                   static_cast<DWORD>(buffer.size()),
                                   FALSE,
                                   FILE_NOTIFY_CHANGE_FILE_NAME |
                                       FILE_NOTIFY_CHANGE_DIR_NAME |
                                       FILE_NOTIFY_CHANGE_LAST_WRITE,
                                   &dwBytes,
                                   &ovl,
                                   nullptr)) {
            DWORD err = GetLastError();
            if (err != ERROR_IO_PENDING) {
                qCritical() << "[WindowsWorker] ReadDirectoryChangesW re-issue failed:" << err;
                break;
            }
        }
    }

    // Clean up the event handle. At this point the overlapped operation is definitely finished.
    CloseHandle(ovl.hEvent);
    isRunning = false;
}
