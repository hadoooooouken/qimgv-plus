#pragma once

#include <QMutex>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

// Hands error messages from the render thread to a GUI-thread receiver.
//
// QQuickRhiItemRenderer::update() only schedules another render(), not
// another synchronize(), so a renderer cannot rely on the next sync to hand
// its errors over. post() queues the message and schedules delivery on the
// application thread with a queued QMetaObject::invokeMethod(); delivery
// drops messages whose receiver was destroyed in the meantime.
//
// Created and owned (std::shared_ptr) by the receiver on the GUI thread; the
// renderer keeps a copy taken in synchronize().
class RenderErrorChannel
    : public std::enable_shared_from_this<RenderErrorChannel> {
public:
  using Sink = std::function<void(const QString &)>;

  // receiver bounds the lifetime of delivery; sink is called on the GUI
  // thread for every message while receiver is alive.
  RenderErrorChannel(QObject *receiver, Sink sink);
  RenderErrorChannel(const RenderErrorChannel &) = delete;
  RenderErrorChannel &operator=(const RenderErrorChannel &) = delete;

  // Any thread.
  void post(const QString &message);

private:
  // GUI thread.
  void deliver();

  // Only read and written on the GUI thread.
  QPointer<QObject> mReceiver;
  Sink mSink;

  QMutex mMutex;
  QStringList mPending;
};
