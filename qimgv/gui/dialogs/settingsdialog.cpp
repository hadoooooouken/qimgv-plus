#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "components/cache/thumbnailcache.h"
#include <QDir>
#include <QFileInfo>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SettingsDialog) {
  ui->setupUi(this);
#ifndef USE_UPSCAYL
  ui->stackedWidget->removeWidget(ui->AIUpscale);
#else
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, ui->preloadUpscaylCheckBox, &QCheckBox::setEnabled);
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, ui->upscaylModelComboBox, &QComboBox::setEnabled);
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, ui->label_upscaylModel, &QLabel::setEnabled);
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, ui->label_upscaylGetModels, &QLabel::setEnabled);
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, ui->upscaylLimitCheckBox, &QCheckBox::setEnabled);

  auto updateLimitControls = [this]() {
    bool enabled = ui->useUpscaylCheckBox->isChecked() && ui->upscaylLimitCheckBox->isChecked();
    ui->upscaylLimitSlider->setEnabled(enabled);
    ui->upscaylLimitValueLabel->setEnabled(enabled);
  };
  connect(ui->useUpscaylCheckBox, &QCheckBox::toggled, this, updateLimitControls);
  connect(ui->upscaylLimitCheckBox, &QCheckBox::toggled, this, updateLimitControls);

  connect(ui->upscaylLimitSlider, &QSlider::valueChanged, this, [this](int value) {
    int snapped = ((value + 2) / 5) * 5;
    if (snapped != value) {
      ui->upscaylLimitSlider->setValue(snapped);
      return;
    }
    ui->upscaylLimitValueLabel->setText(QString::number(snapped) + "%");
  });

  // Auto-scan models directory for compatible models
  QDir modelsDir(qApp->applicationDirPath() + "/models");
  QStringList filters;
  filters << "*.param";
  QStringList files = modelsDir.entryList(filters, QDir::Files);
  QStringList modelNames;
  for (const QString &file : files) {
    QFileInfo fi(file);
    QString modelName = fi.baseName();
    if (modelsDir.exists(modelName + ".bin")) {
      modelNames.append(modelName);
    }
  }
  if (modelNames.isEmpty()) {
    modelNames.append("remacri-4x");
  }
  ui->upscaylModelComboBox->addItems(modelNames);
#endif
  ui->panelSizeSlider->setMinimum(13);
  ui->panelSizeSlider->setMaximum(32);
  ui->panelSizeSlider->setSingleStep(1);
  this->setWindowTitle(tr("Preferences — ") + qApp->applicationName());

  ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->aboutAppTextBrowser->viewport()->setAutoFillBackground(false);
  ui->versionLabel->setText("" + QApplication::applicationVersion());
  ui->qtVersionLabel->setText(qVersion());
  ui->appIconLabel->setPixmap(
      QIcon(":/res/icons/common/logo/app/22.png").pixmap(22, 22));
  ui->qtIconLabel->setPixmap(
      QIcon(":/res/icons/common/logo/3rdparty/qt22.png").pixmap(22, 16));

  // fake combobox that acts as a menu button
  // less code than using pushbutton with menu
  // will be replaced with something custom later
  connect(ui->themeSelectorComboBox,
          qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
            ui->themeSelectorComboBox->blockSignals(true);
            ui->themeSelectorComboBox->setCurrentIndex(index);
            ui->themeSelectorComboBox->blockSignals(false);
            switch (index) {
            case 0:
              setColorScheme(ThemeStore::colorScheme(COLORS_BLACK));
              settings->setColorTid(COLORS_BLACK);
              break;
            case 1:
              setColorScheme(ThemeStore::colorScheme(COLORS_DARK));
              settings->setColorTid(COLORS_DARK);
              break;
            case 2:
              setColorScheme(ThemeStore::colorScheme(COLORS_DARKBLUE));
              settings->setColorTid(COLORS_DARKBLUE);
              break;
            case 3:
              setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));
              settings->setColorTid(COLORS_LIGHT);
              break;
            }
          });

  connect(ui->useSystemColorsCheckBox, &QCheckBox::toggled,
          [this](bool useSystemTheme) {
            if (useSystemTheme) {
              ui->themeSelectorComboBox->setCurrentIndex(-1);
              setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));
              settings->setColorTid(COLORS_SYSTEM);
            } else {
              readColorScheme();
              settings->setColorTid(COLORS_CUSTOMIZED);
            }
            ui->themeSelectorComboBox->setEnabled(!useSystemTheme);
            ui->colorConfigSubgroup->setEnabled(!useSystemTheme);
            ui->modifySystemSchemeLabel->setVisible(useSystemTheme);
          });

  connect(ui->modifySystemSchemeLabel, &ClickableLabel::clicked, [this]() {
    ui->useSystemColorsCheckBox->setChecked(false);
    setColorScheme(ThemeStore::colorScheme(COLORS_CUSTOMIZED));
    settings->setColorTid(COLORS_CUSTOMIZED);
  });

  ui->colorSelectorAccent->setDescription(tr("Accent color"));
  ui->colorSelectorBackground->setDescription(tr("Windowed mode background"));
  ui->colorSelectorFullscreen->setDescription(tr("Fullscreen mode background"));
  ui->colorSelectorFolderview->setDescription(tr("FolderView background"));
  ui->colorSelectorFolderviewPanel->setDescription(tr("FolderView top panel"));
  ui->colorSelectorText->setDescription(tr("Text color"));
  ui->colorSelectorWidget->setDescription(tr("Widget background"));
  ui->colorSelectorWidgetBorder->setDescription(tr("Widget border"));
  ui->colorSelectorOverlay->setDescription(tr("Overlay background"));
  ui->colorSelectorOverlayText->setDescription(tr("Overlay text"));
  ui->colorSelectorScrollbar->setDescription(tr("Scrollbars"));
  ui->colorSelectorThumbpanel->setDescription(tr("Thumbnail panel"));
  ui->colorSelectorThumbpanel->setShowAlpha(true);


  ui->scalingQualityComboBox->clear();
  ui->scalingQualityComboBox->addItem("Nearest", QI_FILTER_NEAREST);
  ui->scalingQualityComboBox->addItem("Bilinear", QI_FILTER_BILINEAR);

#ifdef USE_OPENCV
  ui->scalingQualityComboBox->addItem("Bilinear+sharpen (OpenCV)",
                                      QI_FILTER_CV_BILINEAR_SHARPEN);
  ui->scalingQualityComboBox->addItem("Bicubic (OpenCV)", QI_FILTER_CV_CUBIC);
  ui->scalingQualityComboBox->addItem("Bicubic+sharpen (OpenCV)",
                                      QI_FILTER_CV_CUBIC_SHARPEN);
  ui->scalingQualityComboBox->addItem("Lanczos (OpenCV)", QI_FILTER_CV_LANCZOS);
  ui->scalingQualityComboBox->addItem("Area (OpenCV)", QI_FILTER_CV_AREA);
  ui->scalingQualityComboBox->addItem("Smart sharpen (OpenCV)", QI_FILTER_CV_SMART);
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  ui->memoryLimitSpinBox->setEnabled(false);
  ui->memoryLimitLabel->setEnabled(false);
#endif

  if (!settings->supportedFormats().contains("jxl"))
    ui->animatedJxlCheckBox->hide();

  setupSidebar();

  // setup radioBtn groups
  fitModeGrp.addButton(ui->fitModeWindow);
  fitModeGrp.addButton(ui->fitModeWidth);
  fitModeGrp.addButton(ui->fitMode1to1);
  fitModeGrp.addButton(ui->fitModeWindowStretch);
  folderEndGrp.addButton(ui->folderEndSwitchFolder);
  folderEndGrp.addButton(ui->folderEndNoAction);
  folderEndGrp.addButton(ui->folderEndLoop);
  zoomIndGrp.addButton(ui->zoomIndicatorAuto);
  zoomIndGrp.addButton(ui->zoomIndicatorOff);
  zoomIndGrp.addButton(ui->zoomIndicatorOn);

  // readable language names
  langs.insert("de_DE", "Deutsch");
  langs.insert("en_US", "English");
  langs.insert("es_ES", "Español");
  langs.insert("fr_FR", "Français");
  langs.insert("ja_JP", "日本語");
  langs.insert("tr_TR", "Türkçe");
  langs.insert("uk_UA", "Українська");
  langs.insert("zh_CN", "简体中文");
  // fill langs combobox, sorted by locale
  ui->langComboBox->addItems(langs.values());
  // insert system language entry manually at the beginning
  langs.insert("system", "System language");
  ui->langComboBox->insertItem(0, "System language");

  connect(ui->thumbnailResolutionSlider, &QSlider::valueChanged, this, &SettingsDialog::onThumbnailResolutionSliderChanged);

  // Modern formats quality row
  QHBoxLayout *modernLayout = new QHBoxLayout();
  modernLayout->setContentsMargins(0, 0, 0, 0);
  QLabel *modernTitleLabel = new QLabel(tr("Modern formats quality (WebP, JXL, AVIF):"), this);
  modernQualitySlider = new QSlider(Qt::Horizontal, this);
  modernQualitySlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  modernQualitySlider->setMinimumSize(180, 25);
  modernQualitySlider->setRange(0, 100);
  modernQualitySlider->setPageStep(5);
  modernQualitySlider->setTickPosition(QSlider::TicksBelow);
  modernQualitySlider->setTickInterval(10);
  modernQualityLabel = new QLabel(this);
  QSpacerItem *modernSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

  modernLayout->addWidget(modernTitleLabel);
  modernLayout->addWidget(modernQualitySlider);
  modernLayout->addWidget(modernQualityLabel);
  modernLayout->addSpacerItem(modernSpacer);

  // PNG quality row
  QHBoxLayout *pngLayout = new QHBoxLayout();
  pngLayout->setContentsMargins(0, 0, 0, 0);
  QLabel *pngTitleLabel = new QLabel(tr("PNG compression level:"), this);
  pngQualitySlider = new QSlider(Qt::Horizontal, this);
  pngQualitySlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  pngQualitySlider->setMinimumSize(180, 25);
  pngQualitySlider->setRange(0, 9);
  pngQualitySlider->setPageStep(1);
  pngQualitySlider->setSingleStep(1);
  pngQualitySlider->setTickPosition(QSlider::TicksBelow);
  pngQualitySlider->setTickInterval(1);
  pngQualityLabel = new QLabel(this);
  QSpacerItem *pngSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

  pngLayout->addWidget(pngTitleLabel);
  pngLayout->addWidget(pngQualitySlider);
  pngLayout->addWidget(pngQualityLabel);
  pngLayout->addSpacerItem(pngSpacer);

  // Connect signals
  connect(pngQualitySlider, &QSlider::valueChanged, this, &SettingsDialog::onPNGQualitySliderChanged);
  connect(modernQualitySlider, &QSlider::valueChanged, this, &SettingsDialog::onModernQualitySliderChanged);

  // Insert into vertical layout right after JPEG save quality row (ui->horizontalLayout_10)
  int idx = ui->verticalLayout_34->indexOf(ui->horizontalLayout_10);
  if (idx != -1) {
      ui->verticalLayout_34->insertLayout(idx + 1, modernLayout);
      ui->verticalLayout_34->insertLayout(idx + 2, pngLayout);
  } else {
      ui->verticalLayout_34->addLayout(modernLayout);
      ui->verticalLayout_34->addLayout(pngLayout);
  }

  connect(this, &SettingsDialog::settingsChanged, settings,
          &Settings::sendChangeNotification);
  readSettings();

  adjustSizeToContents();
}
//------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog() { delete ui; }
//------------------------------------------------------------------------------
// an attempt to force minimum width to fit contents
void SettingsDialog::adjustSizeToContents() {
  // general tab
  ui->gridLayout->activate();
  ui->horizontalLayout_28->activate();
  ui->horizontalLayout_19->activate();
  ui->gridLayout_3->activate();
  ui->horizontalLayout_18->activate();
  ui->gridLayout_4->activate();
  ui->horizontalLayout_24->activate();
  ui->gridLayout_5->activate();
  ui->slideshowGroupContents->activate();
  ui->scrollAreaWidgetContents->layout()->activate();
  ui->scrollArea->setMinimumWidth(
      ui->scrollAreaWidgetContents->minimumSizeHint().width());
  // view tab
  ui->horizontalLayout_29->activate();
  ui->horizontalLayout_31->activate();
  ui->widget->layout()->activate();
  ui->scrollAreaWidgetContents_3->layout()->activate();
  ui->scrollArea_3->setMinimumWidth(
      ui->scrollAreaWidgetContents_3->minimumSizeHint().width());
  // container
  // ui->stackedWidget->layout()->activate();
  this->setMinimumWidth(sizeHint().width() + 22);

  // qDebug() << "window:" << this->sizeHint() << this->minimumSizeHint() <<
  // this->size(); qDebug() << "stackedwidget:" << ui->stackedWidget->sizeHint()
  // << ui->stackedWidget->minimumSizeHint() << ui->stackedWidget->size();
  // qDebug() << "scrollarea:" << ui->scrollArea->sizeHint() <<
  // ui->scrollArea->minimumSizeHint() << ui->scrollArea->size(); qDebug() <<
  // "scrollareawidget:" << ui->scrollAreaWidgetContents->sizeHint() <<
  // ui->scrollAreaWidgetContents->minimumSizeHint() <<
  // ui->scrollAreaWidgetContents->size(); qDebug() << "grid" <<
  // ui->gridLayout_15->sizeHint(); qDebug() << "wtf" <<
  // ui->startInFolderViewCheckBox->sizeHint() <<
  // ui->startInFolderViewCheckBox->minimumSizeHint();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetToDesktopTheme() {
  settings->setColorScheme(
      ThemeStore::colorScheme(ColorSchemes::COLORS_SYSTEM));
  this->readColorScheme();
}
//------------------------------------------------------------------------------
void SettingsDialog::setupSidebar() {}
//------------------------------------------------------------------------------
void SettingsDialog::readSettings() {
  ui->loopSlideshowCheckBox->setChecked(settings->loopSlideshow());
  ui->enablePanelCheckBox->setChecked(settings->panelEnabled());
  ui->thumbnailPanelGroupContents->setEnabled(settings->panelEnabled());
  ui->panelFullscreenOnlyCheckBox->setChecked(settings->panelFullscreenOnly());
  ui->squareThumbnailsCheckBox->setChecked(settings->squareThumbnails());
  ui->transparencyGridCheckBox->setChecked(settings->transparencyGrid());
  ui->enableSmoothScrollCheckBox->setChecked(settings->enableSmoothScroll());
  ui->enableSmoothZoomCheckBox->setChecked(settings->enableSmoothZoom());
  ui->usePreloaderCheckBox->setChecked(settings->usePreloader());
  ui->useThumbnailCacheCheckBox->setChecked(settings->useThumbnailCache());
  ui->smoothUpscalingCheckBox->setChecked(settings->smoothUpscaling());
  ui->expandImageCheckBox->setChecked(settings->expandImage());
  ui->expandImagesGroupContents->setEnabled(settings->expandImage());
  ui->smoothAnimatedImagesCheckBox->setChecked(
      settings->smoothAnimatedImages());
  ui->bgOpacitySlider->setValue(
      static_cast<int>(settings->backgroundOpacity() * 100));
  ui->sortingComboBox->setCurrentIndex(settings->sortingMode());
  ui->confirmDeleteCheckBox->setChecked(settings->confirmDelete());
  ui->confirmTrashCheckBox->setChecked(settings->confirmTrash());
  ui->unlockMinZoomCheckBox->setChecked(settings->unlockMinZoom());
  ui->sortFoldersCheckBox->setChecked(settings->sortFolders());
  ui->trackpadDetectionCheckBox->setChecked(settings->trackpadDetection());
  ui->clickableEdgesCheckBox->setChecked(settings->clickableEdges());
  ui->clickableEdgesVisibleCheckBox->setChecked(
      settings->clickableEdgesVisible());
  ui->clickableEdgesVisibleCheckBox->setEnabled(settings->clickableEdges());
  ui->showHiddenFilesCheckBox->setChecked(settings->showHiddenFiles());

  if (settings->zoomIndicatorMode() == INDICATOR_ENABLED)
    ui->zoomIndicatorOn->setChecked(true);
  else if (settings->zoomIndicatorMode() == INDICATOR_AUTO)
    ui->zoomIndicatorAuto->setChecked(true);
  else
    ui->zoomIndicatorOff->setChecked(true);
  ui->showInfoBarFullscreen->setChecked(settings->infoBarFullscreen());
  ui->showInfoBarWindowed->setChecked(settings->infoBarWindowed());
  ui->showExtendedInfoTitle->setChecked(settings->windowTitleExtendedInfo());
  ui->cursorAutohideCheckBox->setChecked(settings->cursorAutohide());
  ui->keepFitModeCheckBox->setChecked(settings->keepFitMode());
  if (settings->focusPointIn1to1Mode() == FOCUS_TOP)
    ui->focus1to1Top->setChecked(true);
  else if (settings->focusPointIn1to1Mode() == FOCUS_CENTER)
    ui->focus1to1Center->setChecked(true);
  else
    ui->focus1to1Cursor->setChecked(true);
  ui->slideshowIntervalSpinBox->setValue(settings->slideshowInterval());
  ui->imageScrollingComboBox->setCurrentIndex(settings->imageScrolling());
  ui->saveOverlayCheckBox->setChecked(settings->showSaveOverlay());
  ui->unloadThumbsCheckBox->setChecked(settings->unloadThumbs());
  if (settings->thumbPanelStyle() == TH_PANEL_SIMPLE)
    ui->thumbStyleSimple->setChecked(true);
  else
    ui->thumbStyleExtended->setChecked(true);
  ui->animatedJxlCheckBox->setChecked(settings->jxlAnimation());
  ui->multiInstanceCheckBox->setChecked(settings->multiInstance());
#ifdef USE_UPSCAYL
  ui->useUpscaylCheckBox->setChecked(settings->useUpscayl());
  ui->preloadUpscaylCheckBox->setChecked(settings->preloadUpscayl());
  ui->preloadUpscaylCheckBox->setEnabled(settings->useUpscayl());
  ui->upscaylModelComboBox->setEnabled(settings->useUpscayl());
  ui->label_upscaylModel->setEnabled(settings->useUpscayl());
  ui->label_upscaylGetModels->setEnabled(settings->useUpscayl());

  int modelIdx = ui->upscaylModelComboBox->findText(settings->upscaylModel());
  if (modelIdx != -1) {
    ui->upscaylModelComboBox->setCurrentIndex(modelIdx);
  } else {
    int defaultIdx = ui->upscaylModelComboBox->findText("remacri-4x");
    if (defaultIdx != -1) {
      ui->upscaylModelComboBox->setCurrentIndex(defaultIdx);
    } else if (ui->upscaylModelComboBox->count() > 0) {
      ui->upscaylModelComboBox->setCurrentIndex(0);
    }
  }

  ui->upscaylLimitCheckBox->setChecked(settings->upscaylLimitEnabled());
  ui->upscaylLimitSlider->setValue(settings->upscaylLimitValue());
  ui->upscaylLimitValueLabel->setText(QString::number(settings->upscaylLimitValue()) + "%");

  ui->upscaylLimitCheckBox->setEnabled(settings->useUpscayl());
  bool limitEnabled = settings->useUpscayl() && settings->upscaylLimitEnabled();
  ui->upscaylLimitSlider->setEnabled(limitEnabled);
  ui->upscaylLimitValueLabel->setEnabled(limitEnabled);
#endif

  ui->autoResizeWindowCheckBox->setChecked(settings->autoResizeWindow());
  ui->panelCenterSelectionCheckBox->setChecked(
      settings->panelCenterSelection());
  ui->showSubfoldersInPanelCheckBox->setChecked(
      settings->showSubfoldersInPanel());
  ui->useFixedZoomLevelsCheckBox->setChecked(settings->useFixedZoomLevels());
  ui->zoomLevels->setText(settings->zoomLevels());

  if (settings->defaultViewMode() == MODE_FOLDERVIEW)
    ui->startInFolderViewCheckBox->setChecked(true);
  else
    ui->startInFolderViewCheckBox->setChecked(false);

  if (settings->folderEndAction() == FOLDER_END_NO_ACTION)
    ui->folderEndNoAction->setChecked(true);
  else if (settings->folderEndAction() == FOLDER_END_LOOP)
    ui->folderEndLoop->setChecked(true);
  else
    ui->folderEndSwitchFolder->setChecked(true);

  ui->zoomStepSlider->setValue(static_cast<int>(settings->zoomStep() * 100.f));
  onZoomStepSliderChanged(ui->zoomStepSlider->value());

  ui->mouseScrollingSpeedSlider->setValue(
      static_cast<int>((settings->mouseScrollingSpeed() - 0.5f) / 0.25f));
  onMouseScrollingSpeedSliderChanged(ui->mouseScrollingSpeedSlider->value());

  ui->autoResizeLimitSlider->setValue(
      static_cast<int>(settings->autoResizeLimit() / 5.f));
  onAutoResizeLimitSliderChanged(ui->autoResizeLimitSlider->value());

  ui->JPEGQualitySlider->setValue(settings->JPEGSaveQuality());
  onJPEGQualitySliderChanged(ui->JPEGQualitySlider->value());

  pngQualitySlider->setValue(settings->pngSaveQuality());
  onPNGQualitySliderChanged(pngQualitySlider->value());

  modernQualitySlider->setValue(settings->modernSaveQuality());
  onModernQualitySliderChanged(modernQualitySlider->value());

  ui->expandLimitSlider->setValue(settings->expandLimit());
  onExpandLimitSliderChanged(ui->expandLimitSlider->value());

  // thumbnailer threads
  ui->thumbnailerThreadsSlider->setValue(settings->thumbnailerThreadCount());
  onThumbnailerThreadsSliderChanged(ui->thumbnailerThreadsSlider->value());

  ui->thumbnailResolutionSlider->setValue(settings->thumbnailResolution());
  onThumbnailResolutionSliderChanged(ui->thumbnailResolutionSlider->value());

  ui->memoryLimitSpinBox->setValue(settings->memoryAllocationLimit());
  ui->excludedCachePathsLineEdit->setText(settings->excludedCachePaths());

  // language
  QString langName = langs.value(settings->language());
  if (langName.isEmpty() || ui->langComboBox->findText(langName) == -1)
    ui->langComboBox->setCurrentText("en_US");
  else
    ui->langComboBox->setCurrentText(langName);

  // ##### fit mode #####
  if (settings->imageFitMode() == FIT_WINDOW)
    ui->fitModeWindow->setChecked(true);
  else if (settings->imageFitMode() == FIT_WIDTH)
    ui->fitModeWidth->setChecked(true);
  else if (settings->imageFitMode() == FIT_WINDOW_STRETCH)
    ui->fitModeWindowStretch->setChecked(true);
  else
    ui->fitMode1to1->setChecked(true);

  // ##### UI #####
  int filterIndex =
      ui->scalingQualityComboBox->findData(settings->scalingFilter());
  if (filterIndex != -1)
    ui->scalingQualityComboBox->setCurrentIndex(filterIndex);
  else
    ui->scalingQualityComboBox->setCurrentIndex(1); // default to Bilinear
  ui->fullscreenCheckBox->setChecked(settings->fullscreenMode());
  ui->pinPanelCheckBox->setChecked(settings->panelPinned());
  ui->panelPositionComboBox->setCurrentIndex(settings->panelPosition());

  // reduce by 8x to have nice granular control in qslider
  ui->panelSizeSlider->setValue(settings->panelPreviewsSize() / 8);

  ui->useSystemColorsCheckBox->setChecked(settings->useSystemColorScheme());
  ui->modifySystemSchemeLabel->setVisible(settings->useSystemColorScheme());
  ui->themeSelectorComboBox->setEnabled(!settings->useSystemColorScheme());
  ui->colorConfigSubgroup->setEnabled(!settings->useSystemColorScheme());

  readColorScheme();
  readShortcuts();
  readScripts();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettings() {
  // wait for all background stuff to finish
  if (QThreadPool::globalInstance()->activeThreadCount()) {
    QThreadPool::globalInstance()->waitForDone();
  }

  settings->setLoopSlideshow(ui->loopSlideshowCheckBox->isChecked());
  settings->setFullscreenMode(ui->fullscreenCheckBox->isChecked());
  if (ui->fitModeWindow->isChecked())
    settings->setImageFitMode(FIT_WINDOW);
  else if (ui->fitModeWidth->isChecked())
    settings->setImageFitMode(FIT_WIDTH);
  else if (ui->fitModeWindowStretch->isChecked())
    settings->setImageFitMode(FIT_WINDOW_STRETCH);
  else
    settings->setImageFitMode(FIT_ORIGINAL);

  settings->setLanguage(langs.key(ui->langComboBox->currentText()));

  settings->setPanelEnabled(ui->enablePanelCheckBox->isChecked());
  settings->setPanelFullscreenOnly(
      ui->panelFullscreenOnlyCheckBox->isChecked());
  settings->setSquareThumbnails(ui->squareThumbnailsCheckBox->isChecked());
  settings->setTransparencyGrid(ui->transparencyGridCheckBox->isChecked());
  settings->setShowHiddenFiles(ui->showHiddenFilesCheckBox->isChecked());
  settings->setEnableSmoothScroll(ui->enableSmoothScrollCheckBox->isChecked());
  settings->setEnableSmoothZoom(ui->enableSmoothZoomCheckBox->isChecked());
  settings->setUsePreloader(ui->usePreloaderCheckBox->isChecked());
  settings->setUseThumbnailCache(ui->useThumbnailCacheCheckBox->isChecked());
  settings->setSmoothUpscaling(ui->smoothUpscalingCheckBox->isChecked());
  settings->setExpandImage(ui->expandImageCheckBox->isChecked());
  settings->setSmoothAnimatedImages(
      ui->smoothAnimatedImagesCheckBox->isChecked());

  settings->setBackgroundOpacity(
      static_cast<qreal>(ui->bgOpacitySlider->value()) / 100.f);
  settings->setSortingMode(
      static_cast<SortingMode>(ui->sortingComboBox->currentIndex()));
  settings->setConfirmDelete(ui->confirmDeleteCheckBox->isChecked());
  settings->setConfirmTrash(ui->confirmTrashCheckBox->isChecked());
  settings->setUnlockMinZoom(ui->unlockMinZoomCheckBox->isChecked());
  settings->setSortFolders(ui->sortFoldersCheckBox->isChecked());
  settings->setTrackpadDetection(ui->trackpadDetectionCheckBox->isChecked());
  settings->setClickableEdges(ui->clickableEdgesCheckBox->isChecked());
  settings->setClickableEdgesVisible(
      ui->clickableEdgesVisibleCheckBox->isChecked());

  if (ui->zoomIndicatorOn->isChecked())
    settings->setZoomIndicatorMode(INDICATOR_ENABLED);
  else if (ui->zoomIndicatorAuto->isChecked())
    settings->setZoomIndicatorMode(INDICATOR_AUTO);
  else
    settings->setZoomIndicatorMode(INDICATOR_DISABLED);
  settings->setInfoBarFullscreen(ui->showInfoBarFullscreen->isChecked());
  settings->setInfoBarWindowed(ui->showInfoBarWindowed->isChecked());
  settings->setWindowTitleExtendedInfo(ui->showExtendedInfoTitle->isChecked());
  settings->setCursorAutohide(ui->cursorAutohideCheckBox->isChecked());
  settings->setKeepFitMode(ui->keepFitModeCheckBox->isChecked());
  if (ui->focus1to1Top->isChecked())
    settings->setFocusPointIn1to1Mode(FOCUS_TOP);
  else if (ui->focus1to1Center->isChecked())
    settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
  else
    settings->setFocusPointIn1to1Mode(FOCUS_CURSOR);

  settings->setSlideshowInterval(ui->slideshowIntervalSpinBox->value());

  if (ui->startInFolderViewCheckBox->isChecked())
    settings->setDefaultViewMode(MODE_FOLDERVIEW);
  else
    settings->setDefaultViewMode(MODE_DOCUMENT);

  if (ui->folderEndNoAction->isChecked())
    settings->setFolderEndAction(FOLDER_END_NO_ACTION);
  else if (ui->folderEndLoop->isChecked())
    settings->setFolderEndAction(FOLDER_END_LOOP);
  else
    settings->setFolderEndAction(FOLDER_END_GOTO_ADJACENT);

  settings->setScalingFilter(static_cast<ScalingFilter>(
      ui->scalingQualityComboBox->currentData().toInt()));
  settings->setImageScrolling(
      static_cast<ImageScrolling>(ui->imageScrollingComboBox->currentIndex()));
  settings->setShowSaveOverlay(ui->saveOverlayCheckBox->isChecked());
  settings->setUnloadThumbs(ui->unloadThumbsCheckBox->isChecked());
  if (ui->thumbStyleSimple->isChecked())
    settings->setThumbPanelStyle(TH_PANEL_SIMPLE);
  else
    settings->setThumbPanelStyle(TH_PANEL_EXTENDED);
  settings->setJxlAnimation(ui->animatedJxlCheckBox->isChecked());
  settings->setMultiInstance(ui->multiInstanceCheckBox->isChecked());
#ifdef USE_UPSCAYL
  settings->setUseUpscayl(ui->useUpscaylCheckBox->isChecked());
  settings->setPreloadUpscayl(ui->preloadUpscaylCheckBox->isChecked());
  settings->setUpscaylModel(ui->upscaylModelComboBox->currentText());
  settings->setUpscaylLimitEnabled(ui->upscaylLimitCheckBox->isChecked());
  settings->setUpscaylLimitValue(ui->upscaylLimitSlider->value());
#endif

  settings->setAutoResizeWindow(ui->autoResizeWindowCheckBox->isChecked());
  settings->setPanelCenterSelection(
      ui->panelCenterSelectionCheckBox->isChecked());
  settings->setShowSubfoldersInPanel(
      ui->showSubfoldersInPanelCheckBox->isChecked());
  settings->setUseFixedZoomLevels(ui->useFixedZoomLevelsCheckBox->isChecked());
  settings->setZoomLevels(ui->zoomLevels->text());

  settings->setPanelPinned(ui->pinPanelCheckBox->isChecked());
  int panelPos = ui->panelPositionComboBox->currentIndex();
  settings->setPanelPosition(static_cast<PanelPosition>(panelPos));

  settings->setPanelPreviewsSize(ui->panelSizeSlider->value() * 8);

  settings->setJPEGSaveQuality(ui->JPEGQualitySlider->value());
  settings->setPngSaveQuality(pngQualitySlider->value());
  settings->setModernSaveQuality(modernQualitySlider->value());
  settings->setZoomStep(
      static_cast<qreal>(ui->zoomStepSlider->value() / 100.f));
  settings->setMouseScrollingSpeed(static_cast<qreal>(
      0.5f + (ui->mouseScrollingSpeedSlider->value() * 0.25f)));
  settings->setAutoResizeLimit(ui->autoResizeLimitSlider->value() * 5);
  settings->setExpandLimit(ui->expandLimitSlider->value());
  settings->setThumbnailerThreadCount(ui->thumbnailerThreadsSlider->value());
  settings->setMemoryAllocationLimit(ui->memoryLimitSpinBox->value());
  settings->setExcludedCachePaths(ui->excludedCachePathsLineEdit->text());

  int oldRes = settings->thumbnailResolution();
  int newRes = ui->thumbnailResolutionSlider->value();
  if (oldRes != newRes) {
    settings->setThumbnailResolution(newRes);
    ThumbnailCache cache;
    cache.clear();
  }

  settings->setUseSystemColorScheme(ui->useSystemColorsCheckBox->isChecked());

  saveColorScheme();
  saveShortcuts();

  scriptManager->saveScripts();
  actionManager->saveShortcuts();
  emit settingsChanged();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettingsAndClose() {
  saveSettings();
  this->close();
}
//------------------------------------------------------------------------------
void SettingsDialog::readColorScheme() {
  auto colors = settings->colorScheme();
  setColorScheme(colors);
}

void SettingsDialog::setColorScheme(ColorScheme colors) {
  switch (colors.tid) {
  case COLORS_LIGHT:
    ui->themeSelectorComboBox->setCurrentIndex(3);
    break;
  case COLORS_BLACK:
    ui->themeSelectorComboBox->setCurrentIndex(0);
    break;
  case COLORS_DARK:
    ui->themeSelectorComboBox->setCurrentIndex(1);
    break;
  case COLORS_DARKBLUE:
    ui->themeSelectorComboBox->setCurrentIndex(2);
    break;
  default:
    ui->themeSelectorComboBox->setCurrentIndex(-1);
    break;
  }
  ui->colorSelectorAccent->setColor(colors.accent);
  ui->colorSelectorBackground->setColor(colors.background);
  ui->colorSelectorFullscreen->setColor(colors.background_fullscreen);
  ui->colorSelectorFolderview->setColor(colors.folderview);
  ui->colorSelectorFolderviewPanel->setColor(colors.folderview_topbar);
  ui->colorSelectorText->setColor(colors.text);
  ui->colorSelectorIcons->setColor(colors.icons);
  ui->colorSelectorWidget->setColor(colors.widget);
  ui->colorSelectorWidgetBorder->setColor(colors.widget_border);
  ui->colorSelectorOverlay->setColor(colors.overlay);
  ui->colorSelectorOverlayText->setColor(colors.overlay_text);
  ui->colorSelectorScrollbar->setColor(colors.scrollbar);
  ui->colorSelectorThumbpanel->setColor(colors.thumbpanel);
}

//------------------------------------------------------------------------------
void SettingsDialog::saveColorScheme() {
  BaseColorScheme base;
  base.accent = ui->colorSelectorAccent->color();
  base.background = ui->colorSelectorBackground->color();
  base.background_fullscreen = ui->colorSelectorFullscreen->color();
  base.folderview = ui->colorSelectorFolderview->color();
  base.folderview_topbar = ui->colorSelectorFolderviewPanel->color();
  base.text = ui->colorSelectorText->color();
  base.icons = ui->colorSelectorIcons->color();
  base.widget = ui->colorSelectorWidget->color();
  base.widget_border = ui->colorSelectorWidgetBorder->color();
  base.overlay = ui->colorSelectorOverlay->color();
  base.overlay_text = ui->colorSelectorOverlayText->color();
  base.scrollbar = ui->colorSelectorScrollbar->color();
  base.thumbpanel = ui->colorSelectorThumbpanel->color();
  base.tid = settings->colorScheme().tid;
  settings->setColorScheme(ColorScheme(base));
}
//------------------------------------------------------------------------------
void SettingsDialog::readShortcuts() {
  ui->shortcutsTableWidget->clearContents();
  ui->shortcutsTableWidget->setRowCount(0);
  const QMap<QString, QString> shortcuts = actionManager->allShortcuts();
  QMapIterator<QString, QString> i(shortcuts);
  while (i.hasNext()) {
    i.next();
    addShortcutToTable(i.value(), i.key());
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::readScripts() {
  ui->scriptsListWidget->clear();
  const QMap<QString, Script> scripts = scriptManager->allScripts();
  QMapIterator<QString, Script> i(scripts);
  while (i.hasNext()) {
    i.next();
    addScriptToList(i.key());
  }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addScriptToList(const QString &name) {
  if (name.isEmpty())
    return;

  QListWidget *list = ui->scriptsListWidget;
  QListWidgetItem *nameItem = new QListWidgetItem(name);
  nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  list->insertItem(ui->scriptsListWidget->count(), nameItem);
  list->sortItems(Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addScript() {
  ScriptEditorDialog w;
  if (w.exec()) {
    if (w.scriptName().isEmpty())
      return;
    scriptManager->addScript(w.scriptName(), w.script());
    readScripts();
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript() {
  int row = ui->scriptsListWidget->currentRow();
  if (row >= 0) {
    QString name = ui->scriptsListWidget->currentItem()->text();
    editScript(name);
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QListWidgetItem *item) {
  if (item) {
    editScript(item->text());
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QString name) {
  ScriptEditorDialog w(name, scriptManager->getScript(name));
  if (w.exec()) {
    if (w.scriptName().isEmpty())
      return;
    scriptManager->addScript(w.scriptName(), w.script());
    readScripts();
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeScript() {
  int row = ui->scriptsListWidget->currentRow();
  if (row >= 0) {
    QString scriptName = ui->scriptsListWidget->currentItem()->text();
    delete ui->scriptsListWidget->takeItem(row);
    saveShortcuts();
    actionManager->removeAllShortcuts("s:" + scriptName);
    readShortcuts();
    scriptManager->removeScript(scriptName);
  }
}
//------------------------------------------------------------------------------
// does not check if the shortcut already there
void SettingsDialog::addShortcutToTable(const QString &action,
                                        const QString &shortcut) {
  if (action.isEmpty() || shortcut.isEmpty())
    return;

  ui->shortcutsTableWidget->setRowCount(ui->shortcutsTableWidget->rowCount() +
                                        1);
  QTableWidgetItem *actionItem = new QTableWidgetItem(action);
  actionItem->setTextAlignment(Qt::AlignCenter);
  ui->shortcutsTableWidget->setItem(ui->shortcutsTableWidget->rowCount() - 1, 0,
                                    actionItem);
  QTableWidgetItem *shortcutItem = new QTableWidgetItem(shortcut);
  shortcutItem->setTextAlignment(Qt::AlignCenter);
  ui->shortcutsTableWidget->setItem(ui->shortcutsTableWidget->rowCount() - 1, 1,
                                    shortcutItem);
  // EFFICIENCY
  ui->shortcutsTableWidget->sortByColumn(0, Qt::AscendingOrder);
}
//------------------------------------------------------------------------------
void SettingsDialog::addShortcut() {
  ShortcutCreatorDialog w;
  if (!w.exec())
    return;
  for (int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
    if (ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut())
      removeShortcutAt(i);
  }
  addShortcutToTable(w.selectedAction(), w.selectedShortcut());
  // select
  auto items = ui->shortcutsTableWidget->findItems(w.selectedShortcut(),
                                                   Qt::MatchExactly);
  if (items.count()) {
    int newRow = ui->shortcutsTableWidget->row(items.at(0));
    ui->shortcutsTableWidget->selectRow(newRow);
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcutAt(int row) {
  if (row > 0 && row >= ui->shortcutsTableWidget->rowCount())
    return;
  ui->shortcutsTableWidget->removeRow(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut(int row) {
  if (row >= 0) {
    ShortcutCreatorDialog w;
    w.setWindowTitle(tr("Edit shortcut"));
    w.setAction(ui->shortcutsTableWidget->item(row, 0)->text());
    w.setShortcut(ui->shortcutsTableWidget->item(row, 1)->text());
    if (!w.exec())
      return;
    // remove itself
    removeShortcutAt(row);
    // remove anything we are replacing
    for (int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
      if (ui->shortcutsTableWidget->item(i, 1)->text() == w.selectedShortcut())
        removeShortcutAt(i);
    }
    // re-add
    addShortcutToTable(w.selectedAction(), w.selectedShortcut());
    // re-select
    auto items = ui->shortcutsTableWidget->findItems(w.selectedShortcut(),
                                                     Qt::MatchExactly);
    if (items.count()) {
      int newRow = ui->shortcutsTableWidget->row(items.at(0));
      ui->shortcutsTableWidget->selectRow(newRow);
    }
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut() {
  editShortcut(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcut() {
  removeShortcutAt(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::saveShortcuts() {
  actionManager->removeAllShortcuts();
  for (int i = 0; i < ui->shortcutsTableWidget->rowCount(); i++) {
    actionManager->addShortcut(ui->shortcutsTableWidget->item(i, 1)->text(),
                               ui->shortcutsTableWidget->item(i, 0)->text());
  }
}
//------------------------------------------------------------------------------
void SettingsDialog::resetShortcuts() {
  actionManager->resetDefaults();
  readShortcuts();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetZoomLevels() {
  ui->zoomLevels->setText(settings->defaultZoomLevels());
}
//------------------------------------------------------------------------------
void SettingsDialog::onExpandLimitSliderChanged(int value) {
  if (value == 0)
    ui->expandLimitLabel->setText("-");
  else
    ui->expandLimitLabel->setText(QString::number(value) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onJPEGQualitySliderChanged(int value) {
  ui->JPEGQualityLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onPNGQualitySliderChanged(int value) {
  QString desc;
  if (value == 0) desc = tr("None (Uncompressed)");
  else if (value <= 3) desc = tr("Fast");
  else if (value <= 6) desc = tr("Balanced");
  else desc = tr("Maximum");
  pngQualityLabel->setText(QString("Level %1 (%2)").arg(value).arg(desc));
}
//------------------------------------------------------------------------------
void SettingsDialog::onModernQualitySliderChanged(int value) {
  modernQualityLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onZoomStepSliderChanged(int value) {
  ui->zoomStepLabel->setText(QString::number(value / 100.f, 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onMouseScrollingSpeedSliderChanged(int value) {
  ui->mouseScrollingSpeedLabel->setText(
      QString::number(0.5f + (value * 0.25f), 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailResolutionSliderChanged(int value) {
  // Snap value to nearest multiple of 16
  int snapped = ((value + 8) / 16) * 16;
  if (snapped != value) {
    ui->thumbnailResolutionSlider->setValue(snapped);
    return;
  }
  ui->thumbnailResolutionValueLabel->setText(QString::number(snapped) + " px");
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailerThreadsSliderChanged(int value) {
  ui->thumbnailerThreadsLabel->setText(QString::number(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onBgOpacitySliderChanged(int value) {
  ui->bgOpacityPercentLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onAutoResizeLimitSliderChanged(int value) {
  ui->autoResizeLimit->setText(QString::number(value * 5.f, 'f', 0) + "%");
}
//------------------------------------------------------------------------------
int SettingsDialog::exec() { return QDialog::exec(); }

void SettingsDialog::switchToPage(int number) {
  ui->sideBar2->selectEntry(number);
}
