#include "framepresentationtracker.h"

#include <QQuickWindow>

FramePresentationTracker::FramePresentationTracker(QObject *parent) : QObject(parent) {}

void FramePresentationTracker::setWindow(QQuickWindow *window) {
    if (mWindow == window)
        return;
    disconnectWindow();
    mWindow = window;
    if (!window)
        return;
    // Direct connections: both signals are emitted on the render thread. The
    // tracker only touches atomics there and posts back to the GUI thread.
    mSyncConnection = connect(window, &QQuickWindow::beforeSynchronizing, this,
                              [this]() { frameSynchronized(); }, Qt::DirectConnection);
    mFrameEndConnection = connect(window, &QQuickWindow::afterFrameEnd, this,
                                  [this]() { frameEnded(); }, Qt::DirectConnection);
}

QQuickWindow *FramePresentationTracker::window() const {
    return mWindow;
}

void FramePresentationTracker::requestPresentation() {
    mPendingRequest = mLatestRequest.load(std::memory_order_relaxed) + 1;
    mLatestRequest.store(mPendingRequest, std::memory_order_release);
}

void FramePresentationTracker::cancel() {
    mPendingRequest = kNoRequest;
}

bool FramePresentationTracker::isPending() const {
    return mPendingRequest != kNoRequest;
}

void FramePresentationTracker::frameSynchronized() {
    mSynchronizedRequest.store(mLatestRequest.load(std::memory_order_acquire),
                               std::memory_order_relaxed);
}

void FramePresentationTracker::frameEnded() {
    const quint64 request = mSynchronizedRequest.load(std::memory_order_relaxed);
    // Every request is reported once, by the first frame that carries it.
    if (request == mReportedRequest)
        return;
    mReportedRequest = request;
    QMetaObject::invokeMethod(this, [this, request]() { onFrameEnded(request); },
                              Qt::QueuedConnection);
}

void FramePresentationTracker::onFrameEnded(quint64 request) {
    if (mPendingRequest == kNoRequest || request < mPendingRequest)
        return;
    mPendingRequest = kNoRequest;
    emit presented();
}

void FramePresentationTracker::disconnectWindow() {
    disconnect(mSyncConnection);
    disconnect(mFrameEndConnection);
    mWindow = nullptr;
}
