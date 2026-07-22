#include "settings.h"

Settings *settings = nullptr;

Settings::Settings(QObject *parent) : QObject(parent) {
  QString appDirPath = QApplication::applicationDirPath();
  QString confPath = appDirPath + "/conf";
  QDir confDir(confPath);

  bool isWritable = true;
  if (!confDir.exists()) {
    isWritable = confDir.mkpath(confPath);
  }
  if (isWritable) {
    QFile testFile(confPath + "/.write_test");
    if (testFile.open(QIODevice::WriteOnly)) {
      testFile.close();
      testFile.remove();
    } else {
      isWritable = false;
    }
  }

  if (!isWritable) {
    confPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  }

  mConfDir = std::make_unique<QDir>(confPath);
  mConfDir->mkpath(mConfDir->absolutePath());

  settingsConf = std::make_unique<QSettings>(mConfDir->absolutePath() + "/" +
                                    qApp->applicationName() + ".ini",
                               QSettings::IniFormat);
  stateConf = std::make_unique<QSettings>(mConfDir->absolutePath() + "/savedState.ini",
                            QSettings::IniFormat);
  themeConf = std::make_unique<QSettings>(mConfDir->absolutePath() + "/theme.ini",
                            QSettings::IniFormat);
  initCache();
}
//------------------------------------------------------------------------------
void Settings::initCache() {
  mCachedUseUpscayl.store(settingsConf->value("useUpscayl", false).toBool(), std::memory_order_relaxed);
  mCachedResizeUseUpscayl.store(settingsConf->value("resizeUseUpscayl", false).toBool(), std::memory_order_relaxed);
  mCachedPreloadUpscayl.store(settingsConf->value("preloadUpscayl", false).toBool(), std::memory_order_relaxed);
  mCachedUpscaylLimitEnabled.store(settingsConf->value("upscaylLimitEnabled", false).toBool(), std::memory_order_relaxed);
  mCachedUpscaylLimitValue.store(settingsConf->value("upscaylLimitValue", 200).toInt(), std::memory_order_relaxed);

  int limit = settingsConf->value("memoryAllocationLimit", 2048).toInt();
  if (limit < 512) limit = 512;
  else if (limit > 8192) limit = 8192;
  mCachedMemoryAllocationLimit.store(limit, std::memory_order_relaxed);

  mCachedThumbnailResolution.store(settingsConf->value("thumbnailResolution", 256).toInt(), std::memory_order_relaxed);
  mCachedColorManagementEnabled.store(settingsConf->value("colorManagementEnabled", false).toBool(), std::memory_order_relaxed);
  mCachedJxlAnimation.store(settingsConf->value("jxlAnimation", false).toBool(), std::memory_order_relaxed);

  mCachedPngSaveQuality.store(std::clamp(settingsConf->value("pngSaveQuality", 3).toInt(), 0, 9), std::memory_order_relaxed);
  mCachedJPEGSaveQuality.store(std::clamp(settingsConf->value("JPEGSaveQuality", 95).toInt(), 0, 100), std::memory_order_relaxed);
  mCachedModernSaveQuality.store(std::clamp(settingsConf->value("modernSaveQuality", 90).toInt(), 0, 100), std::memory_order_relaxed);

  {
    QWriteLocker locker(&mProfileLock);
    mCachedMonitorColorProfileType = settingsConf->value("monitorColorProfileType", "System").toString();
    mCachedMonitorColorProfilePath = settingsConf->value("monitorColorProfilePath", "").toString();
  }

  {
    QWriteLocker locker(&mExcludedPathsLock);
    mCachedExcludedCachePaths = settingsConf->value("excludedCachePaths", "").toString();
    mCachedExcludedCachePathsList = mCachedExcludedCachePaths.split(';');
  }
}
//------------------------------------------------------------------------------
Settings::~Settings() {
  saveTheme();
}
//------------------------------------------------------------------------------
Settings *Settings::getInstance() {
  if (!settings) {
    settings = new Settings();
    settings->setupCache();
    settings->loadTheme();
  }
  return settings;
}
//------------------------------------------------------------------------------
void Settings::setupCache() {
  QString cachePath;
  QString thumbPath;
  QString appDirPath = QDir::cleanPath(QApplication::applicationDirPath());
  QString confPath = QDir::cleanPath(mConfDir->absolutePath());

  if (confPath == QDir::cleanPath(appDirPath + "/conf")) {
    cachePath = appDirPath + "/cache";
    thumbPath = appDirPath + "/thumbnails";
  } else {
    cachePath = mConfDir->absolutePath() + "/cache";
    thumbPath = mConfDir->absolutePath() + "/thumbnails";
  }

  mTmpDir = std::make_unique<QDir>(cachePath);
  mTmpDir->mkpath(mTmpDir->absolutePath());
  mThumbCacheDir = std::make_unique<QDir>(thumbPath);
  mThumbCacheDir->mkpath(mThumbCacheDir->absolutePath());
}
//------------------------------------------------------------------------------
void Settings::sync() {
  settings->settingsConf->sync();
  settings->stateConf->sync();
}
//------------------------------------------------------------------------------
QString Settings::thumbnailCacheDir() { return mThumbCacheDir->path() + "/"; }
//------------------------------------------------------------------------------
int Settings::thumbnailResolution() {
  return mCachedThumbnailResolution.load(std::memory_order_relaxed);
}

void Settings::setThumbnailResolution(int size) {
  size = qBound(128, size, 512);
  settings->settingsConf->setValue("thumbnailResolution", size);
  mCachedThumbnailResolution.store(size, std::memory_order_relaxed);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
QString Settings::tmpDir() { return mTmpDir->path() + "/"; }
//------------------------------------------------------------------------------
// this here is temporarily, will be moved to some sort of theme manager class
void Settings::loadStylesheet() {
  // stylesheet template file
  static QString styleSheetTemplate;
  if (styleSheetTemplate.isEmpty()) {
    QFile file(":/res/styles/style-template.qss");
    if (file.open(QFile::ReadOnly)) {
      styleSheetTemplate = QLatin1String(file.readAll());
    }
  }
  if (!styleSheetTemplate.isEmpty()) {
    QString styleSheet = styleSheetTemplate;

    // --- color scheme ---------------------------------------------
    auto colors = settings->colorScheme();
    // tint color for system windows
    QPalette p;
    QColor sys_text = p.text().color();
    QColor sys_window = p.window().color();

    ThemeMode themeModeVal = settings->themeMode();
    bool isDark = false;
    if (themeModeVal == THEME_AUTO) {
      if (sys_window.valueF() <= 0.45f) {
        isDark = true;
      }
    } else if (themeModeVal == THEME_DARK) {
      isDark = true;
    }

    if (isDark) {
      sys_window = QColor(37, 37, 37);
      sys_text = QColor(220, 220, 220);

      QPalette darkPalette;
      darkPalette.setColor(QPalette::Window, QColor(37, 37, 37));
      darkPalette.setColor(QPalette::WindowText, QColor(220, 220, 220));
      darkPalette.setColor(QPalette::Base, QColor(55, 55, 55));
      darkPalette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
      darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
      darkPalette.setColor(QPalette::ToolTipText, Qt::white);
      darkPalette.setColor(QPalette::Text, QColor(220, 220, 220));
      darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
      darkPalette.setColor(QPalette::Button, QColor(45, 45, 45));
      darkPalette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
      darkPalette.setColor(QPalette::BrightText, Qt::red);
      darkPalette.setColor(QPalette::Link, colors.accent);
      darkPalette.setColor(QPalette::Highlight, colors.accent);
      darkPalette.setColor(QPalette::HighlightedText, Qt::white);
      darkPalette.setColor(QPalette::Mid, QColor(110, 110, 110));
      qApp->setPalette(darkPalette);
    } else {
      sys_window = QColor(245, 245, 245);
      sys_text = QColor(30, 30, 30);

      QPalette lightPalette;
      lightPalette.setColor(QPalette::Window, QColor(245, 245, 245));
      lightPalette.setColor(QPalette::WindowText, QColor(30, 30, 30));
      lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
      lightPalette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
      lightPalette.setColor(QPalette::ToolTipBase, QColor(30, 30, 30));
      lightPalette.setColor(QPalette::ToolTipText, Qt::white);
      lightPalette.setColor(QPalette::Text, QColor(30, 30, 30));
      lightPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
      lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
      lightPalette.setColor(QPalette::ButtonText, QColor(30, 30, 30));
      lightPalette.setColor(QPalette::BrightText, Qt::red);
      lightPalette.setColor(QPalette::Link, colors.accent);
      lightPalette.setColor(QPalette::Highlight, colors.accent);
      lightPalette.setColor(QPalette::HighlightedText, Qt::white);
      qApp->setPalette(lightPalette);
    }
    QColor sys_window_tinted, sys_window_tinted_lc, sys_window_tinted_lc2,
        sys_window_tinted_hc, sys_window_tinted_hc2;
    if (sys_window.valueF() <= 0.45f) {
      // dark system theme
      sys_window_tinted_lc2.setHsv(sys_window.hue(), sys_window.saturation(),
                                   sys_window.value() + 6);
      sys_window_tinted_lc.setHsv(sys_window.hue(), sys_window.saturation(),
                                  sys_window.value() + 14);
      sys_window_tinted.setHsv(sys_window.hue(), sys_window.saturation(),
                               sys_window.value() + 20);
      sys_window_tinted_hc.setHsv(sys_window.hue(), sys_window.saturation(),
                                  sys_window.value() + 35);
      sys_window_tinted_hc2.setHsv(sys_window.hue(), sys_window.saturation(),
                                   sys_window.value() + 50);
    } else {
      // light system theme
      sys_window_tinted_lc2.setHsv(sys_window.hue(), sys_window.saturation(),
                                   sys_window.value() - 6);
      sys_window_tinted_lc.setHsv(sys_window.hue(), sys_window.saturation(),
                                  sys_window.value() - 14);
      sys_window_tinted.setHsv(sys_window.hue(), sys_window.saturation(),
                               sys_window.value() - 20);
      sys_window_tinted_hc.setHsv(sys_window.hue(), sys_window.saturation(),
                                  sys_window.value() - 35);
      sys_window_tinted_hc2.setHsv(sys_window.hue(), sys_window.saturation(),
                                   sys_window.value() - 50);
    }

    // --- widget sizes ---------------------------------------------
    auto fnt = QGuiApplication::font();
    QFontMetrics fm(fnt);
    int font_small = qMax((int)(fnt.pointSize() * 0.9f), 8);
    int font_large = (int)(fnt.pointSize() * 1.8f);
    int text_height = fm.height();
    int text_padding = (int)(text_height * 0.10f);
    int text_padding_small = (int)(text_height * 0.05f);
    int text_padding_large = (int)(text_height * 0.25f);

    // folderview top panel item sizes
    int top_panel_v_margin = 4;
    // ensure at least 4px so its not too thin
    int top_panel_text_padding = qMax(text_padding, 4);
    // scale with font, 38px base size
    int top_panel_height = qMax(
        (text_height + top_panel_text_padding * 2 + top_panel_v_margin * 2),
        38);

    // overlay headers
    int overlay_header_margin = 2;
    // 32px base size
    int overlay_header_size = qMax(text_height + text_padding * 2, 30);

    int button_height = text_height + text_padding_large * 2;

    // pseudo-dpi to scale some widget widths
    int text_height_base = 22;
    qreal pDpr = qMax(((qreal)(text_height) / text_height_base), 1.0);
    int context_menu_width = 212 * pDpr;
    int context_menu_button_height = 32 * pDpr;
    int rename_overlay_width = 380 * pDpr;

    // qDebug()<< "dpr=" << qApp->devicePixelRatio() << "pDpr=" << pDpr;

    // --- write variables into stylesheet --------------------------
    styleSheet.replace("%font_small%", QString::number(font_small) + "pt");
    styleSheet.replace("%font_large%", QString::number(font_large) + "pt");
    styleSheet.replace("%button_height%",
                       QString::number(button_height) + "px");
    styleSheet.replace("%top_panel_height%",
                       QString::number(top_panel_height) + "px");
    styleSheet.replace("%overlay_header_size%",
                       QString::number(overlay_header_size) + "px");
    styleSheet.replace("%context_menu_width%",
                       QString::number(context_menu_width) + "px");
    styleSheet.replace("%context_menu_button_height%",
                       QString::number(context_menu_button_height) + "px");
    styleSheet.replace("%rename_overlay_width%",
                       QString::number(rename_overlay_width) + "px");

    styleSheet.replace("%icontheme%", isDark ? "light" : "dark");
    styleSheet.replace("%contextmenu_border_radius%", "8px");
    styleSheet.replace("%sys_window%", sys_window.name());
    styleSheet.replace("%sys_window_tinted%", sys_window_tinted.name());
    styleSheet.replace("%sys_window_tinted_lc%", sys_window_tinted_lc.name());
    styleSheet.replace("%sys_window_tinted_lc2%", sys_window_tinted_lc2.name());
    styleSheet.replace("%sys_window_tinted_hc%", sys_window_tinted_hc.name());
    styleSheet.replace("%sys_window_tinted_hc2%", sys_window_tinted_hc2.name());
    styleSheet.replace("%sys_text_secondary_rgba%",
                       "rgba(" + QString::number(sys_text.red()) + "," +
                           QString::number(sys_text.green()) + "," +
                           QString::number(sys_text.blue()) + ",50%)");

    styleSheet.replace("%button%", colors.button.name());
    styleSheet.replace("%button_hover%", colors.button_hover.name());
    styleSheet.replace("%button_pressed%", colors.button_pressed.name());
    styleSheet.replace("%panel_button%", colors.panel_button.name());
    styleSheet.replace("%panel_button_hover%",
                       colors.panel_button_hover.name());
    styleSheet.replace("%panel_button_pressed%",
                       colors.panel_button_pressed.name());
    styleSheet.replace("%widget%", colors.widget.name());
    styleSheet.replace("%widget_border%", colors.widget_border.name());
    styleSheet.replace("%folderview%", colors.folderview.name());
    styleSheet.replace("%folderview_topbar%", colors.folderview_topbar.name());
    styleSheet.replace("%thumbpanel%", colors.thumbpanel.name());
    styleSheet.replace("%thumbpanel_hc%", colors.thumbpanel_hc.name());
    styleSheet.replace("%thumbpanel_hc2%", colors.thumbpanel_hc2.name());
    styleSheet.replace("%folderview_hc%", colors.folderview_hc.name());
    styleSheet.replace("%folderview_hc2%", colors.folderview_hc2.name());
    styleSheet.replace("%accent_light%", colors.accent.lighter(130).name());
    styleSheet.replace("%accent%", colors.accent.name());
    styleSheet.replace("%input_field_focus%", colors.input_field_focus.name());
    styleSheet.replace("%overlay%", colors.overlay.name());
    styleSheet.replace("%icons%", colors.icons.name());
    styleSheet.replace("%text_hc2%", colors.text_hc2.name());
    styleSheet.replace("%text_hc%", colors.text_hc.name());
    styleSheet.replace("%text%", colors.text.name());
    styleSheet.replace("%overlay_text%", colors.overlay_text.name());
    styleSheet.replace("%text_lc%", colors.text_lc.name());
    styleSheet.replace("%text_lc2%", colors.text_lc2.name());
    styleSheet.replace("%scrollbar%", colors.scrollbar.name());
    styleSheet.replace("%scrollbar_hover%", colors.scrollbar_hover.name());
    styleSheet.replace("%folderview_button_hover%",
                       colors.folderview_button_hover.name());
    styleSheet.replace("%folderview_button_pressed%",
                       colors.folderview_button_pressed.name());
    styleSheet.replace("%text_secondary_rgba%",
                       "rgba(" + QString::number(colors.text.red()) + "," +
                           QString::number(colors.text.green()) + "," +
                           QString::number(colors.text.blue()) + ",62%)");
    styleSheet.replace("%accent_hover_rgba%",
                       "rgba(" + QString::number(colors.accent.red()) + "," +
                           QString::number(colors.accent.green()) + "," +
                           QString::number(colors.accent.blue()) + ",65%)");
    styleSheet.replace("%overlay_rgba%",
                       "rgba(" + QString::number(colors.overlay.red()) + "," +
                           QString::number(colors.overlay.green()) + "," +
                           QString::number(colors.overlay.blue()) + ",90%)");
    styleSheet.replace(
        "%fv_backdrop_rgba%",
        "rgba(" + QString::number(colors.folderview_hc2.red()) + "," +
            QString::number(colors.folderview_hc2.green()) + "," +
            QString::number(colors.folderview_hc2.blue()) + ",80%)");
    styleSheet.replace("%thumbpanel_rgba%",
                       "rgba(" + QString::number(colors.thumbpanel.red()) +
                           "," + QString::number(colors.thumbpanel.green()) +
                           "," + QString::number(colors.thumbpanel.blue()) +
                           "," + QString::number(colors.thumbpanel.alphaF()) +
                           ")");
    // do not show separator line if topbar color matches folderview
    if (colors.folderview != colors.folderview_topbar)
      styleSheet.replace("%topbar_border_rgba%", "rgba(0,0,0,14%)");
    else
      styleSheet.replace("%topbar_border_rgba%", colors.folderview.name());

    // --- apply -------------------------------------------------
    qApp->setStyleSheet(styleSheet);
  }
}
//------------------------------------------------------------------------------
void Settings::loadTheme() {
  ThemeMode mode = themeMode();
  ColorSchemes baseSchemeName = COLORS_DARK; // Default to dark

  if (mode == THEME_AUTO) {
    QPalette p;
    if (p.window().color().valueF() <= 0.45f) {
      baseSchemeName = COLORS_DARK;
    } else {
      baseSchemeName = COLORS_LIGHT;
    }
  } else if (mode == THEME_DARK) {
    baseSchemeName = COLORS_DARK;
  } else if (mode == THEME_LIGHT) {
    baseSchemeName = COLORS_LIGHT;
  }

  ColorScheme baseScheme = ThemeStore::colorScheme(baseSchemeName);

  themeConf->beginGroup("Colors");
  mHasCustomAccent = themeConf->contains("accent");
  QColor customAccent = mHasCustomAccent
                            ? QColor(themeConf->value("accent").toString())
                            : QColor();
  themeConf->endGroup();

  BaseColorScheme base;
  base.background = useBlackBackground() ? QColor("#000000") : baseScheme.background;
  base.background_fullscreen = useBlackBackground() ? QColor("#000000") : baseScheme.background_fullscreen;
  base.text = baseScheme.text;
  base.icons = baseScheme.icons;
  base.folder_icons = baseScheme.folder_icons;
  base.thumbnail_folder_icons = baseScheme.thumbnail_folder_icons;
  base.widget = baseScheme.widget;
  base.widget_border = baseScheme.widget_border;
  base.accent = customAccent.isValid() ? customAccent : baseScheme.accent;
  base.folderview = baseScheme.folderview;
  base.folderview_topbar = baseScheme.folderview_topbar;
  base.thumbpanel = useBlackBackground() ? QColor("#000000") : baseScheme.thumbpanel;
  base.thumbpanel.setAlphaF(thumbnailOpacity());
  base.scrollbar = baseScheme.scrollbar;
  base.overlay = baseScheme.overlay;
  base.overlay_text = baseScheme.overlay_text;
  base.status_pending = baseScheme.status_pending;
  base.status_error = baseScheme.status_error;
  base.status_processing = baseScheme.status_processing;
  base.status_success = baseScheme.status_success;
  base.danger = baseScheme.danger;
  base.trash = baseScheme.trash;
  base.tid = baseScheme.tid;

  setColorScheme(ColorScheme(base));
}
void Settings::saveTheme() {
  themeConf->beginGroup("Colors");
  if (mHasCustomAccent) {
    themeConf->setValue("accent", mColorScheme.accent.name());
  } else {
    themeConf->remove("accent");
  }
  themeConf->endGroup();
}
void Settings::clearCustomAccent() {
  mHasCustomAccent = false;
  saveTheme();
  loadTheme();
}
//------------------------------------------------------------------------------
const ColorScheme &Settings::colorScheme() { return mColorScheme; }
//------------------------------------------------------------------------------
void Settings::setColorScheme(ColorScheme scheme) {
  mColorScheme = scheme;
  loadStylesheet();
}
//------------------------------------------------------------------------------
void Settings::setColorTid(int tid) { mColorScheme.tid = tid; }
//------------------------------------------------------------------------------
QList<QByteArray> Settings::supportedFormats() {
  auto formats = QImageReader::supportedImageFormats();
  formats << "jfif" << "ai";
  return formats;
}
//------------------------------------------------------------------------------
// (for open/save dialogs, as a single string)
// example:  "Images (*.jpg, *.png)"
QString Settings::supportedFormatsFilter() {
  QString filters;
  auto formats = supportedFormats();
  filters.append("Supported files (");
  for (int i = 0; i < formats.count(); i++)
    filters.append("*." + QString(formats.at(i)) + " ");
  filters.append(")");
  return filters;
}
//------------------------------------------------------------------------------
QString Settings::supportedFormatsRegex() {
  QString filter;
  QList<QByteArray> formats = supportedFormats();
  filter.append(".*\\.(");
  for (int i = 0; i < formats.count(); i++)
    filter.append(QString(formats.at(i)) + "|");
  filter.chop(1);
  filter.append(")$");
  return filter;
}
//------------------------------------------------------------------------------
// returns list of mime types
QStringList Settings::supportedMimeTypes() {
  QStringList filters;
  QList<QByteArray> mimeTypes = QImageReader::supportedMimeTypes();
  for (int i = 0; i < mimeTypes.count(); i++) {
    filters << QString(mimeTypes.at(i));
  }
  return filters;
}
//------------------------------------------------------------------------------
bool Settings::useSystemColorScheme() {
  return settings->settingsConf->value("useSystemColorScheme", false).toBool();
}

void Settings::setUseSystemColorScheme(bool mode) {
  settings->settingsConf->setValue("useSystemColorScheme", mode);
}

ThemeMode Settings::themeMode() {
  int mode = settings->settingsConf->value("themeMode", static_cast<int>(THEME_AUTO)).toInt();
  if (mode < 0 || mode > 2)
    mode = static_cast<int>(THEME_AUTO);
  return static_cast<ThemeMode>(mode);
}

void Settings::setThemeMode(ThemeMode mode) {
  settings->settingsConf->setValue("themeMode", static_cast<int>(mode));
}

qreal Settings::thumbnailOpacity() {
  bool ok = false;
  qreal value =
      settings->settingsConf->value("thumbnailOpacity", 0.6).toReal(&ok);
  if (!ok)
    return 0.6;
  if (value > 1.0)
    return 1.0;
  if (value < 0.0)
    return 0.0;
  return value;
}

void Settings::setThumbnailOpacity(qreal value) {
  if (value > 1.0)
    value = 1.0;
  else if (value < 0.0)
    value = 0.0;
  settings->settingsConf->setValue("thumbnailOpacity", value);
}

bool Settings::useBlackBackground() {
  return settings->settingsConf->value("useBlackBackground", false).toBool();
}

void Settings::setUseBlackBackground(bool mode) {
  settings->settingsConf->setValue("useBlackBackground", mode);
}
//------------------------------------------------------------------------------
QVersionNumber Settings::lastVersion() {
  int vmajor = settings->settingsConf->value("lastVerMajor", 0).toInt();
  int vminor = settings->settingsConf->value("lastVerMinor", 0).toInt();
  int vmicro = settings->settingsConf->value("lastVerMicro", 0).toInt();
  return QVersionNumber(vmajor, vminor, vmicro);
}

void Settings::setLastVersion(QVersionNumber &ver) {
  settings->settingsConf->setValue("lastVerMajor", ver.majorVersion());
  settings->settingsConf->setValue("lastVerMinor", ver.minorVersion());
  settings->settingsConf->setValue("lastVerMicro", ver.microVersion());
}
//------------------------------------------------------------------------------
qreal Settings::backgroundOpacity() {
  bool ok = false;
  qreal value =
      settings->settingsConf->value("backgroundOpacity", 0.8).toReal(&ok);
  if (!ok)
    return 0.8;
  if (value > 1.0)
    return 1.0;
  if (value < 0.0)
    return 0.0;
  return value;
}

void Settings::setBackgroundOpacity(qreal value) {
  if (value > 1.0)
    value = 1.0;
  else if (value < 0.0)
    value = 0.0;
  settings->settingsConf->setValue("backgroundOpacity", value);
}
//------------------------------------------------------------------------------
void Settings::setSortingMode(SortingMode mode) {
  if (mode >= 6)
    mode = SortingMode::SORT_NAME;
  settings->settingsConf->setValue("sortingMode", mode);
}

SortingMode Settings::sortingMode() {
  int mode = settings->settingsConf->value("sortingMode", 0).toInt();
  if (mode < 0 || mode >= 6)
    mode = 0;
  return static_cast<SortingMode>(mode);
}
//------------------------------------------------------------------------------
void Settings::setFolderIconSortingMode(SortingMode mode) {
  if (mode < 0 || mode > SortingMode::SORT_TIME_DESC)
    mode = SortingMode::SORT_TIME_DESC;
  settings->settingsConf->setValue("folderIconSortingMode", mode);
}

SortingMode Settings::folderIconSortingMode() {
  int mode = settings->settingsConf
                 ->value("folderIconSortingMode", SortingMode::SORT_TIME_DESC)
                 .toInt();
  if (mode < 0 || mode > SortingMode::SORT_TIME_DESC)
    mode = SortingMode::SORT_TIME_DESC;
  return static_cast<SortingMode>(mode);
}
//------------------------------------------------------------------------------

ThumbPanelStyle Settings::thumbPanelStyle() {
  int mode = settings->settingsConf->value("thumbPanelStyle", 0).toInt();
  if (mode < 0 || mode > 1)
    mode = 0;
  return static_cast<ThumbPanelStyle>(mode);
}

void Settings::setThumbPanelStyle(ThumbPanelStyle mode) {
  settings->settingsConf->setValue("thumbPanelStyle", mode);
}
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int Settings::panelPreviewsSize() {
  bool ok = true;
  int size = settings->settingsConf->value("panelPreviewsSize", 256).toInt(&ok);
  if (!ok)
    size = 256;
  size = qBound(100, size, 256);
  return size;
}

void Settings::setPanelPreviewsSize(int size) {
  settings->settingsConf->setValue("panelPreviewsSize", size);
}
//------------------------------------------------------------------------------
bool Settings::usePreloader() {
  return settings->settingsConf->value("usePreloader", true).toBool();
}

void Settings::setUsePreloader(bool mode) {
  settings->settingsConf->setValue("usePreloader", mode);
}
//------------------------------------------------------------------------------
bool Settings::useUpscayl() {
  return mCachedUseUpscayl.load(std::memory_order_relaxed);
}

void Settings::setUseUpscayl(bool mode) {
  settings->settingsConf->setValue("useUpscayl", mode);
  mCachedUseUpscayl.store(mode, std::memory_order_relaxed);
}

bool Settings::preloadUpscayl() {
  return mCachedPreloadUpscayl.load(std::memory_order_relaxed);
}

void Settings::setPreloadUpscayl(bool mode) {
  settings->settingsConf->setValue("preloadUpscayl", mode);
  mCachedPreloadUpscayl.store(mode, std::memory_order_relaxed);
}

QString Settings::upscaylModel() {
  const QString model = settings->settingsConf
                            ->value("upscaylModel", defaultUpscaylModel())
                            .toString()
                            .trimmed();
  return model.isEmpty() ? defaultUpscaylModel() : model;
}

void Settings::setUpscaylModel(const QString &model) {
  const QString normalizedModel = model.trimmed();
  settings->settingsConf->setValue(
      "upscaylModel", normalizedModel.isEmpty() ? defaultUpscaylModel()
                                                 : normalizedModel);
}

QString Settings::defaultUpscaylModel() {
  return QStringLiteral("4xLSDIRCompactC3");
}

bool Settings::upscaylLimitEnabled() {
  return mCachedUpscaylLimitEnabled.load(std::memory_order_relaxed);
}

void Settings::setUpscaylLimitEnabled(bool enabled) {
  settings->settingsConf->setValue("upscaylLimitEnabled", enabled);
  mCachedUpscaylLimitEnabled.store(enabled, std::memory_order_relaxed);
}

int Settings::upscaylLimitValue() {
  return mCachedUpscaylLimitValue.load(std::memory_order_relaxed);
}

void Settings::setUpscaylLimitValue(int value) {
  settings->settingsConf->setValue("upscaylLimitValue", value);
  mCachedUpscaylLimitValue.store(value, std::memory_order_relaxed);
}

bool Settings::resizeUseUpscayl() {
  return mCachedResizeUseUpscayl.load(std::memory_order_relaxed);
}

void Settings::setResizeUseUpscayl(bool enabled) {
  settings->settingsConf->setValue("resizeUseUpscayl", enabled);
  mCachedResizeUseUpscayl.store(enabled, std::memory_order_relaxed);
}

bool Settings::hasUpscaylModels() {
  static bool checked = false;
  static bool exists = false;
  if (!checked) {
    QDir modelsDir(QApplication::applicationDirPath() + "/models");
    QStringList filters;
    filters << "*.param";
    QStringList files = modelsDir.entryList(filters, QDir::Files);
    for (const QString &file : files) {
      QFileInfo fi(file);
      QString modelName = fi.baseName();
      if (modelsDir.exists(modelName + ".bin")) {
        exists = true;
        break;
      }
    }
    checked = true;
  }
  return exists;
}

QStringList Settings::availableUpscaylModels() {
  QDir modelsDir(QApplication::applicationDirPath() + "/models");
  QStringList filters;
  filters << "*.param";
  QStringList files = modelsDir.entryList(filters, QDir::Files, QDir::Name);
  QStringList modelNames;
  for (const QString &file : files) {
    QFileInfo fi(file);
    QString modelName = fi.baseName();
    if (modelsDir.exists(modelName + ".bin")) {
      modelNames.append(modelName);
    }
  }
  return modelNames;
}
//------------------------------------------------------------------------------

bool Settings::keepFitMode() {
  return settings->settingsConf->value("keepFitMode", false).toBool();
}

void Settings::setKeepFitMode(bool mode) {
  settings->settingsConf->setValue("keepFitMode", mode);
}
//------------------------------------------------------------------------------
bool Settings::fullscreenMode() {
  return settings->settingsConf->value("openInFullscreen", false).toBool();
}

void Settings::setFullscreenMode(bool mode) {
  settings->settingsConf->setValue("openInFullscreen", mode);
}
//------------------------------------------------------------------------------
bool Settings::standbyMode() {
  return settings->settingsConf->value("standbyMode", false).toBool();
}

void Settings::setStandbyMode(bool mode) {
  settings->settingsConf->setValue("standbyMode", mode);
}
//------------------------------------------------------------------------------
bool Settings::maximizedWindow() {
  return settings->stateConf->value("maximizedWindow", false).toBool();
}

void Settings::setMaximizedWindow(bool mode) {
  settings->stateConf->setValue("maximizedWindow", mode);
}
//------------------------------------------------------------------------------
bool Settings::panelEnabled() {
  return settings->settingsConf->value("panelEnabled", true).toBool();
}

void Settings::setPanelEnabled(bool mode) {
  settings->settingsConf->setValue("panelEnabled", mode);
}
//------------------------------------------------------------------------------
bool Settings::panelFullscreenOnly() {
  return settings->settingsConf->value("panelFullscreenOnly", false).toBool();
}

void Settings::setPanelFullscreenOnly(bool mode) {
  settings->settingsConf->setValue("panelFullscreenOnly", mode);
}
//------------------------------------------------------------------------------
int Settings::lastDisplay() {
  return settings->stateConf->value("lastDisplay", 0).toInt();
}

void Settings::setLastDisplay(int display) {
  settings->stateConf->setValue("lastDisplay", display);
}
//------------------------------------------------------------------------------
PanelPosition Settings::panelPosition() {
  QString posString =
      settings->settingsConf->value("panelPosition", "bottom").toString();
  if (posString == "top") {
    return PanelPosition::PANEL_TOP;
  } else if (posString == "bottom") {
    return PanelPosition::PANEL_BOTTOM;
  } else if (posString == "left") {
    return PanelPosition::PANEL_LEFT;
  } else {
    return PanelPosition::PANEL_RIGHT;
  }
}

void Settings::setPanelPosition(PanelPosition pos) {
  QString posString;
  switch (pos) {
  case PANEL_TOP:
    posString = "top";
    break;
  case PANEL_BOTTOM:
    posString = "bottom";
    break;
  case PANEL_LEFT:
    posString = "left";
    break;
  case PANEL_RIGHT:
    posString = "right";
    break;
  }
  settings->settingsConf->setValue("panelPosition", posString);
}
//------------------------------------------------------------------------------
bool Settings::panelPinned() {
  return settings->settingsConf->value("panelPinned", false).toBool();
}

void Settings::setPanelPinned(bool mode) {
  settings->settingsConf->setValue("panelPinned", mode);
}
//------------------------------------------------------------------------------
/*
 * 0: fit window
 * 1: fit width
 * 2: orginal size
 * 3: fit window (stretch)
 */
ImageFitMode Settings::imageFitMode() {
  int mode = settings->settingsConf->value("defaultFitMode", 0).toInt();
  if (mode < 0 || mode > 3) {
    qWarning() << "Settings: Invalid fit mode ( " + QString::number(mode) +
                    " ). Resetting to default.";
    mode = 0;
  }
  return static_cast<ImageFitMode>(mode);
}

void Settings::setImageFitMode(ImageFitMode mode) {
  int modeInt = static_cast<ImageFitMode>(mode);
  if (modeInt < 0 || modeInt > 3) {
    qWarning() << "Settings: Invalid fit mode ( " + QString::number(modeInt) +
                    " ). Resetting to default.";
    modeInt = 0;
  }
  settings->settingsConf->setValue("defaultFitMode", modeInt);
}
//------------------------------------------------------------------------------
QRect Settings::windowGeometry() {
  QRect savedRect = settings->stateConf->value("windowGeometry").toRect();
  if (savedRect.size().isEmpty())
    savedRect.setRect(346, 350, 1356, 677);
  return savedRect;
}

void Settings::setWindowGeometry(QRect geometry) {
  settings->stateConf->setValue("windowGeometry", geometry);
}
//------------------------------------------------------------------------------
bool Settings::loopSlideshow() {
  return settings->settingsConf->value("loopSlideshow", false).toBool();
}

void Settings::setLoopSlideshow(bool mode) {
  settings->settingsConf->setValue("loopSlideshow", mode);
}

bool Settings::colorManagementEnabled() {
  return mCachedColorManagementEnabled.load(std::memory_order_relaxed);
}

void Settings::setColorManagementEnabled(bool enabled) {
  settings->settingsConf->setValue("colorManagementEnabled", enabled);
  mCachedColorManagementEnabled.store(enabled, std::memory_order_relaxed);
}

QString Settings::monitorColorProfileType() {
  QReadLocker locker(&mProfileLock);
  return mCachedMonitorColorProfileType;
}

void Settings::setMonitorColorProfileType(const QString &type) {
  settings->settingsConf->setValue("monitorColorProfileType", type);
  {
    QWriteLocker locker(&mProfileLock);
    mCachedMonitorColorProfileType = type;
  }
}

QString Settings::monitorColorProfilePath() {
  QReadLocker locker(&mProfileLock);
  return mCachedMonitorColorProfilePath;
}

void Settings::setMonitorColorProfilePath(const QString &path) {
  settings->settingsConf->setValue("monitorColorProfilePath", path);
  {
    QWriteLocker locker(&mProfileLock);
    mCachedMonitorColorProfilePath = path;
  }
}

//------------------------------------------------------------------------------
void Settings::sendChangeNotification() { emit settingsChanged(); }
//------------------------------------------------------------------------------
void Settings::readShortcuts(QMap<QString, QString> &shortcuts) {
  settings->settingsConf->beginGroup("Controls");
  QStringList in, pair;
  in = settings->settingsConf->value("shortcuts").toStringList();
  for (int i = 0; i < in.count(); i++) {
    pair = in[i].split("=");
    if (!pair[0].isEmpty() && !pair[1].isEmpty()) {
      if (pair[1].endsWith("eq"))
        pair[1] = pair[1].chopped(2) + "=";
      shortcuts.insert(pair[1], pair[0]);
    }
  }
  settings->settingsConf->endGroup();
}

void Settings::saveShortcuts(const QMap<QString, QString> &shortcuts) {
  settings->settingsConf->beginGroup("Controls");
  QMapIterator<QString, QString> i(shortcuts);
  QStringList out;
  while (i.hasNext()) {
    i.next();
    if (i.key().endsWith("="))
      out << i.value() + "=" + i.key().chopped(1) + "eq";
    else
      out << i.value() + "=" + i.key();
  }
  settings->settingsConf->setValue("shortcuts", out);
  settings->settingsConf->endGroup();
}
//------------------------------------------------------------------------------
void Settings::readScripts(QMap<QString, Script> &scripts) {
  scripts.clear();
  settings->settingsConf->beginGroup("Scripts");
  int size = settings->settingsConf->beginReadArray("script");
  for (int i = 0; i < size; i++) {
    settings->settingsConf->setArrayIndex(i);
    QString name = settings->settingsConf->value("name").toString();
    QVariant value = settings->settingsConf->value("value");
    Script scr = value.value<Script>();
    scripts.insert(name, scr);
  }
  settings->settingsConf->endArray();
  settings->settingsConf->endGroup();
}

void Settings::saveScripts(const QMap<QString, Script> &scripts) {
  settings->settingsConf->beginGroup("Scripts");
  settings->settingsConf->beginWriteArray("script");
  QMapIterator<QString, Script> i(scripts);
  int counter = 0;
  while (i.hasNext()) {
    i.next();
    settings->settingsConf->setArrayIndex(counter);
    settings->settingsConf->setValue("name", i.key());
    settings->settingsConf->setValue("value", QVariant::fromValue(i.value()));
    counter++;
  }
  settings->settingsConf->endArray();
  settings->settingsConf->endGroup();
}
//------------------------------------------------------------------------------
bool Settings::squareThumbnails() {
  return settings->settingsConf->value("squareThumbnails", false).toBool();
}

void Settings::setSquareThumbnails(bool mode) {
  settings->settingsConf->setValue("squareThumbnails", mode);
}
//------------------------------------------------------------------------------
bool Settings::transparencyGrid() {
  return settings->settingsConf->value("drawTransparencyGrid", false).toBool();
}

void Settings::setTransparencyGrid(bool mode) {
  settings->settingsConf->setValue("drawTransparencyGrid", mode);
}
//------------------------------------------------------------------------------
bool Settings::enableSmoothScroll() {
  return settings->settingsConf->value("enableSmoothScroll", true).toBool();
}

void Settings::setEnableSmoothScroll(bool mode) {
  settings->settingsConf->setValue("enableSmoothScroll", mode);
}
//------------------------------------------------------------------------------
bool Settings::enableSmoothZoom() {
  return settings->settingsConf->value("enableSmoothZoom", true).toBool();
}

void Settings::setEnableSmoothZoom(bool mode) {
  settings->settingsConf->setValue("enableSmoothZoom", mode);
}
//------------------------------------------------------------------------------
bool Settings::useThumbnailCache() {
  return settings->settingsConf->value("thumbnailCache", true).toBool();
}

void Settings::setUseThumbnailCache(bool mode) {
  settings->settingsConf->setValue("thumbnailCache", mode);
}
//------------------------------------------------------------------------------
QStringList Settings::savedPaths() {
  return settings->stateConf->value("savedPaths", QDir::homePath())
      .toStringList();
}

void Settings::setSavedPaths(QStringList paths) {
  settings->stateConf->setValue("savedPaths", paths);
}
//------------------------------------------------------------------------------
QStringList Settings::bookmarks() {
  return settings->stateConf->value("bookmarks").toStringList();
}

void Settings::setBookmarks(QStringList paths) {
  settings->stateConf->setValue("bookmarks", paths);
}
//------------------------------------------------------------------------------
QStringList Settings::formatFilter() {
  return settings->stateConf->value("formatFilter").toStringList();
}

void Settings::setFormatFilter(QStringList extensions) {
  settings->stateConf->setValue("formatFilter", extensions);
}
//------------------------------------------------------------------------------
bool Settings::placesPanel() {
  return settings->stateConf->value("placesPanel", true).toBool();
}

void Settings::setPlacesPanel(bool mode) {
  settings->stateConf->setValue("placesPanel", mode);
}
//------------------------------------------------------------------------------
bool Settings::placesPanelBookmarksExpanded() {
  return settings->stateConf->value("placesPanelBookmarksExpanded", true)
      .toBool();
}

void Settings::setPlacesPanelBookmarksExpanded(bool mode) {
  settings->stateConf->setValue("placesPanelBookmarksExpanded", mode);
}
//------------------------------------------------------------------------------
bool Settings::placesPanelTreeExpanded() {
  return settings->stateConf->value("placesPanelTreeExpanded", true).toBool();
}

void Settings::setPlacesPanelTreeExpanded(bool mode) {
  settings->stateConf->setValue("placesPanelTreeExpanded", mode);
}
//------------------------------------------------------------------------------
int Settings::placesPanelWidth() {
  return settings->stateConf->value("placesPanelWidth", 260).toInt();
}

void Settings::setPlacesPanelWidth(int width) {
  settings->stateConf->setValue("placesPanelWidth", width);
}
//------------------------------------------------------------------------------
void Settings::setSlideshowInterval(int ms) {
  settings->settingsConf->setValue("slideshowInterval", ms);
}

int Settings::slideshowInterval() {
  int interval =
      settings->settingsConf->value("slideshowInterval", 3000).toInt();
  if (interval <= 0)
    interval = 3000;
  return interval;
}
//------------------------------------------------------------------------------
int Settings::thumbnailerThreadCount() {
  int defaultCount = std::clamp(QThread::idealThreadCount() / 2, MinThumbnailerThreads, MaxThumbnailerThreads);
  int count = settings->settingsConf->value("thumbnailerThreads", defaultCount).toInt();
  if (count < MinThumbnailerThreads)
    count = defaultCount;
  return std::clamp(count, MinThumbnailerThreads, MaxThumbnailerThreads);
}

void Settings::setThumbnailerThreadCount(int count) {
  settings->settingsConf->setValue("thumbnailerThreads", count);
}

//------------------------------------------------------------------------------
int Settings::folderViewIconSize() {
  return settings->settingsConf->value("folderViewIconSize", 256).toInt();
}

void Settings::setFolderViewIconSize(int value) {
  settings->settingsConf->setValue("folderViewIconSize", value);
}
//------------------------------------------------------------------------------
bool Settings::expandImage() {
  return settings->settingsConf->value("expandImage", false).toBool();
}

void Settings::setExpandImage(bool mode) {
  settings->settingsConf->setValue("expandImage", mode);
}
//------------------------------------------------------------------------------
int Settings::expandLimit() {
  return settings->settingsConf->value("expandLimit", 2).toInt();
}

void Settings::setExpandLimit(int value) {
  settings->settingsConf->setValue("expandLimit", value);
}
int Settings::JPEGSaveQuality() {
  return mCachedJPEGSaveQuality.load(std::memory_order_relaxed);
}

void Settings::setJPEGSaveQuality(int value) {
  settings->settingsConf->setValue("JPEGSaveQuality", value);
  mCachedJPEGSaveQuality.store(std::clamp(value, 0, 100), std::memory_order_relaxed);
}
//------------------------------------------------------------------------------
int Settings::pngSaveQuality() {
  return mCachedPngSaveQuality.load(std::memory_order_relaxed);
}

void Settings::setPngSaveQuality(int value) {
  settings->settingsConf->setValue("pngSaveQuality", value);
  mCachedPngSaveQuality.store(std::clamp(value, 0, 9), std::memory_order_relaxed);
}
//------------------------------------------------------------------------------
int Settings::modernSaveQuality() {
  return mCachedModernSaveQuality.load(std::memory_order_relaxed);
}

void Settings::setModernSaveQuality(int value) {
  settings->settingsConf->setValue("modernSaveQuality", value);
  mCachedModernSaveQuality.store(std::clamp(value, 0, 100), std::memory_order_relaxed);
}
//------------------------------------------------------------------------------
ScalingFilter Settings::scalingFilter() {
  int mode = settings->settingsConf->value("scalingFilter", QI_FILTER_CAS)
                 .toInt();
  if (mode < 0 || mode > QI_FILTER_MKS2021)
    mode = QI_FILTER_BILINEAR; // default to Bilinear if out of range
  return static_cast<ScalingFilter>(mode);
}

void Settings::setScalingFilter(ScalingFilter mode) {
  settings->settingsConf->setValue("scalingFilter", mode);
}

float Settings::casSharpening() {
  return settings->settingsConf->value("casSharpening", 1.0f).toFloat();
}

void Settings::setCasSharpening(float value) {
  settings->settingsConf->setValue("casSharpening", value);
}

float Settings::casContrast() {
  return settings->settingsConf->value("casContrast", 0.0f).toFloat();
}

void Settings::setCasContrast(float value) {
  settings->settingsConf->setValue("casContrast", value);
}

//------------------------------------------------------------------------------
bool Settings::infoBarFullscreen() {
  return settings->settingsConf->value("infoBarFullscreen", true).toBool();
}

void Settings::setInfoBarFullscreen(bool mode) {
  settings->settingsConf->setValue("infoBarFullscreen", mode);
}
//------------------------------------------------------------------------------
bool Settings::infoBarWindowed() {
  return settings->settingsConf->value("infoBarWindowed", false).toBool();
}

void Settings::setInfoBarWindowed(bool mode) {
  settings->settingsConf->setValue("infoBarWindowed", mode);
}
//------------------------------------------------------------------------------
bool Settings::windowTitleExtendedInfo() {
  return settings->settingsConf->value("windowTitleExtendedInfo", true)
      .toBool();
}

void Settings::setWindowTitleExtendedInfo(bool mode) {
  settings->settingsConf->setValue("windowTitleExtendedInfo", mode);
}

//------------------------------------------------------------------------------
bool Settings::cursorAutohide() {
  return settings->settingsConf->value("cursorAutohiding", true).toBool();
}

void Settings::setCursorAutohide(bool mode) {
  settings->settingsConf->setValue("cursorAutohiding", mode);
}
//------------------------------------------------------------------------------
bool Settings::firstRun() {
  return settings->settingsConf->value("firstRun", true).toBool();
}

void Settings::setFirstRun(bool mode) {
  settings->settingsConf->setValue("firstRun", mode);
}
//------------------------------------------------------------------------------
bool Settings::showSaveOverlay() {
  return settings->settingsConf->value("showSaveOverlay", true).toBool();
}

void Settings::setShowSaveOverlay(bool mode) {
  settings->settingsConf->setValue("showSaveOverlay", mode);
}
//------------------------------------------------------------------------------
bool Settings::confirmDelete() {
  return settings->settingsConf->value("confirmDelete", true).toBool();
}

void Settings::setConfirmDelete(bool mode) {
  settings->settingsConf->setValue("confirmDelete", mode);
}
//------------------------------------------------------------------------------
bool Settings::confirmTrash() {
  return settings->settingsConf->value("confirmTrash", true).toBool();
}

void Settings::setConfirmTrash(bool mode) {
  settings->settingsConf->setValue("confirmTrash", mode);
}
//------------------------------------------------------------------------------
bool Settings::unloadThumbs() {
  return settings->settingsConf->value("unloadThumbs", true).toBool();
}

void Settings::setUnloadThumbs(bool mode) {
  settings->settingsConf->setValue("unloadThumbs", mode);
}
//------------------------------------------------------------------------------
float Settings::zoomStep() {
  bool ok = false;
  float value = settings->settingsConf->value("zoomStep", 0.2f).toFloat(&ok);
  if (!ok)
    return 0.2f;
  value = qBound(0.01f, value, 0.5f);
  return value;
}

void Settings::setZoomStep(float value) {
  value = qBound(0.01f, value, 0.5f);
  settings->settingsConf->setValue("zoomStep", value);
}
//------------------------------------------------------------------------------
float Settings::mouseScrollingSpeed() {
  bool ok = false;
  float value =
      settings->settingsConf->value("mouseScrollingSpeed", 1.0f).toFloat(&ok);
  if (!ok)
    return 1.0f;
  value = qBound(0.5f, value, 2.0f);
  return value;
}

void Settings::setMouseScrollingSpeed(float value) {
  value = qBound(0.5f, value, 2.0f);
  settings->settingsConf->setValue("mouseScrollingSpeed", value);
}
//------------------------------------------------------------------------------
void Settings::setZoomIndicatorMode(ZoomIndicatorMode mode) {
  settings->settingsConf->setValue("zoomIndicatorMode", mode);
}

ZoomIndicatorMode Settings::zoomIndicatorMode() {
  int mode = settings->settingsConf->value("zoomIndicatorMode", 2).toInt();
  if (mode < 0 || mode > 2)
    mode = 2;
  return static_cast<ZoomIndicatorMode>(mode);
}
//------------------------------------------------------------------------------
void Settings::setFocusPointIn1to1Mode(ImageFocusPoint mode) {
  settings->settingsConf->setValue("focusPointIn1to1Mode", mode);
}

ImageFocusPoint Settings::focusPointIn1to1Mode() {
  int mode = settings->settingsConf->value("focusPointIn1to1Mode", 2).toInt();
  if (mode < 0 || mode > 2)
    mode = 2;
  return static_cast<ImageFocusPoint>(mode);
}

void Settings::setDefaultCropAction(DefaultCropAction mode) {
  settings->settingsConf->setValue("defaultCropAction", mode);
}

DefaultCropAction Settings::defaultCropAction() {
  int mode = settings->settingsConf->value("defaultCropAction", 0).toInt();
  if (mode < 0 || mode > 1)
    mode = 0;
  return static_cast<DefaultCropAction>(mode);
}

ImageScrolling Settings::imageScrolling() {
  int mode = settings->settingsConf->value("imageScrolling", 1).toInt();
  if (mode < 0 || mode > 2)
    mode = 0;
  return static_cast<ImageScrolling>(mode);
}

void Settings::setImageScrolling(ImageScrolling mode) {
  settings->settingsConf->setValue("imageScrolling", mode);
}
//------------------------------------------------------------------------------
ViewMode Settings::defaultViewMode() {
  int mode = settings->settingsConf->value("defaultViewMode", 1).toInt();
  if (mode < 0 || mode > 1)
    mode = 1;
  return static_cast<ViewMode>(mode);
}

void Settings::setDefaultViewMode(ViewMode mode) {
  settings->settingsConf->setValue("defaultViewMode", mode);
}
//------------------------------------------------------------------------------
FolderEndAction Settings::folderEndAction() {
  int mode = settings->settingsConf->value("folderEndAction", 0).toInt();
  if (mode < 0 || mode > 2)
    mode = 0;
  return static_cast<FolderEndAction>(mode);
}

void Settings::setFolderEndAction(FolderEndAction mode) {
  settings->settingsConf->setValue("folderEndAction", mode);
}
//------------------------------------------------------------------------------
bool Settings::printLandscape() {
  return stateConf->value("printLandscape", false).toBool();
}

void Settings::setPrintLandscape(bool mode) {
  stateConf->setValue("printLandscape", mode);
}
//------------------------------------------------------------------------------
bool Settings::printPdfDefault() {
  return stateConf->value("printPdfDefault", false).toBool();
}

void Settings::setPrintPdfDefault(bool mode) {
  stateConf->setValue("printPdfDefault", mode);
}
//------------------------------------------------------------------------------
bool Settings::printColor() {
  return stateConf->value("printColor", false).toBool();
}

void Settings::setPrintColor(bool mode) {
  stateConf->setValue("printColor", mode);
}
//------------------------------------------------------------------------------
bool Settings::printFitToPage() {
  return stateConf->value("printFitToPage", true).toBool();
}

void Settings::setPrintFitToPage(bool mode) {
  stateConf->setValue("printFitToPage", mode);
}
//------------------------------------------------------------------------------
QString Settings::lastPrinter() {
  return stateConf->value("lastPrinter", "").toString();
}

void Settings::setLastPrinter(QString name) {
  stateConf->setValue("lastPrinter", name);
}
//------------------------------------------------------------------------------
bool Settings::jxlAnimation() {
  return mCachedJxlAnimation.load(std::memory_order_relaxed);
}

void Settings::setJxlAnimation(bool mode) {
  settings->settingsConf->setValue("jxlAnimation", mode);
  mCachedJxlAnimation.store(mode, std::memory_order_relaxed);
}
//------------------------------------------------------------------------------
bool Settings::autoResizeWindow() {
  return settings->settingsConf->value("autoResizeWindow", false).toBool();
}

void Settings::setAutoResizeWindow(bool mode) {
  settings->settingsConf->setValue("autoResizeWindow", mode);
}
//------------------------------------------------------------------------------
int Settings::autoResizeLimit() {
  int limit = settings->settingsConf->value("autoResizeLimit", 90).toInt();
  if (limit < 30 || limit > 100)
    limit = 90;
  return limit;
}

void Settings::setAutoResizeLimit(int percent) {
  settings->settingsConf->setValue("autoResizeLimit", percent);
}
//------------------------------------------------------------------------------
int Settings::memoryAllocationLimit() {
  return mCachedMemoryAllocationLimit.load(std::memory_order_relaxed);
}

void Settings::setMemoryAllocationLimit(int limitMB) {
  settings->settingsConf->setValue("memoryAllocationLimit", limitMB);
  int limit = limitMB;
  if (limit < 512) limit = 512;
  else if (limit > 8192) limit = 8192;
  mCachedMemoryAllocationLimit.store(limit, std::memory_order_relaxed);
}
//------------------------------------------------------------------------------
bool Settings::panelCenterSelection() {
  return settings->settingsConf->value("panelCenterSelection", true).toBool();
}

void Settings::setPanelCenterSelection(bool mode) {
  settings->settingsConf->setValue("panelCenterSelection", mode);
}
//------------------------------------------------------------------------------
QString Settings::language() {
  return settingsConf->value("language", "en_US").toString();
}

void Settings::setLanguage(QString lang) {
  settingsConf->setValue("language", lang);
}
//------------------------------------------------------------------------------
bool Settings::showSubfoldersInPanel() {
  return settings->settingsConf->value("showSubfoldersInPanel", false).toBool();
}

void Settings::setShowSubfoldersInPanel(bool mode) {
  settings->settingsConf->setValue("showSubfoldersInPanel", mode);
}
//------------------------------------------------------------------------------
bool Settings::useFixedZoomLevels() {
  return settings->settingsConf->value("useFixedZoomLevels", true).toBool();
}

void Settings::setUseFixedZoomLevels(bool mode) {
  settings->settingsConf->setValue("useFixedZoomLevels", mode);
}
//------------------------------------------------------------------------------
QString Settings::defaultZoomLevels() {
  return QString(
      "0.05,0.1,0.125,0.166,0.25,0.333,0.5,0.66,1,1.25,1.5,2,3,4,5,6,7,8");
}
QString Settings::zoomLevels() {
  return settingsConf->value("fixedZoomLevels", defaultZoomLevels()).toString();
}

void Settings::setZoomLevels(QString levels) {
  settingsConf->setValue("fixedZoomLevels", levels);
}
//------------------------------------------------------------------------------
bool Settings::unlockMinZoom() {
  return settings->settingsConf->value("unlockMinZoom", false).toBool();
}

void Settings::setUnlockMinZoom(bool mode) {
  settings->settingsConf->setValue("unlockMinZoom", mode);
}
//------------------------------------------------------------------------------
bool Settings::sortFolders() {
  return settings->settingsConf->value("sortFolders", true).toBool();
}

void Settings::setSortFolders(bool mode) {
  settings->settingsConf->setValue("sortFolders", mode);
}
//------------------------------------------------------------------------------
bool Settings::trackpadDetection() {
  return settings->settingsConf->value("trackpadDetection", false).toBool();
}

void Settings::setTrackpadDetection(bool mode) {
  settings->settingsConf->setValue("trackpadDetection", mode);
}
//------------------------------------------------------------------------------
bool Settings::clickableEdges() {
  return settings->settingsConf->value("clickableEdges", true).toBool();
}

void Settings::setClickableEdges(bool mode) {
  settings->settingsConf->setValue("clickableEdges", mode);
}
//------------------------------------------------------------------------------
bool Settings::clickableEdgesVisible() {
  return settings->settingsConf->value("clickableEdgesVisible", true).toBool();
}

void Settings::setClickableEdgesVisible(bool mode) {
  settings->settingsConf->setValue("clickableEdgesVisible", mode);
}
//------------------------------------------------------------------------------
bool Settings::showHiddenFiles() {
  return settings->settingsConf->value("showHiddenFiles", false).toBool();
}

void Settings::setShowHiddenFiles(bool mode) {
  settings->settingsConf->setValue("showHiddenFiles", mode);
}
//------------------------------------------------------------------------------
bool Settings::multiInstance() {
  return settings->settingsConf->value("multiInstance", false).toBool();
}

void Settings::setMultiInstance(bool mode) {
  settings->settingsConf->setValue("multiInstance", mode);
}
//------------------------------------------------------------------------------
bool Settings::rememberLastFolder() {
  return settings->settingsConf->value("rememberLastFolder", false).toBool();
}

void Settings::setRememberLastFolder(bool mode) {
  settings->settingsConf->setValue("rememberLastFolder", mode);
}

QString Settings::lastFolder() {
  return settings->settingsConf->value("lastFolder", "").toString();
}

void Settings::setLastFolder(const QString &path) {
  settings->settingsConf->setValue("lastFolder", path);
  settings->settingsConf->sync();
}
//------------------------------------------------------------------------------
QString Settings::excludedCachePaths() {
  QReadLocker locker(&mExcludedPathsLock);
  return mCachedExcludedCachePaths;
}

void Settings::setExcludedCachePaths(QString paths) {
  settings->settingsConf->setValue("excludedCachePaths", paths);
  {
    QWriteLocker locker(&mExcludedPathsLock);
    mCachedExcludedCachePaths = paths;
    mCachedExcludedCachePathsList = paths.split(';');
  }
}

bool Settings::isPathExcludedFromCache(const QString &path) {
  QStringList parts;
  {
    QReadLocker locker(&mExcludedPathsLock);
    if (mCachedExcludedCachePaths.isEmpty()) {
      return false;
    }
    parts = mCachedExcludedCachePathsList;
  }

  QString cleanPath = QDir::cleanPath(path);
  for (const QString &part : parts) {
    QString trimmed = part.trimmed();
    if (trimmed.isEmpty()) {
      continue;
    }
    QString cleanExcluded = QDir::cleanPath(trimmed);

    Qt::CaseSensitivity cs = Qt::CaseSensitive;
#ifdef Q_OS_WIN
    cs = Qt::CaseInsensitive;
#endif

    if (cleanPath.compare(cleanExcluded, cs) == 0) {
      return true;
    }
    QString prefix = cleanExcluded;
    if (!prefix.endsWith('/')) {
      prefix.append('/');
    }
    if (cleanPath.startsWith(prefix, cs)) {
      return true;
    }
  }
  return false;
}
