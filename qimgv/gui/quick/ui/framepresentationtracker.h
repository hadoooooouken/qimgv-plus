#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QtGlobal>

#include <atomic>

class QQuickWindow;

// Reports when the scene state of a request has reached the screen: after
// requestPresentation(), presented() is emitted once the first frame whose
// scene graph synchronization started after the request has ended
// (QQuickWindow::afterFrameEnd). The Quick replacement of the widget viewer's
// QOpenGLWidget::frameSwapped wait.
//
// Requests are numbered. The render thread records the newest request number
// while the GUI thread is blocked in the synchronization step
// (beforeSynchronizing), and posts that number back when the frame ends, so a
// frame that was already being rendered when the request was made never
// completes it. A newer request supersedes an older pending one.
//
// GUI thread only, except frameSynchronized() and frameEnded(), which the
// window's render thread calls through the connections made by setWindow().
class FramePresentationTracker final : public QObject {
    Q_OBJECT
public:
    explicit FramePresentationTracker(QObject *parent = nullptr);

    // Follows the frames of window (nullptr detaches).
    void setWindow(QQuickWindow *window);
    [[nodiscard]] QQuickWindow *window() const;

    // Asks for presented() after the current scene state is on screen. The
    // caller schedules the frame (QQuickItem::update()).
    void requestPresentation();
    // Drops the pending request; no presented() follows for it.
    void cancel();
    [[nodiscard]] bool isPending() const;

    // Render thread, while the GUI thread is blocked: the frame being
    // synchronized carries the newest request.
    void frameSynchronized();
    // Render thread: the synchronized frame has been submitted.
    void frameEnded();

signals:
    void presented();

private:
    void onFrameEnded(quint64 request);
    void disconnectWindow();

    static constexpr quint64 kNoRequest = 0;

    QPointer<QQuickWindow> mWindow;
    QMetaObject::Connection mSyncConnection;
    QMetaObject::Connection mFrameEndConnection;
    // Number of the newest request; written on the GUI thread, read on the
    // render thread during synchronization.
    std::atomic<quint64> mLatestRequest{kNoRequest};
    // Request carried by the frame being rendered; render thread only.
    std::atomic<quint64> mSynchronizedRequest{kNoRequest};
    // Newest request already posted back; render thread only.
    quint64 mReportedRequest = kNoRequest;
    // Request waiting for presented(); GUI thread only.
    quint64 mPendingRequest = kNoRequest;
};
