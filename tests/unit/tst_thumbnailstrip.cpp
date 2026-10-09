#include <QFont>
#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "gui/quick/adapters/directoryviewadapter.h"
#include "gui/quick/render/thumbnailitem.h"
#include "gui/quick/ui/thumbnails/thumbnaillistmodel.h"
#include "gui/quick/ui/thumbnails/thumbnailpanelcontroller.h"
#include "gui/quick/ui/thumbnails/thumbnailstriplayout.h"
#include "sourcecontainers/thumbnail.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;
namespace Enums = SettingsEnums;

constexpr int kRequestSize = 100;
constexpr int kItemExtent = 100;
constexpr int kThumbnailSize = 80;
constexpr qreal kViewExtent = 500.0;
constexpr int kTextHeight = 15;
// A hide delay short enough to wait for.
constexpr int kShortHideDelayMs = 10;
constexpr int kWaitMarginMs = 100;
constexpr QSize kLargeWindow(1200, 800);
constexpr QSize kSmallWindow(600, 400);

ThumbnailEntry entryOf(int width, int height) {
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::red);
  return {.handle = {.image = image, .sourceSize = image.size()},
          .name = u"name"_s,
          .info = u"info"_s};
}

// A model of count items, configured with kRequestSize thumbnails and
// kItemExtent cells, active over a view of kViewExtent at offset 0.
void prepare(ThumbnailListModel &model, int count, bool active = true) {
  model.configure({.pixelSize = kRequestSize});
  model.setScrollConfig({.itemExtent = kItemExtent, .thumbnailSize = kThumbnailSize});
  model.populate(count);
  model.setViewport(0.0, kViewExtent);
  if (active)
    model.setActive(true);
}

bool isLoaded(const ThumbnailListModel &model, int row) {
  return model.data(model.index(row), ThumbnailListModel::LoadedRole).toBool();
}

UiSettingsSnapshot panelSettings(bool pinned, Enums::PanelPosition position) {
  UiSettingsSnapshot settings;
  settings.panel.enabled = true;
  settings.panel.pinned = pinned;
  settings.panel.position = position;
  settings.panel.previewsSize = kThumbnailSize;
  settings.panel.hideDelayMs = kShortHideDelayMs;
  settings.viewer.mouseScrollingSpeed = 1.0;
  return settings;
}

// Window coordinates in the area of a bottom panel and above it.
QPointF bottomPanelPoint() {
  return {kLargeWindow.width() / 2.0, kLargeWindow.height() - 2.0};
}
QPointF viewerPoint() {
  return {kLargeWindow.width() / 2.0, kLargeWindow.height() / 2.0};
}
} // namespace

class ThumbnailStripTests : public QObject {
  Q_OBJECT

private slots:
  //--- layout ---------------------------------------------------------------
  void layoutOfHorizontalSimpleStrip() {
    const ThumbnailStripLayout layout = thumbnailStripLayoutFor(
        {.previewsSize = 120,
         .style = Enums::PanelStyle::Simple,
         .position = Enums::PanelPosition::Bottom,
         .textHeight = kTextHeight});
    QVERIFY(layout.horizontal);
    QCOMPARE(layout.thumbnailSize, 120);
    // 120 + (9 + 2) * 2, 90 + (9 + 4) * 2 (ThumbnailWidget::updateBoundingRect).
    QCOMPARE(layout.cellWidth, 142);
    QCOMPARE(layout.cellHeight, 116);
    QCOMPARE(layout.itemExtent(), 142);
    QCOMPARE(layout.imageArea, QSize(120, 90));
    QCOMPARE(layout.imageCenterOffset, 0);
    QVERIFY(!layout.labels);
    // MainPanel::sizeHint(): + 16, + 3 at the bottom.
    QCOMPARE(layout.panelExtent, 135);
    const ThumbnailStripLayout top = thumbnailStripLayoutFor(
        {.previewsSize = 120, .position = Enums::PanelPosition::Top});
    QCOMPARE(top.panelExtent, 132);
  }

  void layoutOfVerticalExtendedStrip() {
    const ThumbnailStripLayout layout = thumbnailStripLayoutFor(
        {.previewsSize = 100,
         .style = Enums::PanelStyle::Extended,
         .position = Enums::PanelPosition::Left,
         .textHeight = kTextHeight});
    QVERIFY(!layout.horizontal);
    // 100 + (9 + 12) * 2; 75 + (9 + 2) * 2 + 9 + 2 * 15.
    QCOMPARE(layout.cellWidth, 142);
    QCOMPARE(layout.cellHeight, 136);
    QCOMPARE(layout.itemExtent(), 136);
    QCOMPARE(layout.panelExtent, 158);
    QVERIFY(layout.labels);
    QCOMPARE(layout.imageCenterOffset, -kTextHeight);
    QCOMPARE(layout.labelLeft, 21);
    QCOMPARE(layout.labelTop, 95);
    QCOMPARE(layout.infoTop, 112);
    QCOMPARE(layout.labelWidth, 100);
  }

  void layoutBoundsThePreviewSize() {
    QCOMPARE(thumbnailStripLayoutFor({.previewsSize = 5}).thumbnailSize,
             ThumbnailStripLayout::kMinimumPreviewSize);
    QCOMPARE(thumbnailStripLayoutFor({.previewsSize = 500}).thumbnailSize,
             ThumbnailStripLayout::kMaximumPreviewSize);
  }

  //--- draw size --------------------------------------------------------------
  void drawSizeFitsTheImageArea() {
    QCOMPARE(thumbnailDrawSize({200, 150}, {2000, 1500}, {120, 90}, 1.0), QSize(120, 90));
    QCOMPARE(thumbnailDrawSize({100, 200}, {1000, 2000}, {120, 90}, 1.0), QSize(45, 90));
  }

  void drawSizeIsLimitedByTheSource() {
    QCOMPARE(thumbnailDrawSize({200, 150}, {60, 45}, {120, 90}, 1.0), QSize(60, 45));
    // The source counts in device pixels.
    QCOMPARE(thumbnailDrawSize({200, 150}, {120, 90}, {120, 90}, 2.0), QSize(60, 45));
    // A source in the other orientation is compared turned.
    QCOMPARE(thumbnailDrawSize({90, 120}, {40, 30}, {120, 120}, 1.0), QSize(30, 40));
    // No source size: no limit.
    QCOMPARE(thumbnailDrawSize({200, 150}, {}, {120, 90}, 1.0), QSize(120, 90));
    QVERIFY(thumbnailDrawSize({}, {60, 45}, {120, 90}, 1.0).isEmpty());
  }

  //--- loading ----------------------------------------------------------------
  void requestsTheVisibleAndPreloadedItems() {
    ThumbnailListModel model;
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    prepare(model, 1000);
    QCOMPARE(requests.count(), 1);
    const auto indices = requests.first().at(0).value<QList<int>>();
    // 0 .. floor((500 + kStripPreloadDistance) / 100).
    QCOMPARE(indices.first(), 0);
    QCOMPARE(indices.last(), 35);
    QCOMPARE(indices.count(), 36);
    QCOMPARE(requests.first().at(1).toInt(), kRequestSize);
    QCOMPARE(requests.first().at(3).toBool(), false);

    // Requested items are not requested again.
    model.setViewport(kItemExtent, kViewExtent);
    QCOMPARE(requests.count(), 2);
    QCOMPARE(requests.last().at(0).value<QList<int>>(), QList<int>{36});
  }

  void requestsNothingWhileInactiveOrBlocked() {
    ThumbnailListModel model;
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    prepare(model, 100, false);
    QCOMPARE(requests.count(), 0);
    model.setLoadingBlocked(true);
    model.setActive(true);
    QCOMPARE(requests.count(), 0);
    model.setLoadingBlocked(false);
    QCOMPARE(requests.count(), 1);
  }

  void requestsBackwardsNearestFirst() {
    ThumbnailListModel model;
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    prepare(model, 1000, false);
    model.setScrollConfig({.itemExtent = kItemExtent,
                           .thumbnailSize = kThumbnailSize,
                           .trackpadDetection = true});
    model.setViewport(50000.0, kViewExtent);
    // A touchpad scroll towards the start.
    model.wheelScrolled({0, 10}, {0, 10});
    model.setActive(true);
    QCOMPARE(requests.count(), 1);
    const auto indices = requests.first().at(0).value<QList<int>>();
    QVERIFY(indices.first() > indices.last());
  }

  void reportsVisibleThumbnailsReadyOnce() {
    ThumbnailListModel model;
    QSignalSpy ready(&model, &ThumbnailListModel::visibleThumbnailsReady);
    prepare(model, 3);
    QCOMPARE(ready.count(), 0);
    model.setThumbnail(0, entryOf(4, 3), kRequestSize);
    model.setThumbnailUnavailable(1, kRequestSize);
    // Another size is not the requested thumbnail.
    model.setThumbnail(2, entryOf(4, 3), kRequestSize + 1);
    QCOMPARE(ready.count(), 0);
    QVERIFY(!isLoaded(model, 2));
    model.setThumbnail(2, entryOf(4, 3), kRequestSize);
    QCOMPARE(ready.count(), 1);
    model.setThumbnail(2, entryOf(4, 3), kRequestSize);
    QCOMPARE(ready.count(), 1);
    QVERIFY(isLoaded(model, 0));
    QVERIFY(model.data(model.index(1), ThumbnailListModel::UnavailableRole).toBool());

    model.populate(0);
    QCOMPARE(ready.count(), 2);
  }

  void waitsForFinalThumbnails() {
    ThumbnailListModel model;
    QSignalSpy ready(&model, &ThumbnailListModel::visibleThumbnailsReady);
    prepare(model, 1);
    model.setThumbnailPending(0, true);
    model.setThumbnail(0, entryOf(4, 3), kRequestSize);
    QCOMPARE(ready.count(), 0);
    QVERIFY(model.data(model.index(0), ThumbnailListModel::PendingRole).toBool());
    model.setThumbnailPending(0, false);
    QCOMPARE(ready.count(), 1);
  }

  void insertAndRemoveShiftItems() {
    ThumbnailListModel model;
    prepare(model, 5);
    model.setThumbnail(3, entryOf(4, 3), kRequestSize);
    model.select(3);
    model.insertItem(1);
    QCOMPARE(model.itemCount(), 6);
    QVERIFY(isLoaded(model, 4));
    QVERIFY(!isLoaded(model, 3));
    QCOMPARE(model.selection(), QList<int>{4});
    QVERIFY(model.data(model.index(4), ThumbnailListModel::SelectedRole).toBool());

    model.removeItem(0);
    QCOMPARE(model.itemCount(), 5);
    QVERIFY(isLoaded(model, 3));
    QCOMPARE(model.selection(), QList<int>{3});
  }

  void removingTheSelectionSelectsANeighbour() {
    ThumbnailListModel model;
    prepare(model, 5);
    model.select(4);
    model.removeItem(4);
    QCOMPARE(model.selection(), QList<int>{3});
    model.select(1);
    model.removeItem(1);
    QCOMPARE(model.selection(), QList<int>{1});
  }

  void selectionDropsInvalidItems() {
    ThumbnailListModel model;
    prepare(model, 5);
    model.select(QList<int>{-1, 2, 9});
    QCOMPARE(model.selection(), QList<int>{2});
    QCOMPARE(model.currentIndex(), 2);
    model.select(42);
    QCOMPARE(model.selection(), QList<int>{0});
  }

  void unloadsThumbnailsFarOutOfView() {
    ThumbnailListModel model;
    model.configure({.pixelSize = kRequestSize, .unloadOffscreen = true});
    model.setScrollConfig({.itemExtent = kItemExtent, .thumbnailSize = kThumbnailSize});
    model.populate(1000);
    model.setViewport(0.0, kViewExtent);
    model.setActive(true);
    model.setThumbnail(0, entryOf(4, 3), kRequestSize);
    QVERIFY(isLoaded(model, 0));
    model.setViewport(50000.0, kViewExtent);
    QVERIFY(!isLoaded(model, 0));
    // A thumbnail arriving out of range is not kept.
    model.setThumbnail(1, entryOf(4, 3), kRequestSize);
    QVERIFY(!isLoaded(model, 1));
  }

  void newRequestConfigUnloadsAndRequestsAgain() {
    ThumbnailListModel model;
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    prepare(model, 2);
    model.setThumbnail(0, entryOf(4, 3), kRequestSize);
    requests.clear();
    model.configure({.pixelSize = kRequestSize, .crop = true});
    QVERIFY(!isLoaded(model, 0));
    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.first().at(0).value<QList<int>>(), (QList<int>{0, 1}));
    QCOMPARE(requests.first().at(2).toBool(), true);
  }

  void reloadForcesARequest() {
    ThumbnailListModel model;
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    prepare(model, 2);
    model.setThumbnail(1, entryOf(4, 3), kRequestSize);
    requests.clear();
    model.reloadItem(1);
    QVERIFY(!isLoaded(model, 1));
    QCOMPARE(requests.count(), 1);
    QCOMPARE(requests.first().at(0).value<QList<int>>(), QList<int>{1});
    QCOMPARE(requests.first().at(3).toBool(), true);
  }

  //--- scrolling --------------------------------------------------------------
  void focusCentresTheSelection() {
    ThumbnailListModel model;
    prepare(model, 100);
    model.setScrollConfig({.itemExtent = kItemExtent,
                           .thumbnailSize = kThumbnailSize,
                           .centerSelection = true});
    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    model.focusOn(50);
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), 5050.0 - kViewExtent / 2.0);
    QCOMPARE(scrolls.first().at(1).toInt(), 0);
    // Near the end the offset stops at the last page.
    model.focusOn(99);
    QCOMPARE(scrolls.last().at(0).toReal(), 100 * kItemExtent - kViewExtent);
  }

  void focusKeepsTheItemInViewWithAMargin() {
    ThumbnailListModel model;
    prepare(model, 100);
    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    model.focusOn(2);
    QCOMPARE(scrolls.count(), 0);
    // Half a cell of the next item stays visible.
    model.focusOn(10);
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), 1150.0 - kViewExtent);
    model.focusOn(0);
    QCOMPARE(scrolls.last().at(0).toReal(), 0.0);
  }

  void smoothWheelStepsAccumulate() {
    ThumbnailListModel model;
    prepare(model, 100);
    model.setScrollConfig({.itemExtent = kItemExtent,
                           .thumbnailSize = kThumbnailSize,
                           .smoothScroll = true,
                           .wheelSpeed = 1.0});
    model.setViewport(1000.0, kViewExtent);
    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    model.wheelScrolled({0, -120}, {});
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), 1000.0 + 120 * ThumbnailListModel::kWheelScrollMultiplier);
    QCOMPARE(scrolls.first().at(1).toInt(), ThumbnailListModel::kScrollDurationMs);
    // Nothing loads while the animation runs.
    requests.clear();
    model.setViewport(1100.0, kViewExtent);
    QCOMPARE(requests.count(), 0);
    // A step right after the first adds to its target, faster.
    model.wheelScrolled({0, -120}, {});
    QCOMPARE(scrolls.count(), 2);
    QCOMPARE(scrolls.last().at(0).toReal(),
             1300.0 + 120 * ThumbnailListModel::kWheelScrollMultiplier *
                          ThumbnailListModel::kScrollAcceleration);
    QVERIFY(scrolls.last().at(1).toInt() < ThumbnailListModel::kScrollDurationMs);
    // The view arrives; its end loads what came into range.
    model.setViewport(1720.0, kViewExtent);
    model.scrollAnimationFinished();
    QCOMPARE(requests.count(), 2);
  }

  void wheelWithoutSmoothScrollingStepsByItem() {
    ThumbnailListModel model;
    prepare(model, 100);
    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    model.wheelScrolled({0, -120}, {});
    // Items 0 - 5 are (nearly) visible: item 6 is brought into view.
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), 200.0);
    QCOMPARE(scrolls.first().at(1).toInt(), 0);
    // At the start nothing scrolls back.
    model.setViewport(0.0, kViewExtent);
    model.wheelScrolled({0, 120}, {});
    QCOMPARE(scrolls.count(), 1);
  }

  void touchpadScrollsByPixels() {
    ThumbnailListModel model;
    prepare(model, 100);
    model.setScrollConfig({.itemExtent = kItemExtent,
                           .thumbnailSize = kThumbnailSize,
                           .smoothScroll = true,
                           .trackpadDetection = true});
    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    model.wheelScrolled({0, -30}, {0, -12});
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), 30.0);
    QCOMPARE(scrolls.first().at(1).toInt(), 0);
  }

  //--- pointer ----------------------------------------------------------------
  void pressActivatesAnItem() {
    ThumbnailListModel model;
    prepare(model, 10);
    QSignalSpy activated(&model, &ThumbnailListModel::activated);
    model.press(3, Qt::LeftButton, Qt::NoModifier, {});
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().at(0).toInt(), 3);
    model.doubleClick(4, Qt::LeftButton);
    QCOMPARE(activated.count(), 2);
    model.doubleClick(4, Qt::RightButton);
    QCOMPARE(activated.count(), 2);
  }

  void controlPressTogglesTheSelection() {
    ThumbnailListModel model;
    prepare(model, 10);
    model.select(1);
    QSignalSpy activated(&model, &ThumbnailListModel::activated);
    model.press(3, Qt::LeftButton, Qt::ControlModifier, {});
    QCOMPARE(model.selection(), (QList<int>{1, 3}));
    model.press(1, Qt::LeftButton, Qt::ControlModifier, {});
    QCOMPARE(model.selection(), QList<int>{3});
    // The last item stays selected.
    model.press(3, Qt::LeftButton, Qt::ControlModifier, {});
    QCOMPARE(model.selection(), QList<int>{3});
    QCOMPARE(activated.count(), 0);
  }

  void releaseSelectsWithinAMultiSelection() {
    ThumbnailListModel model;
    prepare(model, 10);
    model.select(QList<int>{1, 2});
    QSignalSpy activated(&model, &ThumbnailListModel::activated);
    model.press(2, Qt::LeftButton, Qt::NoModifier, {10, 10});
    QCOMPARE(activated.count(), 0);
    model.release(2, Qt::LeftButton, {12, 10});
    QCOMPARE(model.selection(), QList<int>{2});
  }

  void pressOnTheBackgroundClearsTheSelection() {
    ThumbnailListModel model;
    prepare(model, 2);
    model.select(1);
    model.press(-1, Qt::LeftButton, Qt::ControlModifier, {});
    QCOMPARE(model.selection(), QList<int>{1});
    model.press(-1, Qt::LeftButton, Qt::NoModifier, {});
    QVERIFY(model.selection().isEmpty());
  }

  void dragOutStartsFromASelectedItem() {
    ThumbnailListModel model;
    prepare(model, 10);
    model.select(QList<int>{1, 2});
    QSignalSpy dragged(&model, &ThumbnailListModel::dragOutRequested);
    model.press(2, Qt::LeftButton, Qt::NoModifier, {0, 0});
    model.move({ThumbnailListModel::kDragThreshold / 2.0, 0}, Qt::LeftButton);
    QCOMPARE(dragged.count(), 0);
    model.move({ThumbnailListModel::kDragThreshold, 0}, Qt::LeftButton);
    QCOMPARE(dragged.count(), 1);
  }

  void rightButtonSelectsOrScrollsToAnEnd() {
    ThumbnailListModel model;
    prepare(model, 100);
    model.select(1);
    model.press(4, Qt::RightButton, Qt::NoModifier, {200, 0});
    model.release(4, Qt::RightButton, {200, 0});
    QCOMPARE(model.selection(), QList<int>{4});

    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    model.press(4, Qt::RightButton, Qt::NoModifier, {200, 0});
    model.move({200 - ThumbnailListModel::kGestureThreshold - 1.0, 0}, Qt::RightButton);
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), 100 * kItemExtent - kViewExtent);
    QVERIFY(scrolls.first().at(1).toInt() >= ThumbnailListModel::kEdgeScrollMinimumMs);
    // A gesture selects nothing.
    model.release(6, Qt::RightButton, {100, 0});
    QCOMPARE(model.selection(), QList<int>{4});
  }

  void backAndForwardButtonsNavigate() {
    ThumbnailListModel model;
    prepare(model, 3);
    QSignalSpy back(&model, &ThumbnailListModel::backNavigationRequested);
    QSignalSpy forward(&model, &ThumbnailListModel::forwardNavigationRequested);
    model.press(1, Qt::BackButton, Qt::NoModifier, {});
    model.press(-1, Qt::ForwardButton, Qt::NoModifier, {});
    QCOMPARE(back.count(), 1);
    QCOMPARE(forward.count(), 1);
  }

  //--- rows -------------------------------------------------------------------
  void rowsRequestTheVisibleAndPreloadedItems() {
    constexpr int kColumns = 4;
    constexpr int kLeadingSpace = 6;
    constexpr int kPreloadDistance = 200;
    ThumbnailListModel model;
    model.configure({.pixelSize = kRequestSize});
    model.setScrollConfig({.horizontal = false,
                           .itemExtent = kItemExtent,
                           .columns = kColumns,
                           .leadingSpace = kLeadingSpace,
                           .preloadDistance = kPreloadDistance});
    model.populate(100);
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    model.setViewport(0.0, kViewExtent);
    model.setActive(true);
    QCOMPARE(requests.count(), 1);
    // Rows 0 .. floor((500 + 200 - 6) / 100) = 6, four items each.
    const auto indices = requests.first().at(0).value<QList<int>>();
    QCOMPARE(indices.size(), 7 * kColumns);
    QCOMPARE(indices.constFirst(), 0);
    QCOMPARE(indices.constLast(), 7 * kColumns - 1);
  }

  void rowsFocusAndScrollByRow() {
    constexpr int kColumns = 4;
    constexpr int kLeadingSpace = 6;
    ThumbnailListModel model;
    model.configure({.pixelSize = kRequestSize});
    model.setScrollConfig({.horizontal = false,
                           .itemExtent = kItemExtent,
                           .columns = kColumns,
                           .leadingSpace = kLeadingSpace,
                           .focusShowsNeighbours = false,
                           .thumbnailSize = kThumbnailSize});
    model.populate(100);
    model.setViewport(0.0, kViewExtent);
    model.setActive(true);
    QSignalSpy scrolls(&model, &ThumbnailListModel::scrollRequested);
    // Item 41 is in row 10, which ends at 6 + 11 * 100; no neighbour margin.
    model.focusOn(41);
    QCOMPARE(scrolls.count(), 1);
    QCOMPARE(scrolls.first().at(0).toReal(), kLeadingSpace + 11.0 * kItemExtent - kViewExtent);
    // The content ends after 25 rows.
    model.scrollToItem(99);
    QCOMPARE(model.viewOffset(), kLeadingSpace + 25.0 * kItemExtent - kViewExtent);
    // An item of a visible row does not scroll.
    scrolls.clear();
    model.scrollToItem(97);
    QCOMPARE(scrolls.count(), 0);
  }

  void doubleClickActivationSelectsOnPress() {
    ThumbnailListModel model;
    prepare(model, 10);
    model.setItemActivation(ThumbnailListModel::ItemActivation::OnDoubleClick);
    QSignalSpy activated(&model, &ThumbnailListModel::activated);
    model.press(3, Qt::LeftButton, Qt::NoModifier, {});
    QCOMPARE(activated.count(), 0);
    QCOMPARE(model.selection(), QList<int>{3});
    model.doubleClick(3, Qt::LeftButton);
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().at(0).toInt(), 3);
  }

  void shiftExtendsARangeFromTheAnchor() {
    ThumbnailListModel model;
    prepare(model, 10);
    model.select(4);
    // Without an anchor Shift-press changes nothing (the strip).
    model.press(6, Qt::LeftButton, Qt::ShiftModifier, {});
    QCOMPARE(model.selection(), QList<int>{4});

    model.beginRangeSelection();
    QVERIFY(model.rangeSelectionActive());
    model.press(6, Qt::LeftButton, Qt::ShiftModifier, {});
    QCOMPARE(model.selection(), (QList<int>{4, 5, 6}));
    // A new end replaces the previous range; the end is the current item.
    model.selectRangeTo(2);
    QCOMPARE(model.selection(), (QList<int>{4, 3, 2}));
    QCOMPARE(model.currentIndex(), 2);
    model.endRangeSelection();
    QVERIFY(!model.rangeSelectionActive());
    // A new directory drops the anchor.
    model.populate(10);
    model.select(1);
    model.selectRangeTo(3);
    QCOMPARE(model.selection(), QList<int>{1});
  }

  //--- panel ------------------------------------------------------------------
  void pinnedPanelIsDockedInALargeWindow() {
    ThumbnailListModel model;
    ThumbnailPanelController panel(model, panelSettings(true, Enums::PanelPosition::Bottom));
    QVERIFY(!panel.isShown());
    panel.setWindowSize(kLargeWindow);
    QVERIFY(panel.isShown());
    QVERIFY(panel.isDocked());
    QVERIFY(!panel.isAnimated());
    panel.setWindowSize(kSmallWindow);
    QVERIFY(!panel.isShown());
    QVERIFY(!panel.isDocked());
    // A pinned panel ignores the pointer and the folder view.
    panel.setWindowSize(kLargeWindow);
    panel.pointerMoved(viewerPoint(), Qt::NoButton);
    panel.setFolderViewActive(true);
    QVERIFY(panel.isShown());
  }

  void floatingPanelFollowsThePointer() {
    ThumbnailListModel model;
    ThumbnailPanelController panel(model, panelSettings(false, Enums::PanelPosition::Bottom));
    panel.setWindowSize(kLargeWindow);
    QVERIFY(!panel.isShown());
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    QVERIFY(panel.isShown());
    QVERIFY(panel.isAnimated());
    QVERIFY(!panel.isDocked());
    panel.pointerMoved(viewerPoint(), Qt::NoButton);
    QVERIFY(panel.isShown());
    QTRY_VERIFY_WITH_TIMEOUT(!panel.isShown(), kShortHideDelayMs + kWaitMarginMs);
    QVERIFY(panel.isAnimated());
  }

  // The crop mode turns the viewer input off.
  void lockedViewerKeepsTheFloatingPanelHidden() {
    ThumbnailListModel model;
    ThumbnailPanelController panel(model, panelSettings(false, Enums::PanelPosition::Bottom));
    panel.setWindowSize(kLargeWindow);
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    QVERIFY(panel.isShown());
    panel.setInteractionEnabled(false);
    QVERIFY(!panel.isShown());
    QVERIFY(!panel.isAnimated());
    panel.pointerMoved(viewerPoint(), Qt::NoButton);
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    QVERIFY(!panel.isShown());
    panel.setInteractionEnabled(true);
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    QVERIFY(panel.isShown());
  }

  void pressInThePanelAreaKeepsItHidden() {
    ThumbnailListModel model;
    ThumbnailPanelController panel(model, panelSettings(false, Enums::PanelPosition::Bottom));
    panel.setWindowSize(kLargeWindow);
    panel.pointerMoved(bottomPanelPoint(), Qt::LeftButton);
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    QVERIFY(!panel.isShown());
    // Until the pointer left the area.
    panel.pointerMoved(viewerPoint(), Qt::NoButton);
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    QVERIFY(panel.isShown());
  }

  void leavingTheWindowHidesAfterTheGracePeriod() {
    ThumbnailListModel model;
    ThumbnailPanelController panel(model, panelSettings(false, Enums::PanelPosition::Bottom));
    panel.setWindowSize(kLargeWindow);
    panel.pointerMoved(bottomPanelPoint(), Qt::NoButton);
    panel.pointerLeftWindow();
    QTest::qWait(kShortHideDelayMs + kWaitMarginMs);
    QVERIFY(panel.isShown());
    QTRY_VERIFY_WITH_TIMEOUT(!panel.isShown(),
                             ThumbnailPanelController::kWindowExitMinimumHideDelayMs +
                                 kWaitMarginMs);
  }

  void fullscreenOnlyPanelShowsInFullscreen() {
    ThumbnailListModel model;
    UiSettingsSnapshot settings = panelSettings(false, Enums::PanelPosition::Top);
    settings.panel.fullscreenOnly = true;
    ThumbnailPanelController panel(model, settings);
    panel.setWindowSize(kLargeWindow);
    panel.pointerMoved({10, 2}, Qt::NoButton);
    QVERIFY(!panel.isShown());
    QVERIFY(!panel.isExitButtonVisible());
    panel.setFullscreen(true);
    QVERIFY(panel.isExitButtonVisible());
    panel.pointerMoved({10, 2}, Qt::NoButton);
    QVERIFY(panel.isShown());
    // The folder view hides it at once.
    panel.setFolderViewActive(true);
    QVERIFY(!panel.isShown());
    QVERIFY(!panel.isAnimated());
  }

  void pinButtonPublishesThePinnedState() {
    ThumbnailListModel model;
    ThumbnailPanelController panel(model, panelSettings(false, Enums::PanelPosition::Bottom));
    panel.setWindowSize(kLargeWindow);
    QSignalSpy pins(&panel, &ThumbnailPanelController::pinRequested);
    panel.togglePinned();
    QCOMPARE(pins.count(), 1);
    QCOMPARE(pins.first().at(0).toBool(), true);
    QVERIFY(panel.isPinned());
    QVERIFY(panel.isDocked());
  }

  void panelConfiguresTheModel() {
    ThumbnailListModel model;
    UiSettingsSnapshot settings = panelSettings(false, Enums::PanelPosition::Left);
    settings.folderView.squareThumbnails = true;
    settings.panel.unloadThumbnails = true;
    settings.panel.centerSelection = true;
    ThumbnailPanelController panel(model, settings);
    panel.setDevicePixelRatio(1.5);
    QCOMPARE(model.requestConfig().pixelSize, static_cast<int>(1.5 * kThumbnailSize));
    QVERIFY(model.requestConfig().crop);
    QVERIFY(model.requestConfig().unloadOffscreen);
    QVERIFY(!model.scrollConfig().horizontal);
    QCOMPARE(model.scrollConfig().itemExtent, panel.layout().cellHeight);
    QVERIFY(model.scrollConfig().centerSelection);
    // The slide blocks the requests.
    prepare(model, 3, false);
    model.configure({.pixelSize = kRequestSize});
    QSignalSpy requests(&model, &ThumbnailListModel::thumbnailsNeeded);
    panel.setAnimationRunning(true);
    model.setActive(true);
    QCOMPARE(requests.count(), 0);
    panel.setAnimationRunning(false);
    QCOMPARE(requests.count(), 1);
  }

  void labelTextContrastsWithItsBackground() {
    ThumbnailListModel model;
    const ThumbnailPanelController panel(model, panelSettings(false, Enums::PanelPosition::Top));
    QCOMPARE(panel.labelTextColor(QColor(Qt::white)), QColor(Qt::black));
    QCOMPARE(panel.labelTextColor(QColor(32, 32, 32)), QColor(Qt::white));
  }

  //--- adapter ----------------------------------------------------------------
  void adapterPublishesThumbnailsAndRequests() {
    DirectoryViewAdapter adapter;
    IDirectoryView &view = adapter;
    QSignalSpy requests(&adapter, &DirectoryViewAdapter::thumbnailsRequested);
    QSignalSpy activated(&adapter, &DirectoryViewAdapter::itemActivated);
    adapter.configure({.pixelSize = kRequestSize});
    adapter.setScrollConfig({.itemExtent = kItemExtent, .thumbnailSize = kThumbnailSize});
    view.populate(3);
    view.setDirCount(1);
    adapter.setViewport(0.0, kViewExtent);
    adapter.setActive(true);
    QCOMPARE(requests.count(), 1);

    QImage image(40, 30, QImage::Format_RGB32);
    image.fill(Qt::blue);
    view.setThumbnail(2, std::make_shared<Thumbnail>(u"a.png"_s, u"40x30"_s, kRequestSize, image));
    QVERIFY(isLoaded(adapter, 2));
    QCOMPARE(adapter.data(adapter.index(2), ThumbnailListModel::NameRole).toString(), u"a.png"_s);
    const auto handle =
        adapter.data(adapter.index(2), ThumbnailListModel::ThumbnailRole).value<ThumbnailHandle>();
    QCOMPARE(handle.image.cacheKey(), image.cacheKey());
    QVERIFY(adapter.data(adapter.index(0), ThumbnailListModel::DirectoryRole).toBool());
    QVERIFY(!adapter.data(adapter.index(1), ThumbnailListModel::DirectoryRole).toBool());

    adapter.press(1, Qt::LeftButton, Qt::NoModifier, {});
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().at(0).toInt(), 1);
    view.select(QList<int>{2});
    QCOMPARE(view.selection(), QList<int>{2});
  }
};

int runThumbnailStripTests(int argc, char **argv) {
  ThumbnailStripTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_thumbnailstrip.moc"
