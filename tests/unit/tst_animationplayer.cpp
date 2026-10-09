#include <QDir>
#include <QFile>
#include <QMovie>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <vector>

#include "components/animationplayer/animationplayer.h"
#include "testanimations.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;
using namespace TestAnimations;

const QByteArray kGifFormat = "gif"_ba;
// Generous upper bound for one frame delay on a busy machine.
constexpr int kFrameTimeoutMs = 2000;
// Longer than every frame delay: nothing may happen within it.
constexpr int kQuietPeriodMs = 200;

QString writeFile(const QTemporaryDir &dir, const QString &name,
                  const unsigned char *data, qsizetype size) {
  const QString path = dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(reinterpret_cast<const char *>(data), size) != size)
    return {};
  return path;
}

// The delay ImageViewerV2 schedules after each frame: QMovie's own.
std::vector<int> qmovieDelays(const QString &path) {
  QMovie movie(path, kGifFormat);
  std::vector<int> delays;
  movie.jumpToFrame(0);
  for (int frame = 0; frame < movie.frameCount(); ++frame) {
    delays.push_back(movie.nextFrameDelay());
    movie.jumpToNextFrame();
  }
  return delays;
}
} // namespace

class AnimationPlayerTests : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    QVERIFY(mDir.isValid());
    mThreeFrames = writeFile(mDir, u"three.gif"_s, kThreeFrameGif,
                             sizeof(kThreeFrameGif));
    mSingleFrame = writeFile(mDir, u"single.gif"_s, kSingleFrameGif,
                             sizeof(kSingleFrameGif));
    QVERIFY(!mThreeFrames.isEmpty());
    QVERIFY(!mSingleFrame.isEmpty());
  }

  void openPublishesFirstFrameAndPlays() {
    AnimationPlayer player;
    QSignalSpy frames(&player, &AnimationPlayer::frameReady);
    QVERIFY(player.open(mThreeFrames, kGifFormat));
    QCOMPARE(frames.count(), 1);
    QCOMPARE(frames.first().at(1).toInt(), 0);
    QCOMPARE(player.frameCount(), kThreeFrameCount);
    QCOMPARE(player.frameIndex(), 0);
    QCOMPARE(player.frameSize(), QSize(4, 4));
    QVERIFY(player.isPlaying());
    QCOMPARE(player.currentFrame()->pixelColor(0, 0), QColor(Qt::red));
  }

  // Every frame stays for QMovie::nextFrameDelay(), as in the widget viewer.
  void scheduleMatchesMovieDelays() {
    const std::vector<int> expected = qmovieDelays(mThreeFrames);
    QCOMPARE(expected, kThreeFrameDelays);
    AnimationPlayer player;
    player.setLoop(true);
    std::vector<int> delays;
    std::vector<int> order;
    connect(&player, &AnimationPlayer::frameReady, this,
            [&](const std::shared_ptr<const QImage> &, int index) {
              order.push_back(index);
              delays.push_back(player.scheduledDelay());
            });
    QVERIFY(player.open(mThreeFrames, kGifFormat));
    // Frames 0, 1, 2 and the loop back to 0.
    QTRY_VERIFY_WITH_TIMEOUT(order.size() >= expected.size() + 1,
                             kFrameTimeoutMs * kThreeFrameCount);
    for (std::size_t i = 0; i <= expected.size(); ++i) {
      QCOMPARE(order[i], int(i % expected.size()));
      QCOMPARE(delays[i], expected[i % expected.size()]);
    }
  }

  void withoutLoopStopsAtLastFrame() {
    AnimationPlayer player;
    QSignalSpy finished(&player, &AnimationPlayer::playbackFinished);
    QVERIFY(player.open(mThreeFrames, kGifFormat));
    QVERIFY(finished.wait(kFrameTimeoutMs * kThreeFrameCount));
    QVERIFY(!player.isPlaying());
    QVERIFY(player.isFinished());
    QCOMPARE(player.frameIndex(), kLastFrame);
    QCOMPARE(player.scheduledDelay(), AnimationPlayer::kNoDelay);

    // Turning the loop on resumes playback, like ImageViewerV2.
    QSignalSpy frames(&player, &AnimationPlayer::frameReady);
    player.setLoop(true);
    QVERIFY(player.isPlaying());
    QVERIFY(!player.isFinished());
    QVERIFY(frames.wait(kFrameTimeoutMs));
    QCOMPARE(frames.first().at(1).toInt(), 0);
  }

  void steppingWrapsAndKeepsPause() {
    AnimationPlayer player;
    QVERIFY(player.open(mThreeFrames, kGifFormat));
    player.pause();
    QVERIFY(!player.isPlaying());
    player.prevFrame();
    QCOMPARE(player.frameIndex(), kLastFrame);
    QCOMPARE(player.currentFrame()->pixelColor(0, 0), QColor(Qt::blue));
    player.nextFrame();
    QCOMPARE(player.frameIndex(), 0);
    player.nextFrame();
    QCOMPARE(player.frameIndex(), 1);
    QVERIFY(!player.isPlaying());
    QVERIFY(player.showFrame(0));
    QCOMPARE(player.currentFrame()->pixelColor(0, 0), QColor(Qt::red));
    QVERIFY(!player.showFrame(kThreeFrameCount));
    QSignalSpy frames(&player, &AnimationPlayer::frameReady);
    QTest::qWait(kQuietPeriodMs);
    QCOMPARE(frames.count(), 0);
  }

  void singleFrameDoesNotPlay() {
    AnimationPlayer player;
    QVERIFY(player.open(mSingleFrame, kGifFormat));
    QCOMPARE(player.frameIndex(), 0);
    QVERIFY(!player.isPlaying());
    player.play();
    QVERIFY(!player.isPlaying());
  }

  void unreadableFileReportsError() {
    AnimationPlayer player;
    QSignalSpy errors(&player, &AnimationPlayer::playbackError);
    QVERIFY(!player.open(mDir.filePath(u"missing.gif"_s), kGifFormat));
    QCOMPARE(errors.count(), 1);
    QVERIFY(!player.isOpen());
    QCOMPARE(player.frameIndex(), AnimationPlayer::kNoFrame);
  }

  void closeStopsPlayback() {
    AnimationPlayer player;
    QVERIFY(player.open(mThreeFrames, kGifFormat));
    player.close();
    QVERIFY(!player.isPlaying());
    QVERIFY(!player.currentFrame());
    QCOMPARE(player.frameCount(), 0);
    QSignalSpy frames(&player, &AnimationPlayer::frameReady);
    QTest::qWait(kQuietPeriodMs);
    QCOMPARE(frames.count(), 0);
  }

private:
  QTemporaryDir mDir;
  QString mThreeFrames;
  QString mSingleFrame;
};

int runAnimationPlayerTests(int argc, char **argv) {
  AnimationPlayerTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_animationplayer.moc"
