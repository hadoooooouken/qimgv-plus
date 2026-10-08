#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Theme snapshot published by ThemeBridge. The application side fills it from
// Settings::colorScheme() and the application font
// (gui/quick/adapters/bridgesnapshots.h).

// Every colour of ColorScheme (themestore.h), base and extended, under
// camelCase names: ColorScheme::folderview_topbar is folderViewTopBar.
struct ThemeColors {
  Q_GADGET
  QML_VALUE_TYPE(themeColors)
  QML_UNCREATABLE("Provided by Theme.colors")
  Q_PROPERTY(QColor background MEMBER background)
  Q_PROPERTY(QColor backgroundFullscreen MEMBER backgroundFullscreen)
  Q_PROPERTY(QColor text MEMBER text)
  Q_PROPERTY(QColor icons MEMBER icons)
  Q_PROPERTY(QColor folderIcons MEMBER folderIcons)
  Q_PROPERTY(QColor thumbnailFolderIcons MEMBER thumbnailFolderIcons)
  Q_PROPERTY(QColor widget MEMBER widget)
  Q_PROPERTY(QColor widgetBorder MEMBER widgetBorder)
  Q_PROPERTY(QColor accent MEMBER accent)
  Q_PROPERTY(QColor folderView MEMBER folderView)
  Q_PROPERTY(QColor folderViewTopBar MEMBER folderViewTopBar)
  Q_PROPERTY(QColor thumbPanel MEMBER thumbPanel)
  Q_PROPERTY(QColor scrollbar MEMBER scrollbar)
  Q_PROPERTY(QColor scrollbarHover MEMBER scrollbarHover)
  Q_PROPERTY(QColor overlayText MEMBER overlayText)
  Q_PROPERTY(QColor overlay MEMBER overlay)
  Q_PROPERTY(QColor statusPending MEMBER statusPending)
  Q_PROPERTY(QColor statusError MEMBER statusError)
  Q_PROPERTY(QColor statusProcessing MEMBER statusProcessing)
  Q_PROPERTY(QColor statusSuccess MEMBER statusSuccess)
  Q_PROPERTY(QColor danger MEMBER danger)
  Q_PROPERTY(QColor trash MEMBER trash)
  Q_PROPERTY(QColor textHc2 MEMBER textHc2)
  Q_PROPERTY(QColor textHc MEMBER textHc)
  Q_PROPERTY(QColor textLc MEMBER textLc)
  Q_PROPERTY(QColor textLc2 MEMBER textLc2)
  Q_PROPERTY(QColor button MEMBER button)
  Q_PROPERTY(QColor buttonHover MEMBER buttonHover)
  Q_PROPERTY(QColor buttonPressed MEMBER buttonPressed)
  Q_PROPERTY(QColor panelButton MEMBER panelButton)
  Q_PROPERTY(QColor panelButtonHover MEMBER panelButtonHover)
  Q_PROPERTY(QColor panelButtonPressed MEMBER panelButtonPressed)
  Q_PROPERTY(QColor folderViewHc MEMBER folderViewHc)
  Q_PROPERTY(QColor folderViewHc2 MEMBER folderViewHc2)
  Q_PROPERTY(QColor thumbPanelHc MEMBER thumbPanelHc)
  Q_PROPERTY(QColor thumbPanelHc2 MEMBER thumbPanelHc2)
  Q_PROPERTY(QColor thumbPanelText MEMBER thumbPanelText)
  Q_PROPERTY(QColor folderViewButtonHover MEMBER folderViewButtonHover)
  Q_PROPERTY(QColor folderViewButtonPressed MEMBER folderViewButtonPressed)
  Q_PROPERTY(QColor inputFieldFocus MEMBER inputFieldFocus)
  Q_PROPERTY(QColor comboField MEMBER comboField)
  Q_PROPERTY(QColor comboFieldHover MEMBER comboFieldHover)
  Q_PROPERTY(QColor comboFieldPressed MEMBER comboFieldPressed)
  Q_PROPERTY(QColor comboFieldBorder MEMBER comboFieldBorder)

public:
  QColor background;
  QColor backgroundFullscreen;
  QColor text;
  QColor icons;
  QColor folderIcons;
  QColor thumbnailFolderIcons;
  QColor widget;
  QColor widgetBorder;
  QColor accent;
  QColor folderView;
  QColor folderViewTopBar;
  QColor thumbPanel;
  QColor scrollbar;
  QColor scrollbarHover;
  QColor overlayText;
  QColor overlay;
  QColor statusPending;
  QColor statusError;
  QColor statusProcessing;
  QColor statusSuccess;
  QColor danger;
  QColor trash;
  QColor textHc2;
  QColor textHc;
  QColor textLc;
  QColor textLc2;
  QColor button;
  QColor buttonHover;
  QColor buttonPressed;
  QColor panelButton;
  QColor panelButtonHover;
  QColor panelButtonPressed;
  QColor folderViewHc;
  QColor folderViewHc2;
  QColor thumbPanelHc;
  QColor thumbPanelHc2;
  QColor thumbPanelText;
  QColor folderViewButtonHover;
  QColor folderViewButtonPressed;
  QColor inputFieldFocus;
  QColor comboField;
  QColor comboFieldHover;
  QColor comboFieldPressed;
  QColor comboFieldBorder;

  friend bool operator==(const ThemeColors &, const ThemeColors &) = default;
};

// Application font and the derived text styles (UiMetrics::Typography):
// compact, section and large use the small, section and large point sizes.
struct ThemeFonts {
  Q_GADGET
  QML_VALUE_TYPE(themeFonts)
  QML_UNCREATABLE("Provided by Theme.fonts")
  Q_PROPERTY(QFont base MEMBER base)
  Q_PROPERTY(QFont compact MEMBER compact)
  Q_PROPERTY(QFont section MEMBER section)
  Q_PROPERTY(QFont large MEMBER large)

public:
  QFont base;
  QFont compact;
  QFont section;
  QFont large;

  friend bool operator==(const ThemeFonts &, const ThemeFonts &) = default;
};

// Complete set published by ThemeBridge in one step.
struct ThemeSnapshot {
  ThemeColors colors;
  ThemeFonts fonts;
  bool dark = false;
  QString iconFontFamily;

  friend bool operator==(const ThemeSnapshot &, const ThemeSnapshot &) = default;
};
