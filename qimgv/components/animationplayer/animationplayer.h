#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

class QMovie;

// Plays an animated image (GIF, WebP, APNG, animated SVG, ...) frame by
// frame through QMovie, without any UI: every displayed frame is published
// as an immutable image through frameReady().
//
// The schedule is the widget viewer's (ImageViewerV2): each frame stays for
// QMovie::nextFrameDelay() milliseconds; after the last frame playback
// either restarts at frame 0 (loop) or stops and reports playbackFinished().
// Stepping with nextFrame() / prevFrame() wraps around and does not change
// the playing state.
//
// GUI thread only.
class AnimationPlayer : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged FINAL)
  Q_PROPERTY(bool loop READ loops WRITE setLoop NOTIFY loopChanged FINAL)
  Q_PROPERTY(bool finished READ isFinished NOTIFY finishedChanged FINAL)
  Q_PROPERTY(int frameIndex READ frameIndex NOTIFY frameChanged FINAL)
  Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged FINAL)

public:
  // Returned by frameIndex() and scheduledDelay() when there is none.
  static constexpr int kNoFrame = -1;
  static constexpr int kNoDelay = -1;

  explicit AnimationPlayer(QObject *parent = nullptr);
  ~AnimationPlayer() override;

  // Opens the animation at path, decoded as format (QImageReader format
  // name; empty to detect it), publishes its first frame and starts playing
  // when it has more than one frame. Returns false and reports
  // playbackError() when the file cannot be decoded; the player is then
  // closed.
  bool open(const QString &path, const QByteArray &format);
  // Stops playback and drops the animation.
  void close();
  [[nodiscard]] bool isOpen() const;

  // Starts or resumes playback from the current frame; animations with a
  // single frame never play.
  void play();
  void pause();
  void togglePlaying();
  [[nodiscard]] bool isPlaying() const;

  // Loop playback (Settings::loopSlideshow() in the widget UI); turning it
  // on after playback finished resumes playing.
  void setLoop(bool loop);
  [[nodiscard]] bool loops() const;

  // Playback stopped at the last frame because looping is off; cleared by
  // play(), stepping and a new animation.
  [[nodiscard]] bool isFinished() const;

  // Shows frame index (0 .. frameCount() - 1); false when it is out of
  // range or not open.
  bool showFrame(int index);
  void nextFrame();
  void prevFrame();

  [[nodiscard]] int frameIndex() const;
  [[nodiscard]] int frameCount() const;
  // Pixel size of the frames; invalid when not open.
  [[nodiscard]] QSize frameSize() const;
  [[nodiscard]] std::shared_ptr<const QImage> currentFrame() const;
  // Delay in ms after which the next frame is shown while playing;
  // kNoDelay when not playing.
  [[nodiscard]] int scheduledDelay() const;

signals:
  // The displayed frame changed; frame is never null.
  void frameReady(std::shared_ptr<const QImage> frame, int index);
  void frameChanged();
  void frameCountChanged();
  void playingChanged();
  void loopChanged();
  void finishedChanged();
  // Playback stopped after the last frame (looping off).
  void playbackFinished();
  // The animation could not be opened or a frame could not be decoded.
  void playbackError(const QString &message);

private:
  void onTimer();
  // Publishes the movie's current frame and, while playing, schedules the
  // next one.
  void publishCurrentFrame();
  void setPlaying(bool playing);
  void setFinished(bool finished);
  void scheduleNextFrame();
  void reportDecodeError();

  std::unique_ptr<QMovie> mMovie;
  QTimer mTimer;
  std::shared_ptr<const QImage> mFrame;
  bool mPlaying = false;
  bool mLoop = false;
  bool mFinished = false;
};
