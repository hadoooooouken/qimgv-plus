#include <QSignalSpy>
#include <QTest>

#include "gui/quick/ui/crop/cropcontroller.h"
#include "gui/quick/ui/crop/cropselection.h"
#include "testsuites.h"

namespace {
constexpr QSize kSquareImage(100, 100);
constexpr QSize kWideImage(200, 100);
constexpr QSize kLargeImage(400, 200);
constexpr QSize kScreen(1920, 1080);
// kLargeImage drawn at half size, offset in the viewport.
constexpr QRectF kHalfSizeArea(100, 50, 200, 100);
constexpr qreal kHighDpr = 2.0;
constexpr double kTolerance = 0.0001;

UiSettingsSnapshot settingsWith(SettingsEnums::CropAction action) {
  UiSettingsSnapshot settings;
  settings.overlays.defaultCropAction = action;
  return settings;
}

UiSettingsSnapshot defaultSettings() {
  return settingsWith(SettingsEnums::CropAction::Crop);
}

// An open crop mode on kLargeImage drawn into kHalfSizeArea.
void openOnLargeImage(CropController &crop) {
  crop.setImageSize(kLargeImage);
  crop.setImageArea(kHalfSizeArea);
  crop.setScreenSize(kScreen);
  crop.toggle();
}

bool fuzzyEqual(double a, double b) {
  return std::abs(a - b) < kTolerance;
}
} // namespace

class CropTests : public QObject {
  Q_OBJECT

private slots:
  //--- CropSelection ---------------------------------------------------------

  void unlockedSelectionCoversTheImage() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    QCOMPARE(selection.rect(), QRect(QPoint(0, 0), kSquareImage));
    QVERIFY(!selection.isAspectLocked());
  }

  void lockedRatioFitsTheLargestCentredSelection() {
    CropSelection selection;
    selection.setImageSize(QSize(1000, 1000));
    QVERIFY(selection.setAspectRatio(QPointF(16, 9)));
    QVERIFY(selection.isAspectLocked());
    // 1000 / (16 / 9) = 562.5, rounded up; centred vertically.
    QCOMPARE(selection.rect(), QRect(0, 218, 1000, 563));

    QVERIFY(selection.setAspectRatio(QPointF(1, 1)));
    selection.setImageSize(kWideImage);
    QCOMPARE(selection.rect(), QRect(50, 0, 100, 100));
  }

  void ratioWithZeroComponentIsIgnored() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    QVERIFY(!selection.setAspectRatio(QPointF(0, 1)));
    QVERIFY(!selection.isAspectLocked());
    QCOMPARE(selection.aspectRatio(), CropSelection::kDefaultAspectRatio);
  }

  void placedSelectionMovesIntoTheImage() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    QVERIFY(selection.place(QRect(80, 80, 50, 50)));
    QCOMPARE(selection.rect(), QRect(50, 50, 50, 50));
    QVERIFY(!selection.place(QRect(10, 10, 20, 20)));
    QCOMPARE(selection.rect(), QRect(10, 10, 20, 20));
    QVERIFY(selection.place(QRect(-10, 20, 300, 30)));
    QCOMPARE(selection.rect(), QRect(0, 20, 100, 30));
    // Empty requests change nothing.
    QVERIFY(!selection.place(QRect(5, 5, 0, 10)));
    QCOMPARE(selection.rect(), QRect(0, 20, 100, 30));
  }

  void freeResizeMovesTheDraggedEdges() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    selection.place(QRect(10, 10, 50, 50));
    selection.beginDrag(CropHandle::BottomRight);
    QCOMPARE(selection.dragBy(QPointF(10, 5)), CropHandle::BottomRight);
    QCOMPARE(selection.rect(), QRect(10, 10, 60, 55));
    // The travel counts from the start of the drag.
    QCOMPARE(selection.dragBy(QPointF(20, 0)), CropHandle::BottomRight);
    QCOMPARE(selection.rect(), QRect(10, 10, 70, 50));
    // Bounded by the image.
    selection.dragBy(QPointF(500, 500));
    QCOMPARE(selection.rect(), QRect(10, 10, 90, 90));
    selection.endDrag();
    QVERIFY(!selection.isDragging());
  }

  void freeResizeFlipsAcrossTheAnchor() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    selection.place(QRect(10, 10, 50, 50));
    selection.beginDrag(CropHandle::Left);
    QCOMPARE(selection.dragBy(QPointF(70, 0)), CropHandle::Right);
    QCOMPARE(selection.rect(), QRect(QPoint(59, 10), QPoint(80, 59)));

    selection.endDrag();
    selection.place(QRect(10, 10, 50, 50));
    selection.beginDrag(CropHandle::TopLeft);
    QCOMPARE(selection.dragBy(QPointF(0, 70)), CropHandle::BottomLeft);
    QCOMPARE(selection.rect(), QRect(QPoint(10, 59), QPoint(59, 80)));
  }

  void lockedCornerKeepsTheRatioWithinTheImage() {
    CropSelection selection;
    selection.setImageSize(kWideImage);
    QVERIFY(selection.setAspectRatio(QPointF(2, 1)));
    QCOMPARE(selection.rect(), QRect(QPoint(0, 0), kWideImage));
    selection.beginDrag(CropHandle::BottomRight);
    // The corner moves to (99, 99): width 99, height 49.5 rounded.
    QCOMPARE(selection.dragBy(QPointF(-100, 0)), CropHandle::BottomRight);
    QCOMPARE(selection.rect(), QRect(0, 0, 99, 50));
    // Across the anchor's left side there is one pixel of room.
    QCOMPARE(selection.dragBy(QPointF(-300, 0)), CropHandle::BottomLeft);
    QCOMPARE(selection.rect(), QRect(0, 0, 1, 1));
  }

  void lockedEdgeResizesAroundTheCentreLine() {
    CropSelection selection;
    selection.setImageSize(kWideImage);
    QVERIFY(selection.setAspectRatio(QPointF(1, 1)));
    QCOMPARE(selection.rect(), QRect(50, 0, 100, 100));
    selection.beginDrag(CropHandle::Right);
    QCOMPARE(selection.dragBy(QPointF(-50, 0)), CropHandle::Right);
    QCOMPARE(selection.rect(), QRect(50, 25, 50, 50));
    // Past the left edge it grows to the left of the anchor.
    QCOMPARE(selection.dragBy(QPointF(-150, 0)), CropHandle::Left);
    QCOMPARE(selection.rect(), QRect(1, 25, 50, 50));
  }

  void movingKeepsTheSelectionInsideTheImage() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    selection.place(QRect(10, 10, 50, 50));
    selection.beginDrag(CropHandle::Move);
    QCOMPARE(selection.dragBy(QPointF(100, -100)), CropHandle::Move);
    QCOMPARE(selection.rect(), QRect(50, 0, 50, 50));
  }

  void dragsAccumulateFractionalTravel() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    selection.place(QRect(10, 10, 50, 50));
    selection.beginDrag(CropHandle::Move);
    selection.dragBy(QPointF(0.4, 0.4));
    QCOMPARE(selection.rect(), QRect(10, 10, 50, 50));
    selection.dragBy(QPointF(0.8, 0.8));
    QCOMPARE(selection.rect(), QRect(11, 11, 50, 50));
  }

  void newSelectionTakesItsDirectionFromTheTravel() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    selection.beginNewSelection(QPoint(10, 10));
    QCOMPARE(selection.rect(), QRect(10, 10, 1, 1));
    // Travel along one axis gives no direction yet.
    QCOMPARE(selection.dragBy(QPointF(5, 0)), CropHandle::None);
    QCOMPARE(selection.rect(), QRect(10, 10, 1, 1));
    QCOMPARE(selection.dragBy(QPointF(20, 10)), CropHandle::BottomRight);
    QCOMPARE(selection.rect(), QRect(10, 10, 21, 11));
    selection.endDrag();
    QVERIFY(selection.hasSelection());

    selection.beginNewSelection(QPoint(40, 40));
    QCOMPARE(selection.dragBy(QPointF(-10, -5)), CropHandle::TopLeft);
    QCOMPARE(selection.rect(), QRect(QPoint(30, 35), QPoint(40, 40)));
  }

  void newSelectionWithoutDirectionIsDropped() {
    CropSelection selection;
    selection.setImageSize(kSquareImage);
    selection.beginNewSelection(QPoint(10, 10));
    selection.dragBy(QPointF(0, 7));
    selection.endDrag();
    QVERIFY(!selection.hasSelection());
  }

  //--- CropController: mode ----------------------------------------------------

  void opensOnlyWithAnImageInTheDocumentView() {
    CropController crop(defaultSettings());
    QSignalSpy active(&crop, &CropController::activeChanged);
    crop.toggle();
    QVERIFY(!crop.isActive());

    crop.setImageSize(kLargeImage);
    crop.setFolderViewActive(true);
    crop.toggle();
    QVERIFY(!crop.isActive());

    crop.setFolderViewActive(false);
    crop.toggle();
    QVERIFY(crop.isActive());
    QCOMPARE(active.count(), 1);
    QCOMPARE(crop.selection(), QRect(QPoint(0, 0), kLargeImage));
    QCOMPARE(crop.aspectPreset(), CropController::AspectPreset::Free);
    QVERIFY(fuzzyEqual(crop.aspectWidth(), 2.0));
    QVERIFY(fuzzyEqual(crop.aspectHeight(), 1.0));

    crop.toggle();
    QVERIFY(!crop.isActive());
    QVERIFY(!crop.hasSelection());
  }

  void folderViewAndImageLossClose() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.setFolderViewActive(true);
    QVERIFY(!crop.isActive());

    crop.setFolderViewActive(false);
    crop.toggle();
    QVERIFY(crop.isActive());
    crop.setImageSize(QSize());
    QVERIFY(!crop.isActive());
  }

  void newImageResetsToTheFreeRatio() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.selectAspectPreset(CropController::AspectPreset::Square);
    QCOMPARE(crop.selection(), QRect(100, 0, 200, 200));
    crop.setImageSize(kWideImage);
    QVERIFY(crop.isActive());
    QCOMPARE(crop.aspectPreset(), CropController::AspectPreset::Free);
    QCOMPARE(crop.selection(), QRect(QPoint(0, 0), kWideImage));
  }

  //--- CropController: aspect ratio ------------------------------------------

  void presetsLockAndFitTheSelection() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.selectAspectPreset(CropController::AspectPreset::SixteenByNine);
    QVERIFY(fuzzyEqual(crop.aspectWidth(), 16));
    QVERIFY(fuzzyEqual(crop.aspectHeight(), 9));
    // 200 * 16 / 9 = 355.6, rounded; centred horizontally.
    QCOMPARE(crop.selection(), QRect(22, 0, 356, 200));

    crop.selectAspectPreset(CropController::AspectPreset::Screen);
    QVERIFY(fuzzyEqual(crop.aspectWidth(), 1920.0 / 1080.0));
    QVERIFY(fuzzyEqual(crop.aspectHeight(), 1));

    crop.selectAspectPreset(CropController::AspectPreset::CurrentImage);
    QCOMPARE(crop.selection(), QRect(QPoint(0, 0), kLargeImage));

    crop.selectAspectPreset(CropController::AspectPreset::FourByThree);
    QCOMPARE(crop.selection(), QRect(66, 0, 267, 200));
  }

  void freePresetUnlocksWithoutMovingTheSelection() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.selectAspectPreset(CropController::AspectPreset::Square);
    const QRect square = crop.selection();
    crop.selectAspectPreset(CropController::AspectPreset::Free);
    QCOMPARE(crop.selection(), square);
    QVERIFY(fuzzyEqual(crop.aspectWidth(), 2.0));
    // Unlocked: a resize changes the ratio.
    crop.setImageArea(QRectF(QPointF(0, 0), QSizeF(kLargeImage)));
    crop.pointerPressed(QPointF(299, 199), Qt::LeftButton);
    crop.pointerMoved(QPointF(250, 100), Qt::LeftButton);
    crop.pointerReleased(QPointF(250, 100));
    QCOMPARE(crop.selection(), QRect(100, 0, 151, 101));
  }

  void screenPresetWithoutScreenKeepsTheRatio() {
    CropController crop(defaultSettings());
    crop.setImageSize(kLargeImage);
    crop.toggle();
    crop.setCustomAspect(3, 2);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("screen size is unknown"));
    crop.selectAspectPreset(CropController::AspectPreset::Screen);
    QVERIFY(fuzzyEqual(crop.aspectWidth(), 3));
    QVERIFY(fuzzyEqual(crop.aspectHeight(), 2));
  }

  void editedRatioSwitchesToCustom() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.setCustomAspect(1, 2);
    QCOMPARE(crop.aspectPreset(), CropController::AspectPreset::Custom);
    QCOMPARE(crop.selection(), QRect(150, 0, 100, 200));
    crop.swapAspect();
    QVERIFY(fuzzyEqual(crop.aspectWidth(), 2));
    QVERIFY(fuzzyEqual(crop.aspectHeight(), 1));
    QCOMPARE(crop.selection(), QRect(QPoint(0, 0), kLargeImage));
    crop.reset();
    QCOMPARE(crop.aspectPreset(), CropController::AspectPreset::Free);
  }

  //--- CropController: panel -------------------------------------------------

  void inputsAreMovedIntoTheImageAndAlwaysNotified() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    QSignalSpy changed(&crop, &CropController::selectionChanged);
    crop.setSelectionValues(350, 10, 100, 50);
    QCOMPARE(crop.selection(), QRect(300, 10, 100, 50));
    QCOMPARE(changed.count(), 1);
    crop.setSelectionValues(300, 10, 100, 50);
    QCOMPARE(changed.count(), 2);
  }

  void cropRequestsOnlyAPartOfTheImage() {
    CropController crop(defaultSettings());
    QSignalSpy cropped(&crop, &CropController::cropRequested);
    QSignalSpy saved(&crop, &CropController::cropAndSaveRequested);
    openOnLargeImage(crop);
    // The whole image: nothing to crop, the mode just closes.
    crop.cropDefault();
    QVERIFY(!crop.isActive());
    QCOMPARE(cropped.count(), 0);

    crop.toggle();
    crop.setSelectionValues(10, 20, 30, 40);
    crop.cropDefault();
    QVERIFY(!crop.isActive());
    QCOMPARE(cropped.count(), 1);
    QCOMPARE(cropped.constFirst().constFirst().toRect(), QRect(10, 20, 30, 40));

    crop.toggle();
    crop.setSelectionValues(10, 20, 30, 40);
    crop.cropAndSave();
    QCOMPARE(saved.count(), 1);
  }

  void defaultActionFollowsTheChoiceAndTheSettings() {
    CropController crop(settingsWith(SettingsEnums::CropAction::CropAndSave));
    QCOMPARE(crop.defaultAction(), SettingsEnums::CropAction::CropAndSave);
    QSignalSpy saved(&crop, &CropController::cropAndSaveRequested);
    openOnLargeImage(crop);
    crop.setSelectionValues(0, 0, 10, 10);
    crop.cropDefault();
    QCOMPARE(saved.count(), 1);

    QSignalSpy chosen(&crop, &CropController::defaultActionChosen);
    crop.chooseDefaultAction(SettingsEnums::CropAction::Crop);
    QCOMPARE(crop.defaultAction(), SettingsEnums::CropAction::Crop);
    QCOMPARE(chosen.count(), 1);

    QSignalSpy changed(&crop, &CropController::defaultActionChanged);
    crop.applySettings(settingsWith(SettingsEnums::CropAction::CropAndSave));
    QCOMPARE(crop.defaultAction(), SettingsEnums::CropAction::CropAndSave);
    QCOMPARE(changed.count(), 1);
  }

  //--- CropController: pointer -----------------------------------------------

  void pointerDrawsANewSelectionInImagePixels() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.pointerPressed(QPointF(150, 75), Qt::RightButton);
    QVERIFY(!crop.hasSelection());

    crop.pointerPressed(QPointF(150, 75), Qt::LeftButton);
    crop.pointerMoved(QPointF(250, 125), Qt::LeftButton);
    QCOMPARE(crop.cursorShape(), Qt::SizeFDiagCursor);
    crop.pointerReleased(QPointF(250, 125));
    QCOMPARE(crop.selection(), QRect(100, 50, 201, 101));
    QCOMPARE(crop.selectionArea(), QRectF(150, 75, 100.5, 50.5));
  }

  void clickWithoutTravelLeavesNoSelection() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.pointerPressed(QPointF(150, 75), Qt::RightButton);
    crop.pointerPressed(QPointF(150, 75), Qt::LeftButton);
    crop.pointerReleased(QPointF(150, 75));
    QVERIFY(!crop.hasSelection());
  }

  void pointerOutsideTheImageIsClamped() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.pointerPressed(QPointF(0, 0), Qt::RightButton);
    crop.pointerPressed(QPointF(0, 0), Qt::LeftButton);
    crop.pointerMoved(QPointF(1000, 1000), Qt::LeftButton);
    crop.pointerReleased(QPointF(1000, 1000));
    QCOMPARE(crop.selection(), QRect(QPoint(0, 0), kLargeImage));
  }

  void handlesFollowTheSelectionAndHideWhileDragging() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.setSelectionValues(100, 50, 200, 100);
    const QList<QRectF> handles = crop.handles();
    QCOMPARE(handles.size(), 8);
    const qreal size = CropController::kHandleSize;
    QCOMPARE(handles.at(0), QRectF(150, 75, size, size));
    QCOMPARE(handles.at(3), QRectF(250 - size, 125 - size, size, size));
    QCOMPARE(handles.at(6), QRectF(200 - size / 2, 75, size, size));
    // 100 x 50 logical pixels: too small at a ratio of 1, large enough at 2.
    QVERIFY(!crop.handlesVisible());
    crop.setDevicePixelRatio(kHighDpr);
    QVERIFY(crop.handlesVisible());

    crop.pointerMoved(QPointF(152, 77), Qt::NoButton);
    QCOMPARE(crop.cursorShape(), Qt::SizeFDiagCursor);
    crop.pointerMoved(QPointF(200, 100), Qt::NoButton);
    QCOMPARE(crop.cursorShape(), Qt::OpenHandCursor);

    crop.pointerPressed(QPointF(200, 100), Qt::LeftButton);
    QVERIFY(!crop.handlesVisible());
    crop.pointerMoved(QPointF(210, 100), Qt::LeftButton);
    QCOMPARE(crop.cursorShape(), Qt::ClosedHandCursor);
    QCOMPARE(crop.selection(), QRect(120, 50, 200, 100));
    crop.pointerReleased(QPointF(210, 100));
    QVERIFY(crop.handlesVisible());
    QCOMPARE(crop.cursorShape(), Qt::OpenHandCursor);
  }

  void pointerIsIgnoredWhileClosed() {
    CropController crop(defaultSettings());
    crop.setImageSize(kLargeImage);
    crop.setImageArea(kHalfSizeArea);
    crop.pointerPressed(QPointF(150, 75), Qt::LeftButton);
    crop.pointerMoved(QPointF(250, 125), Qt::LeftButton);
    QCOMPARE(crop.cursorShape(), Qt::ArrowCursor);
  }

  void selectAllCoversTheImage() {
    CropController crop(defaultSettings());
    openOnLargeImage(crop);
    crop.setSelectionValues(0, 0, 10, 10);
    crop.selectAll();
    QCOMPARE(crop.selection(), QRect(QPoint(0, 0), kLargeImage));
  }
};

int runCropTests(int argc, char **argv) {
  CropTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_crop.moc"
