#include "rendererrorreporter.h"

#include <utility>

void RenderErrorReporter::report(const QString &message) {
  if (message == mLastMessage)
    return;
  mLastMessage = message;
  if (mChannel)
    mChannel->post(message);
  else
    mUnsent.append(message);
}

void RenderErrorReporter::setChannel(
    std::shared_ptr<RenderErrorChannel> channel) {
  mChannel = std::move(channel);
  if (!mChannel)
    return;
  for (const QString &message : std::exchange(mUnsent, {}))
    mChannel->post(message);
}

void RenderErrorReporter::clearRepeatGuard() { mLastMessage.clear(); }
