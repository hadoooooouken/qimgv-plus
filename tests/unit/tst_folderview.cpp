#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QFont>
#include <QFontMetrics>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "gui/quick/ui/folderview/bookmarksmodel.h"
#include "gui/quick/ui/folderview/foldergridcontroller.h"
#include "gui/quick/ui/folderview/foldergridlayout.h"
#include "gui/quick/ui/folderview/folderviewcontroller.h"
#include "gui/quick/ui/folderview/formatfiltermodel.h"
#include "gui/quick/ui/thumbnails/thumbnaillistmodel.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;
namespace Enums = SettingsEnums;

constexpr int kTextHeight = 15;
constexpr int kIconSize = 128;
// 618 px leave 600 px between the margins: four 148 px cells.
constexpr int kFourColumnWidth = 618;
constexpr int kItemCount = 10;
constexpr qreal kViewHeight = 2000.0;
constexpr int kWheelStep = 120;
constexpr int kWaitMs = 1000;
constexpr int kTreeLoadTimeoutMs = 5000;

UiSettingsSnapshot folderSettings() {
  UiSettingsSnapshot settings;
  settings.folderView.iconSize = kIconSize;
  settings.folderView.placesPanelWidth = FolderViewController::kPlacesPanelMinimumWidth;
  settings.folderView.bookmarksExpanded = true;
  settings.folderView.treeExpanded = true;
  settings.folderView.bookmarks = {u"C:/bookmarked"_s};
  settings.viewer.mouseScrollingSpeed = 1.0;
  return settings;
}

// A grid of kItemCount items in four columns, active over a tall view.
struct GridFixture {
  ThumbnailListModel model;
  FolderGridController grid{model, folderSettings()};

  explicit GridFixture(int count = kItemCount) {
    grid.setViewWidth(kFourColumnWidth);
    model.populate(count);
    model.setViewport(0.0, kViewHeight);
    model.setActive(true);
  }

  // Centre of the item's cell in view coordinates.
  [[nodiscard]] QPointF centreOf(int index) const {
    return grid.layout().itemRect(index).center() - QPointF(0.0, model.viewOffset());
  }

  bool key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString &text = {}) {
    return grid.keyPressed(key, modifiers.toInt(), text);
  }
};

QList<FormatCategory> testCategories() {
  return {
      {u"Common"_s, {{u"JPG"_s, {u"jpg"_s, u"jpeg"_s}}, {u"PNG"_s, {u"png"_s}}}},
      {u"Other"_s, {{u"RAW"_s, {u"cr2"_s, u"nef"_s}}}},
  };
}

QStringList bookmarkNames(const BookmarksModel &bookmarks) {
  QStringList names;
  const QAbstractItemModel *items = bookmarks.items();
  const int nameRole = items->roleNames().key("name");
  for (int row = 0; row < items->rowCount(); ++row)
    names.append(items->data(items->index(row, 0), nameRole).toString());
  return names;
}
} // namespace

// The folder view of the Qt Quick UI: the grid layout and controller, the
// format filter, the bookmarks and the folder view controller.
class FolderViewTests : public QObject {
  Q_OBJECT

private slots:
  //--- layout ---------------------------------------------------------------
  void gridLayoutFollowsTheWidgetGrid() {
    const FolderGridLayout layout = folderGridLayoutFor(
        {.viewWidth = 1000, .iconSize = kIconSize, .itemCount = 20, .textHeight = kTextHeight});
    // 128 + 2 * (8 + 2) wide; 96 + 20 + 9 + 2 * 15 high.
    QCOMPARE(layout.cellWidth(), 148);
    QCOMPARE(layout.cellHeight(), 155);
    QCOMPARE(layout.cell.imageArea, QSize(128, 96));
    QCOMPARE(layout.cell.labelTop, 115);
    QCOMPARE(layout.cell.infoTop, 132);
    QVERIFY(layout.cell.labels);
    // 982 px between the margins: six columns, 94 px split on both sides.
    QCOMPARE(layout.columns, 6);
    QCOMPARE(layout.left, 9.0 + 47.0);
    QCOMPARE(layout.itemRect(7), QRectF(56.0 + 148.0, 6.0 + 155.0, 148.0, 155.0));
  }

  void shortDirectoriesAreNotCentred() {
    const FolderGridLayout layout = folderGridLayoutFor(
        {.viewWidth = 1000, .iconSize = kIconSize, .itemCount = 3, .textHeight = kTextHeight});
    QCOMPARE(layout.left, static_cast<qreal>(FolderGridLayout::kLeftMargin));
    // A view narrower than a cell still has one column.
    const FolderGridLayout narrow = folderGridLayoutFor(
        {.viewWidth = 50, .iconSize = kIconSize, .itemCount = 3, .textHeight = kTextHeight});
    QCOMPARE(narrow.columns, 1);
  }

  void iconSizeIsBounded() {
    QCOMPARE(boundedFolderIconSize(10), FolderGridLayout::kMinimumIconSize);
    QCOMPARE(boundedFolderIconSize(9999), FolderGridLayout::kMaximumIconSize);
    QCOMPARE(boundedFolderIconSize(300), 300);
  }

  //--- grid controller ----------------------------------------------------
  void gridConfiguresTheModel() {
    GridFixture fixture;
    fixture.grid.setDevicePixelRatio(1.5);
    const ThumbnailRequestConfig request = fixture.model.requestConfig();
    QCOMPARE(request.pixelSize, static_cast<int>(1.5 * kIconSize));
    QVERIFY(!request.crop);
    const ThumbnailScrollConfig scroll = fixture.model.scrollConfig();
    QCOMPARE(scroll.columns, 4);
    QCOMPARE(scroll.itemExtent, fixture.grid.layout().cellHeight());
    QCOMPARE(scroll.leadingSpace, FolderGridLayout::kTopMargin);
    QCOMPARE(scroll.preloadDistance, FolderGridController::kPreloadDistance);
    QVERIFY(!scroll.focusShowsNeighbours);
    QVERIFY(!scroll.horizontal);
  }

  void pressSelectsAndDoubleClickActivates() {
    GridFixture fixture;
    QSignalSpy activated(&fixture.model, &ThumbnailListModel::activated);
    fixture.grid.press(3, Qt::LeftButton, Qt::NoModifier, fixture.centreOf(3));
    fixture.grid.release(3, Qt::LeftButton, fixture.centreOf(3));
    QCOMPARE(fixture.model.selection(), QList<int>{3});
    QCOMPARE(activated.count(), 0);
    fixture.grid.doubleClick(3, Qt::LeftButton);
    QCOMPARE(activated.count(), 1);
  }

  void arrowsMoveInTheGrid() {
    GridFixture fixture;
    fixture.model.select(0);
    fixture.key(Qt::Key_Left);
    QCOMPARE(fixture.model.currentIndex(), 0);
    fixture.key(Qt::Key_Right);
    QCOMPARE(fixture.model.currentIndex(), 1);
    fixture.key(Qt::Key_Down);
    QCOMPARE(fixture.model.currentIndex(), 5);
    fixture.key(Qt::Key_Down);
    QCOMPARE(fixture.model.currentIndex(), 9);
    // The last row: Down stays.
    fixture.key(Qt::Key_Down);
    QCOMPARE(fixture.model.currentIndex(), 9);
    fixture.key(Qt::Key_Up);
    QCOMPARE(fixture.model.currentIndex(), 5);
    fixture.key(Qt::Key_Home);
    QCOMPARE(fixture.model.currentIndex(), 0);
    fixture.key(Qt::Key_End);
    QCOMPARE(fixture.model.currentIndex(), 9);
  }

  void upReturnsToTheColumnDownLeft() {
    GridFixture fixture;
    // Item 6 is in column 2; below it the short last row ends at 9 (column 1).
    fixture.model.select(6);
    fixture.key(Qt::Key_Down);
    QCOMPARE(fixture.model.currentIndex(), 9);
    fixture.key(Qt::Key_Up);
    QCOMPARE(fixture.model.currentIndex(), 6);
  }

  void pageKeysMoveFourRows() {
    GridFixture fixture(40);
    fixture.model.select(1);
    fixture.key(Qt::Key_PageDown);
    QCOMPARE(fixture.model.currentIndex(), 1 + 4 * 4);
    fixture.key(Qt::Key_PageUp);
    QCOMPARE(fixture.model.currentIndex(), 1);
  }

  void shiftExtendsTheSelection() {
    GridFixture fixture;
    fixture.model.select(1);
    fixture.key(Qt::Key_Shift, Qt::ShiftModifier);
    fixture.key(Qt::Key_Right, Qt::ShiftModifier);
    fixture.key(Qt::Key_Right, Qt::ShiftModifier);
    QCOMPARE(fixture.model.selection(), (QList<int>{1, 2, 3}));
    fixture.key(Qt::Key_Down, Qt::ShiftModifier);
    QCOMPARE(fixture.model.selection(), (QList<int>{1, 2, 3, 4, 5, 6, 7}));
    // Shift-click extends from the same anchor.
    fixture.grid.press(0, Qt::LeftButton, Qt::ShiftModifier, fixture.centreOf(0));
    QCOMPARE(fixture.model.selection(), (QList<int>{1, 0}));
    fixture.grid.keyReleased(Qt::Key_Shift);
    fixture.key(Qt::Key_Right);
    QCOMPARE(fixture.model.selection(), QList<int>{1});
  }

  void controlAKeepsTheCurrentItem() {
    GridFixture fixture;
    fixture.model.select(3);
    QVERIFY(fixture.key(Qt::Key_A, Qt::ControlModifier, u"\x01"_s));
    QCOMPARE(fixture.model.selection().count(), kItemCount);
    QCOMPARE(fixture.model.currentIndex(), 3);
    // Other Ctrl shortcuts are left to the actions.
    QVERIFY(!fixture.key(Qt::Key_R, Qt::ControlModifier, u"\x12"_s));
    QVERIFY(!fixture.key(Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier, u"\x01"_s));
  }

  void keysActivateGoUpAndTypeAhead() {
    GridFixture fixture;
    fixture.model.select(2);
    QSignalSpy activated(&fixture.model, &ThumbnailListModel::activated);
    QSignalSpy actions(&fixture.grid, &FolderGridController::actionRequested);
    QSignalSpy typeAhead(&fixture.grid, &FolderGridController::typeAheadRequested);
    QVERIFY(fixture.key(Qt::Key_Return));
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.first().at(0).toInt(), 2);
    QVERIFY(fixture.key(Qt::Key_Backspace));
    QCOMPARE(actions.count(), 1);
    QCOMPARE(actions.first().at(0).toString(), u"goUp"_s);
    QVERIFY(fixture.key(Qt::Key_B, Qt::NoModifier, u"b"_s));
    QCOMPARE(typeAhead.count(), 1);
    QCOMPARE(typeAhead.first().at(0).toString(), u"b"_s);
    // Alt combinations and keys without text go to the actions.
    QVERIFY(!fixture.key(Qt::Key_B, Qt::AltModifier, u"b"_s));
    QVERIFY(!fixture.key(Qt::Key_F5));
    QCOMPARE(typeAhead.count(), 1);
  }

  void rubberBandSelectsTouchedCells() {
    GridFixture fixture;
    const FolderGridLayout layout = fixture.grid.layout();
    QSignalSpy band(&fixture.grid, &FolderGridController::rubberBandChanged);
    fixture.model.select(9);
    // From the left margin into the second cell of the second row.
    const QPointF start(1.0, 1.0);
    const QPointF end = layout.itemRect(5).topLeft() + QPointF(5.0, 5.0);
    fixture.grid.press(-1, Qt::LeftButton, Qt::NoModifier, start);
    QVERIFY(fixture.model.selection().isEmpty());
    fixture.grid.move(end, Qt::LeftButton, Qt::NoModifier);
    QCOMPARE(fixture.model.selection(), (QList<int>{0, 1, 4, 5}));
    QCOMPARE(fixture.grid.rubberBand(), QRectF(start, end).normalized());
    fixture.grid.release(-1, Qt::LeftButton, end);
    QVERIFY(fixture.grid.rubberBand().isNull());
    QVERIFY(band.count() >= 2);
  }

  void controlRubberBandTogglesAgainstItsStart() {
    GridFixture fixture;
    const FolderGridLayout layout = fixture.grid.layout();
    fixture.model.select(0);
    fixture.grid.press(-1, Qt::LeftButton, Qt::ControlModifier, QPointF(1.0, 1.0));
    QCOMPARE(fixture.model.selection(), QList<int>{0});
    const QPointF end = layout.itemRect(1).topLeft() + QPointF(5.0, 5.0);
    fixture.grid.move(end, Qt::LeftButton, Qt::ControlModifier);
    QCOMPARE(fixture.model.selection(), QList<int>{1});
    fixture.grid.release(-1, Qt::LeftButton, end);
  }

  void rightClickOpensTheMenuUnlessItWasAGesture() {
    GridFixture fixture(100);
    QSignalSpy menu(&fixture.grid, &FolderGridController::contextMenuRequested);
    const QPointF point = fixture.centreOf(2);
    fixture.grid.press(2, Qt::RightButton, Qt::NoModifier, point);
    fixture.grid.release(2, Qt::RightButton, point);
    QCOMPARE(menu.count(), 1);
    QCOMPARE(menu.first().at(0).toPointF(), point);
    QCOMPARE(fixture.model.selection(), QList<int>{2});

    fixture.grid.press(2, Qt::RightButton, Qt::NoModifier, point);
    fixture.grid.move(point + QPointF(0.0, ThumbnailListModel::kGestureThreshold + 1.0),
                      Qt::RightButton, Qt::NoModifier);
    fixture.grid.release(2, Qt::RightButton, point);
    QCOMPARE(menu.count(), 1);
  }

  void controlWheelZoomsAndStoresTheSize() {
    GridFixture fixture;
    QSignalSpy committed(&fixture.grid, &FolderGridController::iconSizeCommitted);
    QSignalSpy scrolls(&fixture.model, &ThumbnailListModel::scrollRequested);
    fixture.grid.wheel({0, kWheelStep}, {}, Qt::ControlModifier);
    QCOMPARE(fixture.grid.iconSize(), kIconSize + FolderGridLayout::kZoomStep);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.first().at(0).toInt(), kIconSize + FolderGridLayout::kZoomStep);
    // The thumbnails are requested again at the new size.
    QCOMPARE(fixture.model.requestConfig().pixelSize, kIconSize + FolderGridLayout::kZoomStep);
    fixture.grid.wheel({0, -kWheelStep}, {}, Qt::ControlModifier);
    fixture.grid.wheel({0, -kWheelStep}, {}, Qt::ControlModifier);
    QCOMPARE(fixture.grid.iconSize(), FolderGridLayout::kMinimumIconSize);
    QCOMPARE(scrolls.count(), 0);
  }

  void sliderSnapsAndStoresOnRelease() {
    GridFixture fixture;
    QSignalSpy committed(&fixture.grid, &FolderGridController::iconSizeCommitted);
    QCOMPARE(FolderGridController::zoomSliderMinimum(), 700);
    QCOMPARE(FolderGridController::zoomSliderMaximum(), 900);
    fixture.grid.setZoomSliderPressed(true);
    fixture.grid.setZoomSliderValue(FolderGridController::kSliderSnapValue + 5);
    QCOMPARE(fixture.grid.iconSize(), 256);
    QCOMPARE(fixture.grid.zoomSliderValue(), FolderGridController::kSliderSnapValue);
    fixture.grid.setZoomSliderValue(850);
    QCOMPARE(fixture.grid.iconSize(), 362);
    QCOMPARE(committed.count(), 0);
    fixture.grid.setZoomSliderPressed(false);
    QCOMPARE(committed.count(), 1);
    QCOMPARE(committed.first().at(0).toInt(), 362);
  }

  void dropsKeepTheirTargetAndAction() {
    GridFixture fixture;
    QSignalSpy over(&fixture.grid, &FolderGridController::draggedOver);
    QSignalSpy dropped(&fixture.grid, &FolderGridController::urlsDropped);
    QVERIFY(FolderGridController::acceptsDropAction(Qt::CopyAction));
    QVERIFY(FolderGridController::acceptsDropAction(Qt::MoveAction));
    QVERIFY(!FolderGridController::acceptsDropAction(Qt::LinkAction));

    fixture.grid.dragMoved(3);
    QCOMPARE(over.last().at(0).toInt(), 3);
    // The presenter frames a folder; a new target removes the frame.
    fixture.model.setDragHover(3);
    fixture.grid.dragMoved(42);
    QCOMPARE(over.last().at(0).toInt(), -1);
    QVERIFY(!fixture.model.data(fixture.model.index(3), ThumbnailListModel::DragHoverRole).toBool());

    const QList<QUrl> urls{QUrl::fromLocalFile(u"C:/images/a.png"_s)};
    fixture.grid.drop(urls, nullptr, 3, Qt::MoveAction);
    QCOMPARE(dropped.count(), 1);
    QCOMPARE(dropped.first().at(0).value<QList<QUrl>>(), urls);
    QCOMPARE(dropped.first().at(2).toInt(), 3);
    QCOMPARE(dropped.first().at(3).value<Qt::DropAction>(), Qt::MoveAction);
    QTest::ignoreMessage(QtWarningMsg, "FolderGridController ignores a drop with action 4");
    fixture.grid.drop(urls, nullptr, 3, Qt::LinkAction);
    QCOMPARE(dropped.count(), 1);
  }

  void selectedImagesSkipFolders() {
    GridFixture fixture;
    QSignalSpy changed(&fixture.grid, &FolderGridController::selectedImageCountChanged);
    fixture.model.setDirCount(2);
    fixture.model.select(QList<int>{0, 1, 2, 3});
    QCOMPARE(fixture.grid.selectedImageCount(), 2);
    fixture.model.setDirCount(3);
    QCOMPARE(fixture.grid.selectedImageCount(), 1);
    QCOMPARE(changed.count(), 2);
  }

  void labelColourContrastsWithTheSurface() {
    QCOMPARE(FolderGridController::labelTextColor(QColor(Qt::white)), QColor(Qt::black));
    QCOMPARE(FolderGridController::labelTextColor(QColor(30, 30, 30)), QColor(Qt::white));
  }

  //--- format filter --------------------------------------------------------
  void formatFilterStartsANewSetFromAllFormats() {
    FormatFilterModel filter(testCategories());
    QSignalSpy selected(&filter, &FormatFilterModel::filterSelected);
    QVERIFY(filter.allFormats());
    QCOMPARE(filter.displayText(), u"All formats"_s);
    filter.setFormatChecked(1, true);
    QCOMPARE(filter.displayText(), u"PNG"_s);
    QCOMPARE(selected.last().at(0).toStringList(), QStringList{u"png"_s});
    QCOMPARE(filter.data(filter.index(0), FormatFilterModel::CheckStateRole).toInt(),
             static_cast<int>(Qt::PartiallyChecked));
    filter.setFormatChecked(0, true);
    QCOMPARE(filter.displayText(), u"Custom"_s);
    QCOMPARE(filter.data(filter.index(0), FormatFilterModel::CheckStateRole).toInt(),
             static_cast<int>(Qt::Checked));
    QCOMPARE(selected.last().at(0).toStringList(),
             (QStringList{u"jpg"_s, u"jpeg"_s, u"png"_s}));
  }

  void formatFilterCategoriesAndEmptySets() {
    FormatFilterModel filter(testCategories());
    QSignalSpy selected(&filter, &FormatFilterModel::filterSelected);
    filter.setCategoryChecked(1, true);
    QCOMPARE(selected.last().at(0).toStringList(), (QStringList{u"cr2"_s, u"nef"_s}));
    // Unchecking the last format shows all formats again.
    filter.setCategoryChecked(1, false);
    QVERIFY(filter.allFormats());
    QVERIFY(selected.last().at(0).toStringList().isEmpty());
    filter.setFormatChecked(0, true);
    filter.selectAllFormats();
    QVERIFY(filter.allFormats());
    QCOMPARE(selected.count(), 4);
    const QVariantList formats =
        filter.data(filter.index(0), FormatFilterModel::FormatsRole).toList();
    QCOMPARE(formats.size(), 2);
    QCOMPARE(formats.at(1).toMap().value(u"label"_s).toString(), u"PNG"_s);
    QCOMPARE(formats.at(1).toMap().value(u"index"_s).toInt(), 1);
  }

  void formatFilterShowsTheStoredExtensions() {
    FormatFilterModel filter(testCategories());
    QSignalSpy selected(&filter, &FormatFilterModel::filterSelected);
    filter.setCheckedExtensions({u"NEF"_s});
    QCOMPARE(filter.displayText(), u"RAW"_s);
    QCOMPARE(filter.checkedExtensions(), (QStringList{u"cr2"_s, u"nef"_s}));
    filter.setCheckedExtensions({u"xyz"_s});
    QVERIFY(filter.allFormats());
    QCOMPARE(selected.count(), 0);
    QTest::ignoreMessage(QtWarningMsg, "FormatFilterModel has no format 7");
    filter.setFormatChecked(7, true);
  }

  //--- bookmarks ------------------------------------------------------------
  void bookmarksHoldTheHomeFolderWhenEmpty() {
    BookmarksModel bookmarks(u"C:/Users/me"_s);
    QSignalSpy changed(&bookmarks, &BookmarksModel::pathsChanged);
    bookmarks.setPaths({});
    QCOMPARE(bookmarks.paths(), QStringList{u"C:/Users/me"_s});
    QCOMPARE(changed.count(), 1);
    QCOMPARE(bookmarkNames(bookmarks), QStringList{u"me"_s});
  }

  void bookmarksAddMoveAndRemove() {
    BookmarksModel bookmarks(u"C:/home"_s);
    QSignalSpy changed(&bookmarks, &BookmarksModel::pathsChanged);
    bookmarks.setPaths({u"C:/a"_s, u"C:/b"_s, u"C:/a"_s});
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/a"_s, u"C:/b"_s}));
    QCOMPARE(changed.count(), 0);
    bookmarks.add(u"C:/c"_s);
    bookmarks.add(u"C:/a"_s);
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/a"_s, u"C:/b"_s, u"C:/c"_s}));
    bookmarks.moveUp(u"C:/c"_s);
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/a"_s, u"C:/c"_s, u"C:/b"_s}));
    bookmarks.moveDown(u"C:/a"_s);
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/c"_s, u"C:/a"_s, u"C:/b"_s}));
    bookmarks.move(0, 2);
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/a"_s, u"C:/b"_s, u"C:/c"_s}));
    bookmarks.move(2, 0);
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/c"_s, u"C:/a"_s, u"C:/b"_s}));
    bookmarks.remove(u"C:/a"_s);
    QCOMPARE(bookmarks.paths(), (QStringList{u"C:/c"_s, u"C:/b"_s}));
    QCOMPARE(changed.count(), 6);
    QCOMPARE(changed.last().at(0).toStringList(), bookmarks.paths());
    // A drive root shows its path.
    bookmarks.add(u"D:/"_s);
    QCOMPARE(bookmarkNames(bookmarks).last(), u"D:\\"_s);
  }

  void bookmarksTakeDroppedFolders() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(u"folder"_s));
    QFile file(dir.filePath(u"image.png"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    BookmarksModel bookmarks(u"C:/home"_s);
    bookmarks.setPaths({u"C:/a"_s});
    QVERIFY(!bookmarks.addFolders({QUrl::fromLocalFile(file.fileName())}));
    QVERIFY(bookmarks.addFolders({QUrl::fromLocalFile(dir.filePath(u"folder"_s))}));
    QCOMPARE(bookmarks.paths().last(), dir.filePath(u"folder"_s));
    QSignalSpy current(&bookmarks, &BookmarksModel::currentPathChanged);
    bookmarks.setCurrentPath(u"C:/a"_s);
    bookmarks.setCurrentPath(u"C:/a"_s);
    QCOMPARE(current.count(), 1);
  }

  //--- folder view controller ---------------------------------------------
  void placesPanelNeedsRoomAndTheButton() {
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy stored(&view, &FolderViewController::placesPanelEnabledRequested);
    QVERIFY(!view.isPlacesPanelShown());
    view.setPlacesPanelEnabled(true);
    QVERIFY(view.isPlacesPanelShown());
    QCOMPARE(stored.count(), 1);
    view.setViewWidth(FolderViewController::kPlacesPanelMinimumViewWidth - 1);
    QVERIFY(!view.isPlacesPanelShown());
    view.setViewWidth(FolderViewController::kPlacesPanelMinimumViewWidth);
    QVERIFY(view.isPlacesPanelShown());
    QVERIFY(!view.isCompactTopBar());
    view.setViewWidth(FolderViewController::kCompactTopBarWidth - 1);
    QVERIFY(view.isCompactTopBar());

    QSignalSpy width(&view, &FolderViewController::placesPanelWidthRequested);
    view.resizePlacesPanel(10);
    QCOMPARE(view.placesPanelWidth(), FolderViewController::kPlacesPanelMinimumWidth);
    QCOMPARE(width.count(), 0);
    view.resizePlacesPanel(400);
    QCOMPARE(width.last().at(0).toInt(), 400);
  }

  void settingsAreApplied() {
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    UiSettingsSnapshot settings = folderSettings();
    settings.folderView.placesPanel = true;
    settings.folderView.placesPanelWidth = 333;
    settings.folderView.treeExpanded = false;
    settings.folderView.folderSortingMode = Enums::SortingMode::TimeDescending;
    settings.folderView.formatFilter = {u"png"_s};
    settings.folderView.bookmarks = {u"C:/x"_s, u"C:/y"_s};
    settings.folderView.iconSize = 200;
    view.applySettings(settings);
    QVERIFY(view.isPlacesPanelEnabled());
    QCOMPARE(view.placesPanelWidth(), 333);
    QVERIFY(!view.treeExpanded());
    QCOMPARE(view.folderSortingMode(), static_cast<int>(Enums::SortingMode::TimeDescending));
    QCOMPARE(view.formatFilter()->displayText(), u"PNG"_s);
    QVERIFY(view.formatFilter()->checkedExtensions().contains(u"png"_s));
    QCOMPARE(view.bookmarks()->paths(), (QStringList{u"C:/x"_s, u"C:/y"_s}));
    QCOMPARE(grid.iconSize(), 200);
  }

  void nameFilterIsPublishedAfterTheDelay() {
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy selected(&view, &FolderViewController::nameFilterSelected);
    view.editNameFilter(u"a"_s);
    view.editNameFilter(u"ab"_s);
    QCOMPARE(selected.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(selected.count(), 1, kWaitMs);
    QCOMPARE(selected.first().at(0).toString(), u"ab"_s);
    QCOMPARE(view.nameFilter(), u"ab"_s);
  }

  void sortingRequestsAreChecked() {
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy sorting(&view, &FolderViewController::sortingSelected);
    QSignalSpy folderSorting(&view, &FolderViewController::folderSortingSelected);
    view.selectSorting(static_cast<int>(Enums::SortingMode::SizeDescending));
    QCOMPARE(sorting.last().at(0).value<Enums::SortingMode>(), Enums::SortingMode::SizeDescending);
    view.selectFolderSorting(static_cast<int>(Enums::SortingMode::Time));
    QCOMPARE(folderSorting.last().at(0).value<Enums::SortingMode>(), Enums::SortingMode::Time);
    QTest::ignoreMessage(QtWarningMsg, "FolderViewController ignores the sorting mode 9");
    view.selectSorting(9);
    QCOMPARE(sorting.count(), 1);
    // The indicators follow Core without publishing.
    view.setSortingMode(Enums::SortingMode::Name);
    QCOMPARE(view.sortingMode(), static_cast<int>(Enums::SortingMode::Name));
    QCOMPARE(sorting.count(), 1);
  }

  void firstActivationAsksForTheTree() {
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy created(&view, &FolderViewController::createdChanged);
    QSignalSpy treeRequests(&view, &FolderViewController::folderTreeRequested);
    view.setActive(true);
    QVERIFY(view.isCreated());
    QCOMPARE(created.count(), 1);
    QCOMPARE(treeRequests.count(), 1);
    QFileSystemModel tree;
    view.setFolderTree(&tree);
    view.setActive(false);
    view.setActive(true);
    QCOMPARE(created.count(), 1);
    QCOMPARE(treeRequests.count(), 1);
    view.setFolderTree(nullptr);
  }

  void readyAtOnceWithoutTheTree() {
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy ready(&view, &FolderViewController::filesystemViewReady);
    // Nothing is ready before the view is laid out.
    view.setDirectoryPath(u"Z:/missing/folder"_s);
    QCOMPARE(ready.count(), 0);
    // The places panel is off.
    view.setViewWidth(kFourColumnWidth);
    QCOMPARE(ready.count(), 1);
    view.setDirectoryPath(u"Z:/missing/other"_s);
    QCOMPARE(ready.count(), 2);

    // Shown with the tree: the parent never loads until the tree hides.
    view.setPlacesPanelEnabled(true);
    view.setDirectoryPath(u"Z:/missing/folder"_s);
    QCOMPARE(ready.count(), 2);
    QSignalSpy stored(&view, &FolderViewController::treeExpandedRequested);
    view.toggleTree();
    QCOMPARE(ready.count(), 3);
    QCOMPARE(stored.last().at(0).toBool(), false);
  }

  void readyOnceTheTreeLoadedTheParent() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(u"a/b"_s));
    QVERIFY(QDir(dir.path()).mkdir(u"c"_s));
    const QString path = QDir(dir.path()).filePath(u"a/b"_s);

    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    UiSettingsSnapshot settings = folderSettings();
    settings.folderView.placesPanel = true;
    FolderViewController view(grid, settings, u"C:/home"_s);
    view.setViewWidth(kFourColumnWidth);
    QFileSystemModel tree;
    tree.setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    tree.setRootPath(QString());
    view.setFolderTree(&tree);
    QSignalSpy ready(&view, &FolderViewController::filesystemViewReady);
    QSignalSpy current(&view, &FolderViewController::currentFolderChanged);
    view.setDirectoryPath(path);
    QCOMPARE(current.count(), 1);
    QVERIFY(view.hasCurrentFolder());
    QCOMPARE(tree.filePath(view.currentFolder()), path);
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, kTreeLoadTimeoutMs);
    // The folders above are listed like an expanded widget tree: the
    // sibling of the parent shows.
    QTRY_COMPARE_WITH_TIMEOUT(tree.rowCount(tree.index(dir.path())), 2, kTreeLoadTimeoutMs);

    // A refresh waits again.
    view.beginFolderTreeRefresh(path);
    emit tree.directoryLoaded(QDir(dir.path()).filePath(u"a"_s));
    view.endFolderTreeRefresh();
    QCOMPARE(ready.count(), 2);

    QSignalSpy selected(&view, &FolderViewController::directorySelected);
    view.openFolder(view.currentFolder());
    QCOMPARE(selected.last().at(0).toString(), path);
    view.setFolderTree(nullptr);
  }

  void dropsOntoBookmarksAndFolders() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkdir(u"folder"_s));
    QFile file(dir.filePath(u"image.png"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    const QUrl fileUrl = QUrl::fromLocalFile(file.fileName());
    const QUrl folderUrl = QUrl::fromLocalFile(dir.filePath(u"folder"_s));

    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy copies(&view, &FolderViewController::copyUrlsRequested);
    QSignalSpy moves(&view, &FolderViewController::moveUrlsRequested);
    view.dropOnBookmark({fileUrl, folderUrl}, u"C:/bookmarked"_s, Qt::CopyAction);
    QCOMPARE(copies.count(), 1);
    QCOMPARE(copies.first().at(0).value<QList<QString>>(), QList<QString>{file.fileName()});
    QCOMPARE(copies.first().at(1).toString(), u"C:/bookmarked"_s);
    QVERIFY(view.bookmarks()->paths().contains(dir.filePath(u"folder"_s)));
    // Only folders are taken by the header.
    QVERIFY(!view.dropFoldersOnBookmarks({fileUrl}));

    QFileSystemModel tree;
    view.setFolderTree(&tree);
    const QModelIndex target = tree.index(dir.filePath(u"folder"_s));
    view.dropOnFolder({fileUrl}, target, Qt::MoveAction);
    QCOMPARE(moves.count(), 1);
    QCOMPARE(moves.first().at(1).toString(), dir.filePath(u"folder"_s));
    QTest::ignoreMessage(QtWarningMsg,
                         "FolderViewController cannot drop onto a folder outside the folder tree");
    view.dropOnFolder({fileUrl}, {}, Qt::MoveAction);
    view.setFolderTree(nullptr);
  }

  void homeAndBookmarkDialogFolders() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ThumbnailListModel model;
    FolderGridController grid(model, folderSettings());
    FolderViewController view(grid, folderSettings(), u"C:/home"_s);
    QSignalSpy selected(&view, &FolderViewController::directorySelected);
    view.goHome();
    QCOMPARE(selected.last().at(0).toString(), u"C:/home"_s);
    view.openBookmark(u"C:/bookmarked"_s);
    QCOMPARE(selected.last().at(0).toString(), u"C:/bookmarked"_s);
    QCOMPARE(view.bookmarkDialogFolder(), QUrl::fromLocalFile(u"C:/home"_s));
    view.setDirectoryPath(dir.path());
    QCOMPARE(view.bookmarkDialogFolder(), QUrl::fromLocalFile(dir.path()));
    view.addBookmark(QUrl::fromLocalFile(dir.path()));
    QVERIFY(view.bookmarks()->paths().contains(dir.path()));
    QCOMPARE(view.bookmarks()->currentPath(), dir.path());
  }
};

int runFolderViewTests(int argc, char **argv) {
  FolderViewTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_folderview.moc"
