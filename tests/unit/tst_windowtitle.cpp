#include <QCoreApplication>
#include <QTest>

#include "components/shellinfo/windowtitle.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

constexpr qint64 kFileSize = 3 * 1024 * 1024;
constexpr int kFileSizePrecision = 1;

WindowTitleState documentState() {
  WindowTitleState state;
  state.info.index = 2;
  state.info.fileCount = 10;
  state.info.fileName = u"photo.png"_s;
  state.info.filePath = u"C:/images/photo.png"_s;
  state.info.imageSize = QSize(1920, 1080);
  state.info.fileSize = kFileSize;
  state.info.format = u"png"_s;
  state.info.colorProfile = u"sRGB"_s;
  state.viewMode = MODE_DOCUMENT;
  state.scalePercent = 50;
  return state;
}
} // namespace

class WindowTitleTests : public QObject {
  Q_OBJECT

private slots:
  void folderViewHasAFixedTitle() {
    WindowTitleState state = documentState();
    state.viewMode = MODE_FOLDERVIEW;
    QCOMPARE(windowTitleFor(state, QLocale::c()), u"Folder view"_s);
  }

  void withoutAFileTheApplicationNameIsShown() {
    WindowTitleState state;
    QCOMPARE(windowTitleFor(state, QLocale::c()), QCoreApplication::applicationName());
  }

  void showsTheFileNameAndZoom() {
    QCOMPARE(windowTitleFor(documentState(), QLocale::c()), u"photo.png [50%]"_s);
  }

  void extendedInfoAddsPositionAndDetails() {
    WindowTitleState state = documentState();
    state.extendedInfo = true;
    const QString size = QLocale::c().formattedDataSize(kFileSize, kFileSizePrecision);
    QCOMPARE(windowTitleFor(state, QLocale::c()),
             u"[ 3/10 ]  photo.png [50%] - 1920 x 1080 (16:9) - sRGB - PNG - "_s + size);
  }

  void extendedInfoSkipsMissingDetails() {
    WindowTitleState state = documentState();
    state.extendedInfo = true;
    state.info.fileCount = 0;
    state.info.imageSize = QSize();
    state.info.colorProfile.clear();
    state.info.fileSize = 0;
    QCOMPARE(windowTitleFor(state, QLocale::c()), u"  photo.png [50%] - PNG"_s);
  }

  void statesAndEditsAreMarked() {
    WindowTitleState state = documentState();
    state.info.slideshow = true;
    state.info.shuffle = true;
    state.zoomLocked = true;
    state.viewLocked = true;
    state.info.edited = true;
    QCOMPARE(windowTitleFor(state, QLocale::c()),
             u"* photo.png [50%] - [slideshow] [shuffle] [zoom lock] [view lock]"_s);
  }
};

int runWindowTitleTests(int argc, char **argv) {
  WindowTitleTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_windowtitle.moc"
