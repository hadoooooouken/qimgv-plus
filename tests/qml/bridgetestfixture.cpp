#include "bridgetestfixture.h"

#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDropEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>

#include <memory>

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

// Distinct backgrounds so that a theme switch changes ThemeColors.
constexpr QRgb kDarkBackground = 0xff1f1f1f;
constexpr QRgb kLightBackground = 0xfff2f2f2;
const QString kIconFontFamily = u"FluentSystemIcons-Custom"_s;
const QString kTestImagePath = u"C:/fixture/test.png"_s;
constexpr QRgb kTestImageColor = 0xff3080c0;

ThemeSnapshot testTheme(bool dark) {
  ThemeSnapshot theme;
  theme.colors.background = QColor::fromRgb(dark ? kDarkBackground : kLightBackground);
  theme.dark = dark;
  theme.iconFontFamily = kIconFontFamily;
  return theme;
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

void FakeActionDispatcher::setShortcut(const QString &action,
                                       const QString &shortcut) {
  mShortcuts.insert(action, shortcut);
}

//------------------------------------------------------------------------------
BridgeTestFixture::BridgeTestFixture(QObject *parent)
    : QObject(parent), mSettings(testSettings()),
      mSettingsBridge(mSettings), mThemeBridge(testTheme(mDark)),
      mActionBridge(mDispatcher), mViewport(mSettings) {
  connect(&mWindowShell, &MainWindowShell::urlsDropped, this,
          [this](const QList<QUrl> &urls) { mLastDroppedUrls = urls; });
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

void BridgeTestFixture::setFolderViewActive(bool active) {
  mWindowShell.setFolderViewActive(active);
}

void BridgeTestFixture::setFullscreen(bool fullscreen) {
  mWindowShell.setFullscreen(fullscreen);
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
}

void BridgeTestFixture::showTestImage(int width, int height) {
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor::fromRgba(kTestImageColor));
  // Showing the same path again would keep the view (rotation handling).
  mViewport.closeImage();
  mViewport.showImage(std::make_shared<const QImage>(std::move(image)),
                      kTestImagePath);
}

void BridgeTestFixture::clearInputLog() {
  mDispatcher.lastInvoked.clear();
  mDispatcher.lastKey = 0;
  mDispatcher.lastModifiers = 0;
  mDispatcher.lastWheelAngleDelta = QPoint();
  mDispatcher.mouseEventTypes.clear();
  mDispatcher.mouseButtons.clear();
}
