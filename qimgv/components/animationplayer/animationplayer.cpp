#include "animationplayer.h"

#include <QMovie>

namespace {
using namespace Qt::StringLiterals;

// QMovie numbers its frames from 0; animations with fewer frames than this
// are shown, not played.
constexpr int kFirstFrame = 0;
constexpr int kMinimumPlayableFrames = 2;
} // namespace

//------------------------------------------------------------------------------
AnimationPlayer::AnimationPlayer(QObject *parent) : QObject(parent) {
  mTimer.setSingleShot(true);
  // Frame delays are short; the default coarse timer would drift by up to 5%.
  mTimer.setTimerType(Qt::PreciseTimer);
  connect(&mTimer, &QTimer::timeout, this, &AnimationPlayer::onTimer);
}

AnimationPlayer::~AnimationPlayer() = default;

//------------------------------------------------------------------------------
bool AnimationPlayer::open(const QString &path, const QByteArray &format) {
  close();
  auto movie = std::make_unique<QMovie>(path, format);
  if (!movie->isValid()) {
    emit playbackError(u"Cannot open the animation %1: %2"_s.arg(
        path, movie->lastErrorString()));
    return false;
  }
  // Frames are advanced by the player's own timer; the QMovie is never
  // started.
  mMovie = std::move(movie);
  if (!mMovie->jumpToFrame(kFirstFrame) || mMovie->currentImage().isNull()) {
    reportDecodeError();
    close();
    return false;
  }
  emit frameCountChanged();
  // Playing before the first frame is published, so that its delay is
  // already scheduled when frameReady() arrives.
  if (mMovie->frameCount() >= kMinimumPlayableFrames)
    setPlaying(true);
  publishCurrentFrame();
  return true;
}

void AnimationPlayer::close() {
  mTimer.stop();
  const bool wasOpen = mMovie != nullptr;
  mMovie.reset();
  mFrame.reset();
  setPlaying(false);
  setFinished(false);
  if (wasOpen) {
    emit frameCountChanged();
    emit frameChanged();
  }
}

bool AnimationPlayer::isOpen() const { return mMovie != nullptr; }

//------------------------------------------------------------------------------
void AnimationPlayer::play() {
  if (!mMovie || mMovie->frameCount() < kMinimumPlayableFrames)
    return;
  setFinished(false);
  setPlaying(true);
  scheduleNextFrame();
}

void AnimationPlayer::pause() {
  mTimer.stop();
  setPlaying(false);
}

void AnimationPlayer::togglePlaying() {
  if (mPlaying)
    pause();
  else
    play();
}

bool AnimationPlayer::isPlaying() const { return mPlaying; }

void AnimationPlayer::setLoop(bool loop) {
  if (mLoop == loop)
    return;
  mLoop = loop;
  emit loopChanged();
  // ImageViewerV2::setLoopPlayback(): enabling the loop restarts playback.
  if (mLoop)
    play();
}

bool AnimationPlayer::loops() const { return mLoop; }

bool AnimationPlayer::isFinished() const { return mFinished; }

//------------------------------------------------------------------------------
bool AnimationPlayer::showFrame(int index) {
  if (!mMovie || index < kFirstFrame || index >= mMovie->frameCount())
    return false;
  setFinished(false);
  if (mMovie->currentFrameNumber() == index)
    return true;
  // QMovie only decodes forwards; earlier frames restart from the first one.
  if (index < mMovie->currentFrameNumber() &&
      !mMovie->jumpToFrame(kFirstFrame)) {
    reportDecodeError();
    return false;
  }
  while (mMovie->currentFrameNumber() != index) {
    if (!mMovie->jumpToNextFrame()) {
      reportDecodeError();
      break;
    }
  }
  publishCurrentFrame();
  return true;
}

void AnimationPlayer::nextFrame() {
  if (!mMovie)
    return;
  const int last = mMovie->frameCount() - 1;
  showFrame(mMovie->currentFrameNumber() >= last
                ? kFirstFrame
                : mMovie->currentFrameNumber() + 1);
}

void AnimationPlayer::prevFrame() {
  if (!mMovie)
    return;
  showFrame(mMovie->currentFrameNumber() <= kFirstFrame
                ? mMovie->frameCount() - 1
                : mMovie->currentFrameNumber() - 1);
}

int AnimationPlayer::frameIndex() const {
  return mMovie && mFrame ? mMovie->currentFrameNumber() : kNoFrame;
}

int AnimationPlayer::frameCount() const {
  return mMovie ? mMovie->frameCount() : 0;
}

QSize AnimationPlayer::frameSize() const {
  return mFrame ? mFrame->size() : QSize();
}

std::shared_ptr<const QImage> AnimationPlayer::currentFrame() const {
  return mFrame;
}

int AnimationPlayer::scheduledDelay() const {
  return mTimer.isActive() ? mTimer.interval() : kNoDelay;
}

//------------------------------------------------------------------------------
void AnimationPlayer::onTimer() {
  if (!mMovie || !mPlaying)
    return;
  if (mMovie->currentFrameNumber() >= mMovie->frameCount() - 1) {
    if (!mLoop) {
      setPlaying(false);
      setFinished(true);
      emit playbackFinished();
      return;
    }
    if (!mMovie->jumpToFrame(kFirstFrame)) {
      reportDecodeError();
      pause();
      return;
    }
  } else if (!mMovie->jumpToNextFrame()) {
    reportDecodeError();
    pause();
    return;
  }
  publishCurrentFrame();
}

void AnimationPlayer::publishCurrentFrame() {
  QImage frame = mMovie->currentImage();
  if (frame.isNull()) {
    reportDecodeError();
    pause();
    return;
  }
  mFrame = std::make_shared<const QImage>(std::move(frame));
  // Scheduled first: receivers of frameReady() see the frame's delay.
  if (mPlaying)
    scheduleNextFrame();
  emit frameReady(mFrame, mMovie->currentFrameNumber());
  emit frameChanged();
}

void AnimationPlayer::scheduleNextFrame() {
  mTimer.start(mMovie->nextFrameDelay());
}

void AnimationPlayer::setPlaying(bool playing) {
  if (mPlaying == playing)
    return;
  mPlaying = playing;
  if (!mPlaying)
    mTimer.stop();
  emit playingChanged();
}

void AnimationPlayer::setFinished(bool finished) {
  if (mFinished == finished)
    return;
  mFinished = finished;
  emit finishedChanged();
}

void AnimationPlayer::reportDecodeError() {
  emit playbackError(u"Cannot decode frame %1 of the animation: %2"_s.arg(
      mMovie->currentFrameNumber() + 1).arg(mMovie->lastErrorString()));
}
