#include "bridgetestfixture.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDropEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>

#include <memory>
#include <utility>

#include "gui/quick/adapters/themesnapshotbuilder.h"
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
  return settings;
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
      mCrop(mSettings) {
  mThumbnailPanel.setLabelFont(QGuiApplication::font());
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
  mWindowShell.setFolderViewActive(active);
  mOverlays.setFolderViewActive(active);
  mContextMenu.setFolderViewActive(active);
  mCrop.setFolderViewActive(active);
}

void BridgeTestFixture::setFullscreen(bool fullscreen) {
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
