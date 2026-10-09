#pragma once

#include <QString>
#include <QStringList>
#include <memory>

#include "gui/quick/render/rendererrorchannel.h"

// Render-thread error log of one ImageRenderer, shared by its RenderDevice
// and ImageLayers: posts each message to the item's RenderErrorChannel, and
// keeps messages reported before the first synchronize() supplied the
// channel. A failure that repeats every frame is reported once.
//
// Render thread only.
class RenderErrorReporter {
public:
  void report(const QString &message);
  // Called from synchronize(); delivers the messages kept so far.
  void setChannel(std::shared_ptr<RenderErrorChannel> channel);
  // The failing step succeeded: the same message is reported again when it
  // fails next time.
  void clearRepeatGuard();

private:
  std::shared_ptr<RenderErrorChannel> mChannel;
  QStringList mUnsent;
  QString mLastMessage;
};
