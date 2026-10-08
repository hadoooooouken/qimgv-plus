#include "bridgetestfixture.h"

#include <QColor>
#include <QKeyEvent>
#include <QWheelEvent>

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
  settings.panel.position = SettingsEnums::PanelPosition::Bottom;
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
      mActionBridge(mDispatcher) {}

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

//------------------------------------------------------------------------------
void BridgeTestFixture::toggleSmoothZoom() {
  mSettings.viewer.smoothZoom = !mSettings.viewer.smoothZoom;
  mSettingsBridge.apply(mSettings);
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
