#include "bridgetestfixture.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDropEvent>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMetaProperty>
#include <QMimeData>
#include <QMouseEvent>
#include <QTest>
#include <QWheelEvent>

#include <memory>
#include <utility>

#include "gui/quick/adapters/themesnapshotbuilder.h"
#include "settings_types.h"
#include "themestore.h"

namespace {
using namespace Qt::StringLiterals;

// Shortcuts the fake dispatcher starts with.
const QMap<QString, QString> &initialShortcuts() {
  static const QMap<QString, QString> shortcuts = {
      {u"nextImage"_s, u"Right"_s},
      {u"prevImage"_s, u"Left"_s},
      {u"zoomIn"_s, u"Ctrl++"_s},
  };
  return shortcuts;
}

const QString kIconFontFamily = u"FluentSystemIcons-Custom"_s;
const QString kTestImagePath = u"C:/fixture/test.png"_s;
// Destinations of the copy / move overlay.
const QStringList kCopyTargets{u"C:/fixture/first"_s, u"C:/fixture/second"_s,
                               u"C:/fixture/third"_s};
constexpr QRgb kTestImageColor = 0xff3080c0;
// Thumbnail strip: preview size, hide delay, and the delivered thumbnails
// (4:3, one hue per item).
constexpr int kTestPreviewsSize = 80;
constexpr int kTestPanelHideDelayMs = 10;
constexpr int kTestThumbnailWidth = 64;
constexpr int kTestThumbnailHeight = 48;
// Size of the image the thumbnails are made from (larger than any cell).
constexpr QSize kTestSourceSize(1600, 1200);
constexpr int kHueSteps = 360;
constexpr int kHueStride = 37;
constexpr int kThumbnailSaturation = 200;
constexpr int kThumbnailValue = 200;

// The application's own dark and light schemes, built as the application
// builds them (the two have distinct backgrounds, so a switch changes
// ThemeColors).
ThemeSnapshot testTheme(bool dark) {
  return buildThemeSnapshot(
      ThemeStore::colorScheme(dark ? COLORS_DARK : COLORS_LIGHT),
      QGuiApplication::font(), kIconFontFamily);
}

UiSettingsSnapshot testSettings() {
  UiSettingsSnapshot settings;
  settings.viewer.smoothZoom = true;
  settings.viewer.fitMode = SettingsEnums::FitMode::Window;
  settings.viewer.zoomStep = 0.25;
  settings.viewer.imageScrolling = SettingsEnums::ImageScrolling::ByTrackpadAndWheel;
  settings.viewer.mouseScrollingSpeed = 1.0;
  settings.viewer.trackpadDetection = true;
  settings.panel.position = SettingsEnums::PanelPosition::Bottom;
  settings.overlays.zoomIndicatorMode = SettingsEnums::ZoomIndicatorMode::Enabled;
  settings.folderView.iconSize = FolderGridLayout::kMinimumIconSize;
  settings.folderView.placesPanelWidth = FolderViewController::kPlacesPanelMinimumWidth;
  settings.folderView.bookmarksExpanded = true;
  settings.folderView.treeExpanded = true;
  return settings;
}

// Disk usage the fake thumbnail cache reports until it is cleared.
constexpr qint64 kTestCacheBytes = 4096;

// Stored settings the settings window tests start from: valid values of
// every page, two Upscayl models.
SettingsValues testSettingsValues() {
  SettingsValues v;
  v.general.language = u"en_US"_s;
  v.general.zoomIndicatorMode = INDICATOR_AUTO;
  v.general.autoResizeLimitStep = 18;
  v.general.panelEnabled = true;
  v.general.panelHideDelayMs = 600;
  v.general.thumbPanelStyle = TH_PANEL_EXTENDED;
  v.general.panelSizeStep = 20;
  v.general.panelPosition = PANEL_BOTTOM;
  v.general.folderEndAction = FOLDER_END_GOTO_ADJACENT;
  v.general.sortingMode = SORT_NAME;
  v.general.slideshowIntervalMs = 3000;
  v.view.imageFitMode = FIT_WINDOW;
  v.view.focusPoint = FOCUS_CURSOR;
  v.view.expandLimit = 2;
  v.view.zoomStepPercent = 25;
  v.view.zoomLevels = u"0.5,1,2"_s;
  v.view.scalingFilter = QI_FILTER_MKS2021_GPU;
  v.view.casSharpeningPercent = 40;
  v.view.monitorProfileType = u"System"_s;
  v.view.hdrTargetWhiteLevel = 203;
  v.theme.themeMode = THEME_DARK;
  v.theme.accentColor = QColor(u"#3daee9"_s);
  v.theme.backgroundOpacityPercent = 100;
  v.theme.thumbnailOpacityPercent = 60;
  v.controls.mouseScrollingSpeedStep = 2;
  v.advanced.thumbnailerThreads = 4;
  v.advanced.thumbnailResolution = 256;
  v.advanced.thumbnailCacheMaxSizeMB = 512;
  v.advanced.jpegQuality = 95;
  v.advanced.modernQuality = 90;
  v.advanced.pngCompression = 3;
  v.advanced.memoryLimitMB = 1024;
  v.upscale.model = u"modelA"_s;
  v.upscale.limitPercent = 200;
  return v;
}

// The meta-object and address of page in values; nulls for no page.
std::pair<const QMetaObject *, void *> settingsPage(SettingsValues &values,
                                                    const QString &page) {
  if (page == u"general"_s)
    return {&GeneralSettings::staticMetaObject, &values.general};
  if (page == u"view"_s)
    return {&ViewSettings::staticMetaObject, &values.view};
  if (page == u"theme"_s)
    return {&ThemeSettings::staticMetaObject, &values.theme};
  if (page == u"controls"_s)
    return {&ControlsSettings::staticMetaObject, &values.controls};
  if (page == u"advanced"_s)
    return {&AdvancedSettings::staticMetaObject, &values.advanced};
  if (page == u"upscale"_s)
    return {&UpscaleSettings::staticMetaObject, &values.upscale};
  return {nullptr, nullptr};
}

QMetaProperty settingsField(const QMetaObject *page, const QString &field) {
  if (!page)
    return {};
  return page->property(page->indexOfProperty(field.toLatin1().constData()));
}
} // namespace

//------------------------------------------------------------------------------
FakeActionDispatcher::FakeActionDispatcher() : mShortcuts(initialShortcuts()) {}

QStringList FakeActionDispatcher::actionNames() const {
  return mShortcuts.keys();
}

QString FakeActionDispatcher::shortcutFor(const QString &action) const {
  return mShortcuts.value(action);
}

bool FakeActionDispatcher::invoke(const QString &action) {
  if (!mShortcuts.contains(action))
    return false;
  lastInvoked = action;
  return true;
}

bool FakeActionDispatcher::processEvent(QInputEvent &event) {
  lastModifiers = event.modifiers().toInt();
  if (event.type() == QEvent::KeyPress) {
    lastKey = static_cast<QKeyEvent &>(event).key();
    return true;
  }
  if (event.type() == QEvent::Wheel) {
    lastWheelAngleDelta = static_cast<QWheelEvent &>(event).angleDelta();
    return true;
  }
  if (event.type() == QEvent::MouseButtonPress ||
      event.type() == QEvent::MouseButtonRelease ||
      event.type() == QEvent::MouseButtonDblClick) {
    mouseEventTypes.append(static_cast<int>(event.type()));
    mouseButtons.append(static_cast<int>(static_cast<QMouseEvent &>(event).button()));
    return true;
  }
  return false;
}

QString FakeActionDispatcher::shortcutText(QInputEvent &event) const {
  if (event.type() == QEvent::Wheel) {
    const auto &wheelEvent = static_cast<const QWheelEvent &>(event);
    return wheelEvent.angleDelta().y() > 0 ? u"WheelUp"_s : u"WheelDown"_s;
  }
  if (event.type() == QEvent::MouseButtonPress || event.type() == QEvent::MouseButtonRelease) {
    // Presses of every button but the right one, releases of the right one.
    const auto &mouseEvent = static_cast<const QMouseEvent &>(event);
    const bool rightButton = mouseEvent.button() == Qt::RightButton;
    if ((event.type() == QEvent::MouseButtonPress) == rightButton)
      return {};
    return rightButton ? u"RMB"_s : u"LMB"_s;
  }
  if (event.type() != QEvent::KeyPress)
    return {};
  const auto &keyEvent = static_cast<const QKeyEvent &>(event);
  return QKeySequence(keyEvent.keyCombination()).toString();
}

QString FakeActionDispatcher::keyText(const QKeyEvent &event) const {
  return QKeySequence(event.key()).toString();
}

void FakeActionDispatcher::setShortcut(const QString &action,
                                       const QString &shortcut) {
  mShortcuts.insert(action, shortcut);
}

//------------------------------------------------------------------------------
BridgeTestFixture::BridgeTestFixture(QObject *parent)
    : QObject(parent), mSettings(testSettings()),
      mSettingsBridge(mSettings), mThemeBridge(testTheme(mDark)),
      mActionBridge(mDispatcher), mViewport(mSettings), mOverlays(mSettings),
      mThumbnailPanel(mThumbnails, mSettings), mContextMenu(mDispatcher),
      mCrop(mSettings), mFolderGrid(mFolderThumbnails, mSettings),
      mFolderView(mFolderGrid, mSettings, QDir::homePath()),
      mSettingsEditor(mSettingsValues, mShortcutScripts),
      mSettingsDialog(mSettingsEditor) {
  resetSettingsStores();
  mThumbnailPanel.setLabelFont(QGuiApplication::font());
  connectFolderView();
  connectDialogs();
  connect(&mThumbnails, &ThumbnailListModel::thumbnailsNeeded, this,
          [this](const QList<int> &indices) {
            ++mThumbnailRequestCount;
            mRequestedThumbnailCount += indices.count();
            mUnansweredThumbnails.append(indices);
          });
  connect(&mThumbnails, &ThumbnailListModel::activated, this,
          [this](int index) { mLastActivatedThumbnail = index; });
  connect(&mThumbnailPanel, &ThumbnailPanelController::pinRequested, this,
          [this](bool pinned) { mLastPinRequest = pinned ? 1 : 0; });

  connect(&mWindowShell, &MainWindowShell::urlsDropped, this,
          [this](const QList<QUrl> &urls) { mLastDroppedUrls = urls; });

  CopyTargetsModel *targets = mOverlays.copyTargets();
  connect(&mOverlays, &OverlayCoordinator::copyTargetsNeeded, this,
          [targets]() { targets->setTargets(kCopyTargets); });
  connect(targets, &CopyTargetsModel::copyRequested, this,
          [this](const QString &dir) { mLastFileRequest = u"copy:"_s + dir; });
  connect(targets, &CopyTargetsModel::moveRequested, this,
          [this](const QString &dir) { mLastFileRequest = u"move:"_s + dir; });
  connect(mOverlays.renamePrompt(), &RenamePromptController::renameRequested,
          this, [this](const QString &name) { mLastFileRequest = u"rename:"_s + name; });
  connect(&mOverlays, &OverlayCoordinator::saveRequested, this,
          [this]() { mLastFileRequest = u"save"_s; });
  connect(&mOverlays, &OverlayCoordinator::saveAsRequested, this,
          [this]() { mLastFileRequest = u"saveAs"_s; });
  connect(&mOverlays, &OverlayCoordinator::discardEditsRequested, this,
          [this]() { mLastFileRequest = u"discard"_s; });
  connect(mOverlays.colorAdjustmentsEditor(), &ColorAdjustmentsEditor::previewChanged,
          &mViewport, &ImageViewportController::setColorAdjustments);

  connect(&mContextMenu, &ContextMenuModel::scriptSettingsRequested, this,
          [this]() { ++mScriptSettingsRequests; });

  const auto syncCropImage = [this]() {
    mCrop.setImageSize(mViewport.imageSize());
    mCrop.setDevicePixelRatio(mViewport.devicePixelRatio());
    mCrop.setImageArea(mViewport.imageArea());
  };
  connect(&mViewport, &ImageViewportController::imageChanged, this, syncCropImage);
  connect(&mViewport, &ImageViewportController::imageGeometryChanged, this,
          syncCropImage);
  connect(&mCrop, &CropController::activeChanged, this, [this]() {
    const bool active = mCrop.isActive();
    mViewport.setExpandSmallImagesInFitMode(active);
    if (active)
      mViewport.fitWindow();
    mViewport.setInteractionEnabled(!active);
    mContextMenu.setInteractionEnabled(!active);
    mThumbnailPanel.setInteractionEnabled(!active);
  });
  const auto rectText = [](const QRect &rect) {
    return u"%1,%2,%3,%4"_s.arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
  };
  connect(&mCrop, &CropController::cropRequested, this,
          [this, rectText](const QRect &rect) { mLastCropRequest = u"crop:"_s + rectText(rect); });
  connect(&mCrop, &CropController::cropAndSaveRequested, this, [this, rectText](const QRect &rect) {
    mLastCropRequest = u"cropAndSave:"_s + rectText(rect);
  });
  connect(&mCrop, &CropController::defaultActionChosen, this,
          [this](SettingsEnums::CropAction action) {
            mLastCropRequest = u"default:"_s + QString::number(static_cast<int>(action));
          });
}

SettingsBridge &BridgeTestFixture::settingsBridge() { return mSettingsBridge; }

ThemeBridge &BridgeTestFixture::themeBridge() { return mThemeBridge; }

ActionBridge &BridgeTestFixture::actionBridge() { return mActionBridge; }

QString BridgeTestFixture::lastInvoked() const {
  return mDispatcher.lastInvoked;
}

int BridgeTestFixture::lastKey() const { return mDispatcher.lastKey; }

int BridgeTestFixture::lastModifiers() const {
  return mDispatcher.lastModifiers;
}

QPoint BridgeTestFixture::lastWheelAngleDelta() const {
  return mDispatcher.lastWheelAngleDelta;
}

QVariantList BridgeTestFixture::mouseEventTypes() const {
  return mDispatcher.mouseEventTypes;
}

QVariantList BridgeTestFixture::mouseButtons() const {
  return mDispatcher.mouseButtons;
}

ImageViewportController *BridgeTestFixture::viewportController() {
  return &mViewport;
}

MainWindowShell *BridgeTestFixture::windowShell() {
  return &mWindowShell;
}

QList<QUrl> BridgeTestFixture::lastDroppedUrls() const {
  return mLastDroppedUrls;
}

OverlayCoordinator *BridgeTestFixture::overlays() { return &mOverlays; }

QString BridgeTestFixture::lastFileRequest() const { return mLastFileRequest; }

ThumbnailPanelController *BridgeTestFixture::thumbnailPanel() {
  return &mThumbnailPanel;
}

int BridgeTestFixture::thumbnailRequestCount() const {
  return mThumbnailRequestCount;
}

int BridgeTestFixture::requestedThumbnailCount() const {
  return mRequestedThumbnailCount;
}

int BridgeTestFixture::lastActivatedThumbnail() const {
  return mLastActivatedThumbnail;
}

int BridgeTestFixture::lastPinRequest() const { return mLastPinRequest; }

void BridgeTestFixture::setFolderViewActive(bool active) {
  mFolderView.setActive(active);
  mWindowShell.setFolderViewActive(active);
  mOverlays.setFolderViewActive(active);
  mContextMenu.setFolderViewActive(active);
  mCrop.setFolderViewActive(active);
}

void BridgeTestFixture::setFullscreen(bool fullscreen) {
  mFolderView.setFullscreen(fullscreen);
  mWindowShell.setFullscreen(fullscreen);
  mOverlays.setFullscreen(fullscreen);
}

bool BridgeTestFixture::dropExternalFile(QQuickWindow *window, QPointF position,
                                         const QUrl &url) {
  if (!window) {
    qWarning() << "BridgeTestFixture: dropExternalFile() needs a window";
    return false;
  }
  QMimeData mimeData;
  mimeData.setUrls({url});
  constexpr Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
  QDragEnterEvent enter(position.toPoint(), actions, &mimeData, Qt::LeftButton,
                        Qt::NoModifier);
  QCoreApplication::sendEvent(window, &enter);
  if (!enter.isAccepted())
    return false;
  QDropEvent drop(position, actions, &mimeData, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(window, &drop);
  return drop.isAccepted();
}

//------------------------------------------------------------------------------
void BridgeTestFixture::toggleSmoothZoom() {
  mSettings.viewer.smoothZoom = !mSettings.viewer.smoothZoom;
  mSettingsBridge.apply(mSettings);
  mViewport.applySettings(mSettings);
}

void BridgeTestFixture::reapplySettings() { mSettingsBridge.apply(mSettings); }

void BridgeTestFixture::switchTheme() {
  mDark = !mDark;
  mThemeBridge.apply(testTheme(mDark));
}

void BridgeTestFixture::setShortcut(const QString &action,
                                    const QString &shortcut) {
  mDispatcher.setShortcut(action, shortcut);
  mActionBridge.refresh();
  mContextMenu.refreshShortcuts();
}

void BridgeTestFixture::showTestImage(int width, int height) {
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor::fromRgba(kTestImageColor));
  // Showing the same path again would keep the view (rotation handling).
  mViewport.closeImage();
  mViewport.showImage(std::make_shared<const QImage>(std::move(image)),
                      kTestImagePath);
  mOverlays.setDocumentDisplayed(true);
  mContextMenu.setImageDisplayed(true);
}

void BridgeTestFixture::closeTestImage() {
  mViewport.closeImage();
  mOverlays.setDocumentDisplayed(false);
  mContextMenu.setImageDisplayed(false);
}

ContextMenuModel *BridgeTestFixture::contextMenu() { return &mContextMenu; }

CropController *BridgeTestFixture::crop() { return &mCrop; }

QString BridgeTestFixture::lastCropRequest() const { return mLastCropRequest; }

int BridgeTestFixture::scriptSettingsRequests() const { return mScriptSettingsRequests; }

void BridgeTestFixture::toggleContextMenu(const QStringList &scripts, bool casFilter) {
  mContextMenu.setScripts(scripts);
  mContextMenu.setImageDisplayed(mViewport.hasImage());
  mContextMenu.setCasFilterActive(casFilter);
  mContextMenu.toggle();
}

void BridgeTestFixture::toggleCrop(QSize screenSize) {
  mCrop.setScreenSize(screenSize);
  mCrop.toggle();
}

void BridgeTestFixture::clearCropRequest() { mLastCropRequest.clear(); }

//------------------------------------------------------------------------------
void BridgeTestFixture::connectDialogs() {
  connect(mDialogs.confirmation(), &DialogSession::finished, this, [this]() {
    mLastDialogAnswer =
        u"confirm:%1"_s.arg(mDialogs.confirmation()->result().accepted ? 1 : 0);
  });
  connect(mDialogs.fileReplace(), &DialogSession::finished, this, [this]() {
    const FileReplaceDecision decision = mDialogs.fileReplace()->result();
    mLastDialogAnswer = u"replace:%1:%2:%3"_s.arg(decision.yes ? 1 : 0)
                            .arg(decision.all ? 1 : 0)
                            .arg(decision.cancel ? 1 : 0);
  });
  connect(mDialogs.resize(), &DialogSession::finished, this, [this]() {
    const std::optional<ResizeRequest> request = mDialogs.resize()->result();
    mLastDialogAnswer =
        request ? u"resize:%1x%2:%3:%4:%5"_s.arg(request->size.width())
                      .arg(request->size.height())
                      .arg(static_cast<int>(request->filter))
                      .arg(request->useUpscayl ? 1 : 0)
                      .arg(request->upscaylModel)
                : u"resize:none"_s;
  });
  connect(mDialogs.textInput(), &DialogSession::finished, this, [this]() {
    const TextInputResult result = mDialogs.textInput()->result();
    mLastDialogAnswer = u"text:%1:%2"_s.arg(result.accepted ? 1 : 0).arg(result.text);
  });
}

DialogCoordinator *BridgeTestFixture::dialogs() { return &mDialogs; }

//------------------------------------------------------------------------------
SettingsDialogController *BridgeTestFixture::settingsDialog() { return &mSettingsDialog; }

int BridgeTestFixture::settingsApplyCount() const { return mSettingsValues.applyCount; }

QString BridgeTestFixture::appliedShortcuts() const {
  QStringList entries;
  for (const ShortcutEntry &entry : mSettingsValues.appliedShortcuts)
    entries << entry.action + u'=' + entry.shortcut;
  return entries.join(u';');
}

QStringList BridgeTestFixture::storedScripts() const { return mShortcutScripts.scripts.keys(); }

int BridgeTestFixture::previewedThemeMode() const { return mSettingsValues.previewedThemeMode; }

int BridgeTestFixture::cacheClearCount() const { return mSettingsValues.cacheClearCount; }

void BridgeTestFixture::openSettings(int page) {
  mSettingsDialog.show(static_cast<SettingsEditorModel::Page>(page));
}

void BridgeTestFixture::closeSettings() {
  mSettingsEditor.shortcutEditor()->reject();
  mSettingsEditor.scriptEditor()->reject();
  mSettingsDialog.dismiss();
}

QVariant BridgeTestFixture::storedSetting(const QString &page, const QString &field) const {
  SettingsValues values = mSettingsValues.stored;
  const auto [metaObject, gadget] = settingsPage(values, page);
  const QMetaProperty property = settingsField(metaObject, field);
  if (!property.isValid()) {
    qWarning() << "Fixture.storedSetting: no field" << field << "on page" << page;
    return {};
  }
  return property.readOnGadget(gadget);
}

void BridgeTestFixture::setStoredSetting(const QString &page, const QString &field,
                                         const QVariant &value) {
  const auto [metaObject, gadget] = settingsPage(mSettingsValues.stored, page);
  const QMetaProperty property = settingsField(metaObject, field);
  if (!property.isValid() || !property.writeOnGadget(gadget, value))
    qWarning() << "Fixture.setStoredSetting: cannot set" << field << "on page" << page;
}

void BridgeTestFixture::resetSettingsStores() {
  mSettingsValues = FakeSettingsValueStore();
  mSettingsValues.stored = testSettingsValues();
  mSettingsValues.environmentValues = {.upscaylModels = {u"modelA"_s, u"modelB"_s},
                                       .defaultUpscaylModel = u"modelA"_s,
                                       .defaultZoomLevels = u"0.25,0.5,1,2,4"_s};
  mSettingsValues.cacheBytes = kTestCacheBytes;
  mShortcutScripts = FakeShortcutScriptStore();
  mShortcutScripts.actions = {u"nextImage"_s, u"prevImage"_s, u"zoomIn"_s};
  mShortcutScripts.live = {{.action = u"nextImage"_s, .shortcut = u"Right"_s},
                           {.action = u"prevImage"_s, .shortcut = u"Left"_s}};
  mShortcutScripts.defaults = {{.action = u"zoomIn"_s, .shortcut = u"+"_s}};
  mShortcutScripts.scripts = {{u"gimp"_s, {.command = u"gimp %file%"_s, .blocking = false}}};
}

QString BridgeTestFixture::lastDialogAnswer() const { return mLastDialogAnswer; }

bool BridgeTestFixture::requestConfirmation(const QString &title, const QString &message) {
  return mDialogs.confirmation()->start({.title = title, .message = message});
}

bool BridgeTestFixture::requestFileReplace(const QString &source, const QString &destination,
                                           int mode, bool multiple) {
  return mDialogs.fileReplace()->start({.sourcePath = source,
                                        .targetPath = destination,
                                        .mode = static_cast<FileReplaceMode>(mode),
                                        .multiple = multiple});
}

bool BridgeTestFixture::requestResize(QSize originalSize, QSize desktopSize,
                                      const QStringList &upscaylModels, bool useUpscayl) {
  return mDialogs.resize()->start({.originalSize = originalSize,
                                   .desktopSize = desktopSize,
                                   .upscaylModels = upscaylModels,
                                   .useUpscayl = useUpscayl,
                                   .upscaylModel = {}});
}

bool BridgeTestFixture::requestText(const QString &title, const QString &label,
                                    const QString &initialText) {
  return mDialogs.textInput()->start(
      {.title = title, .label = label, .initialText = initialText});
}

void BridgeTestFixture::clearDialogAnswer() { mLastDialogAnswer.clear(); }

void BridgeTestFixture::abandonDialogs() {
  mDialogs.confirmation()->abandon();
  mDialogs.fileReplace()->abandon();
  mDialogs.resize()->abandon();
  mDialogs.savePath()->abandon();
  mDialogs.textInput()->abandon();
}

void BridgeTestFixture::sendKey(QQuickWindow *window, int key) {
  if (!window) {
    qWarning() << "Fixture.sendKey: no window";
    return;
  }
  QTest::keyClick(window, static_cast<Qt::Key>(key));
}

void BridgeTestFixture::sendText(QQuickWindow *window, const QString &text) {
  if (!window) {
    qWarning() << "Fixture.sendText: no window";
    return;
  }
  for (const QChar character : text)
    QTest::keyClick(window, character.toLatin1());
}

QString BridgeTestFixture::artifactPath(const QString &fileName) const {
  return QDir(QCoreApplication::applicationDirPath()).filePath(fileName);
}

void BridgeTestFixture::clearInputLog() {
  mDispatcher.lastInvoked.clear();
  mDispatcher.lastKey = 0;
  mDispatcher.lastModifiers = 0;
  mDispatcher.lastWheelAngleDelta = QPoint();
  mDispatcher.mouseEventTypes.clear();
  mDispatcher.mouseButtons.clear();
}

//------------------------------------------------------------------------------
void BridgeTestFixture::configureThumbnailPanel(bool pinned, int position,
                                                bool extended) {
  mSettings.panel.enabled = true;
  mSettings.panel.pinned = pinned;
  mSettings.panel.position = static_cast<SettingsEnums::PanelPosition>(position);
  mSettings.panel.style = extended ? SettingsEnums::PanelStyle::Extended
                                   : SettingsEnums::PanelStyle::Simple;
  mSettings.panel.previewsSize = kTestPreviewsSize;
  mSettings.panel.hideDelayMs = kTestPanelHideDelayMs;
  mThumbnailPanel.applySettings(mSettings);
  mLastPinRequest = -1;
}

void BridgeTestFixture::allowThumbnailPanelCreation() {
  mThumbnailPanel.allowCreation();
}

void BridgeTestFixture::setPanelWindowSize(int width, int height) {
  mThumbnailPanel.setWindowSize(QSizeF(width, height));
}

void BridgeTestFixture::populateThumbnails(int count) {
  mUnansweredThumbnails.clear();
  mThumbnailRequestCount = 0;
  mRequestedThumbnailCount = 0;
  mLastActivatedThumbnail = -1;
  mThumbnails.populate(count);
}

void BridgeTestFixture::selectThumbnail(int index) {
  mThumbnails.select(index);
  mThumbnails.focusOn(index);
}

int BridgeTestFixture::deliverRequestedThumbnails() {
  const QList<int> indices = std::exchange(mUnansweredThumbnails, {});
  const int size = mThumbnails.requestConfig().pixelSize;
  for (const int index : indices) {
    QImage image(kTestThumbnailWidth, kTestThumbnailHeight,
                 QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor::fromHsv((index * kHueStride) % kHueSteps, kThumbnailSaturation,
                                   kThumbnailValue));
    mThumbnails.setThumbnail(
        index,
        ThumbnailEntry{.handle = {.image = image, .sourceSize = kTestSourceSize},
                       .name = u"item %1"_s.arg(index),
                       .info = u"%1 x %2"_s.arg(kTestThumbnailWidth).arg(kTestThumbnailHeight)},
        size);
  }
  return static_cast<int>(indices.count());
}

//------------------------------------------------------------------------------
// Folder view

// The requests go where QuickFolderViewActions sends them in the
// application; here they are recorded.
void BridgeTestFixture::connectFolderView() {
  mFolderGrid.setLabelFont(QGuiApplication::font());
  connect(&mFolderThumbnails, &ThumbnailListModel::thumbnailsNeeded, this,
          [this](const QList<int> &indices) {
            ++mFolderThumbnailRequestCount;
            mUnansweredFolderThumbnails.append(indices);
          });
  connect(&mFolderThumbnails, &ThumbnailListModel::activated, this,
          [this](int index) { mLastFolderRequest = u"activated:%1"_s.arg(index); });
  connect(&mFolderView, &FolderViewController::folderTreeRequested, this, [this]() {
    mFolderTree = std::make_unique<QFileSystemModel>();
    mFolderTree->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs);
    mFolderTree->setRootPath(QString());
    mFolderView.setFolderTree(mFolderTree.get());
  });
  connect(&mFolderView, &FolderViewController::sortingSelected, this,
          [this](SettingsEnums::SortingMode mode) {
            mLastFolderRequest = u"sorting:%1"_s.arg(static_cast<int>(mode));
          });
  connect(&mFolderView, &FolderViewController::folderSortingSelected, this,
          [this](SettingsEnums::SortingMode mode) {
            mLastFolderRequest = u"folderSorting:%1"_s.arg(static_cast<int>(mode));
          });
  connect(&mFolderView, &FolderViewController::nameFilterSelected, this,
          [this](const QString &text) { mLastFolderRequest = u"nameFilter:"_s + text; });
  connect(&mFolderView, &FolderViewController::formatFilterSelected, this,
          [this](const QStringList &extensions) {
            mLastFolderRequest = u"formats:"_s + extensions.join(u',');
          });
  connect(&mFolderView, &FolderViewController::directorySelected, this,
          [this](const QString &path) { mLastFolderRequest = u"directory:"_s + path; });
  connect(&mFolderGrid, &FolderGridController::typeAheadRequested, this,
          [this](const QString &text) { mLastFolderRequest = u"typeAhead:"_s + text; });
  connect(&mFolderGrid, &FolderGridController::contextMenuRequested, this,
          [this]() { mLastFolderRequest = u"contextMenu"_s; });
  connect(&mFolderGrid, &FolderGridController::batchConversionRequested, this,
          [this]() { mLastFolderRequest = u"batch"_s; });
  connect(&mFolderGrid, &FolderGridController::openSelectedRequested, this,
          [this]() { mLastFolderRequest = u"openSelected"_s; });
  connect(&mFolderGrid, &FolderGridController::urlsDropped, this,
          [this](const QList<QUrl> &, QObject *, int index, Qt::DropAction action) {
            mLastFolderRequest = u"drop:%1:%2"_s.arg(index).arg(static_cast<int>(action));
          });
}

FolderViewController *BridgeTestFixture::folderView() {
  return &mFolderView;
}

QString BridgeTestFixture::lastFolderRequest() const {
  return mLastFolderRequest;
}

int BridgeTestFixture::folderThumbnailRequestCount() const {
  return mFolderThumbnailRequestCount;
}

void BridgeTestFixture::populateFolder(int count, int dirCount, const QString &path) {
  mUnansweredFolderThumbnails.clear();
  mFolderThumbnailRequestCount = 0;
  mLastFolderRequest.clear();
  mFolderThumbnails.populate(count);
  mFolderThumbnails.setDirCount(dirCount);
  mFolderView.setDirectoryPath(path);
}

int BridgeTestFixture::deliverFolderThumbnails() {
  const QList<int> indices = std::exchange(mUnansweredFolderThumbnails, {});
  const int size = mFolderThumbnails.requestConfig().pixelSize;
  for (const int index : indices) {
    QImage image(kTestThumbnailWidth, kTestThumbnailHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor::fromHsv((index * kHueStride) % kHueSteps, kThumbnailSaturation,
                               kThumbnailValue));
    mFolderThumbnails.setThumbnail(
        index,
        ThumbnailEntry{.handle = {.image = image, .sourceSize = kTestSourceSize},
                       .name = u"file %1"_s.arg(index),
                       .info = u"%1 x %2"_s.arg(kTestThumbnailWidth).arg(kTestThumbnailHeight)},
        size);
  }
  return static_cast<int>(indices.count());
}

void BridgeTestFixture::clearFolderRequest() {
  mLastFolderRequest.clear();
}

void BridgeTestFixture::panelPointerMoved(QPointF position, int buttons) {
  mThumbnailPanel.pointerMoved(position, Qt::MouseButtons::fromInt(buttons));
}

//------------------------------------------------------------------------------
void BridgeTestFixture::toggleCopy() { mOverlays.toggleCopy(); }

void BridgeTestFixture::toggleMove() { mOverlays.toggleMove(); }

void BridgeTestFixture::toggleImageInfo() { mOverlays.toggleImageInfo(); }

void BridgeTestFixture::toggleRename(const QString &currentName) {
  mOverlays.toggleRename(currentName);
}

void BridgeTestFixture::toggleColorAdjustments() {
  mOverlays.toggleColorAdjustments();
}

void BridgeTestFixture::toggleCasSettings() { mOverlays.toggleCasSettings(); }

void BridgeTestFixture::setSaveConfirmVisible(bool visible) {
  mOverlays.setSaveConfirmVisible(visible);
}

void BridgeTestFixture::showMessage(const QString &text) {
  mOverlays.messages()->showMessage(text);
}

void BridgeTestFixture::setMetadataEntries(int count) {
  MetadataEntries entries;
  for (int i = 0; i < count; ++i)
    entries.append({u"Name %1"_s.arg(i), u"Value %1"_s.arg(i)});
  mOverlays.setMetadata(entries);
}

void BridgeTestFixture::pointerMoved(QPointF position) {
  mOverlays.pointerMoved(position, true);
}

void BridgeTestFixture::closeOverlays() {
  for (OverlayState *state :
       {mOverlays.imageInfo(), mOverlays.saveConfirm(), mOverlays.copy(),
        mOverlays.rename(), mOverlays.colorAdjustments(), mOverlays.casSettings()})
    state->close();
  mOverlays.messages()->hideNotifications();
  mLastFileRequest.clear();
}
