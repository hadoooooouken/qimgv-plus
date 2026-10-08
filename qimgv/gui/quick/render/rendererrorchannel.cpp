#include "rendererrorchannel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMutexLocker>
#include <utility>

RenderErrorChannel::RenderErrorChannel(QObject *receiver, Sink sink)
    : mReceiver(receiver), mSink(std::move(sink)) {}

void RenderErrorChannel::post(const QString &message) {
  {
    const QMutexLocker locker(&mMutex);
    mPending.append(message);
  }
  // The application object lives on the GUI thread for the whole run, unlike
  // the receiver, which may be destroyed concurrently with this call.
  QCoreApplication *application = QCoreApplication::instance();
  if (!application) {
    qCritical().noquote() << "RenderErrorChannel: no application to deliver"
                          << message;
    return;
  }
  const std::weak_ptr<RenderErrorChannel> channel = weak_from_this();
  QMetaObject::invokeMethod(
      application,
      [channel]() {
        if (const auto alive = channel.lock())
          alive->deliver();
      },
      Qt::QueuedConnection);
}

void RenderErrorChannel::deliver() {
  QStringList messages;
  {
    const QMutexLocker locker(&mMutex);
    messages = std::exchange(mPending, {});
  }
  if (!mReceiver || !mSink)
    return;
  for (const QString &message : std::as_const(messages))
    mSink(message);
}
