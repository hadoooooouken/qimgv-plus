#include "settingsdialog.h"
#include "settings.h"
#include "components/settingseditor/settingsoptions.h"
#include "components/settingseditor/settingsscales.h"
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelSliderMinimumWidth = 180;

void fillComboBox(QComboBox *comboBox, const SettingsOptionList &options) {
  comboBox->clear();
  for (const SettingsOption &option : options)
    comboBox->addItem(option.text, option.value);
}

void selectData(QComboBox *comboBox, const QVariant &value) {
  const int index = comboBox->findData(value);
  if (index != -1)
    comboBox->setCurrentIndex(index);
}

void setRange(QSlider *slider, const SettingsRange &range) {
  slider->setRange(range.from, range.to);
  slider->setSingleStep(range.step);
  slider->setPageStep(range.pageStep);
}

void setRange(QSpinBox *spinBox, const SettingsRange &range) {
  spinBox->setRange(range.from, range.to);
  spinBox->setSingleStep(range.step);
}
} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent),
      mValueStore(*settings, *actionManager, *scriptManager),
      mShortcutStore(*actionManager, *scriptManager),
      mEditor(mValueStore, mShortcutStore) {
  setupUi();
  retranslateUi();
  mEditor.load();

  const SettingsRanges ranges = SettingsEditorModel::ranges();

  connect(&mValueStore, &AppSettingsStore::clearThumbnailCacheRequested, this,
          &SettingsDialog::clearThumbnailCacheRequested);
  connect(&mEditor, &SettingsEditorModel::shortcutPut, this,
          [this](int row) { shortcutsTableView->selectRow(row); });

  connect(useUpscaylCheckBox, &QCheckBox::toggled,
          preloadUpscaylCheckBox, &QCheckBox::setEnabled);
  connect(useUpscaylCheckBox, &QCheckBox::toggled, upscaylModelComboBox,
          &QComboBox::setEnabled);
  connect(useUpscaylCheckBox, &QCheckBox::toggled, label_upscaylModel,
          &QLabel::setEnabled);
  // label_upscaylGetModels is intentionally not tied to useUpscayl state:
  // it is a plain external link and must stay clickable even when
  // upscaling is disabled (e.g. when no models are installed yet).
  connect(useUpscaylCheckBox, &QCheckBox::toggled, upscaylLimitCheckBox,
          &QCheckBox::setEnabled);

  const auto updateLimitControls = [this]() {
    const bool enabled = useUpscaylCheckBox->isChecked() &&
                         upscaylLimitCheckBox->isChecked();
    upscaylLimitSlider->setEnabled(enabled);
    upscaylLimitValueLabel->setEnabled(enabled);
  };
  connect(useUpscaylCheckBox, &QCheckBox::toggled, this,
          updateLimitControls);
  connect(upscaylLimitCheckBox, &QCheckBox::toggled, this,
          updateLimitControls);

  setRange(upscaylLimitSlider, ranges.upscaylLimit);
  connect(upscaylLimitSlider, &QSlider::valueChanged, this,
          [this](int value) {
            const int snapped =
                SettingsScales::snapped(value, upscaylLimitSlider->singleStep());
            if (snapped != value) {
              upscaylLimitSlider->setValue(snapped);
              return;
            }
            upscaylLimitValueLabel->setText(SettingsScales::percentText(snapped));
          });

  upscaylModelComboBox->addItems(mEditor.upscaylModels());

  setRange(panelSizeSlider, ranges.panelSize);
  setRange(panelHideDelaySlider, ranges.panelHideDelay);
  connect(panelHideDelaySlider, &QSlider::valueChanged, this,
          [this](int value) {
            const int snapped =
                SettingsScales::snapped(value, panelHideDelaySlider->singleStep());
            if (snapped != value) {
              panelHideDelaySlider->setValue(snapped);
              return;
            }
            panelHideDelayValueLabel->setText(
                SettingsScales::panelHideDelayText(snapped));
          });
  this->setWindowTitle(SettingsEditorModel::windowTitle());

  shortcutsTableView->setModel(mEditor.shortcutTable());
  shortcutsTableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  scriptsListView->setModel(mEditor.scriptList());
  aboutAppTextBrowser->viewport()->setAutoFillBackground(false);
  versionLabel->setText(SettingsEditorModel::applicationVersion());
  qtVersionLabel->setText(SettingsEditorModel::qtVersion());
  appIconLabel->setPixmap(
      QIcon(":/res/icons/common/logo/app/qimgv.svg").pixmap(22, 22));
  qtIconLabel->setPixmap(
      QIcon(":/res/icons/common/logo/3rdparty/qt.svg").pixmap(22, 16));

  // Theme mode selector
  loadPresetLabel->setText(tr("Theme mode:"));
  fillComboBox(themeSelectorComboBox, SettingsOptions::themeModes());

  connect(themeSelectorComboBox,
          qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (index < 0)
              return;
            mEditor.setThemeMode(themeSelectorComboBox->itemData(index).toInt());
            showAccentColor();
          });

  // Hide unused checkboxes and labels
  useSystemColorsCheckBox->hide();
  modifySystemSchemeLabel->hide();

  // Hide all color selectors except Accent
  colorSelectorBackground->hide();
  label_34->hide();
  colorSelectorFullscreen->hide();
  label_35->hide();
  colorSelectorText->hide();
  label_11->hide();
  colorSelectorIcons->hide();
  label_14->hide();
  colorSelectorFolderview->hide();
  label_36->hide();
  colorSelectorFolderviewPanel->hide();
  label_21->hide();
  colorSelectorWidget->hide();
  label_31->hide();
  colorSelectorWidgetBorder->hide();
  label_22->hide();
  colorSelectorOverlay->hide();
  label_37->hide();
  colorSelectorOverlayText->hide();
  label_32->hide();
  colorSelectorScrollbar->hide();
  label_23->hide();
  colorSelectorThumbpanel->hide();
  label_thumbpanel->hide();

  // "Use custom accent" checkbox
  gridLayout_2->removeWidget(colorSelectorAccent);
  gridLayout_2->removeWidget(label_33);
  label_33->hide();

  QHBoxLayout *accentLayout = new QHBoxLayout();
  accentLayout->setContentsMargins(0, 0, 0, 0);
  accentLayout->setSpacing(12);

  useCustomAccentCheckBox = new QCheckBox(tr("Use custom accent"), this);

  accentLayout->addWidget(useCustomAccentCheckBox);
  accentLayout->addWidget(colorSelectorAccent);
  accentLayout->addStretch(1);

  gridLayout_2->addLayout(accentLayout, 0, 0, 1, 5);

  connect(useCustomAccentCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) {
            colorSelectorAccent->setEnabled(checked);
            mEditor.setCustomAccent(checked);
            showAccentColor();
          });

  // Accent color changes are previewed and saved at once
  connect(colorSelectorAccent, &ColorSelectorButton::colorChanged, this,
          [this](QColor color) { mEditor.setAccentColor(color); });

  // Align opacity labels to make sliders line up perfectly
  label_5->setMinimumWidth(140);
  label_5_thumb->setMinimumWidth(140);
  setRange(bgOpacitySlider, ranges.opacity);
  setRange(thumbOpacitySlider, ranges.opacity);

  // Connect thumbnail opacity slider
  connect(thumbOpacitySlider, &QSlider::valueChanged, this,
          &SettingsDialog::onThumbOpacitySliderChanged);
  connect(thumbOpacitySlider, &QSlider::sliderReleased, this,
          &SettingsDialog::onThumbOpacitySliderReleased);

  connect(useBlackBackgroundCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) {
            mEditor.setUseBlackBackground(checked);
            showAccentColor();
          });

  colorSelectorAccent->setDescription(tr("Accent color"));
  colorSelectorBackground->setDescription(tr("Windowed mode background"));
  colorSelectorFullscreen->setDescription(tr("Fullscreen mode background"));
  colorSelectorFolderview->setDescription(tr("FolderView background"));
  colorSelectorFolderviewPanel->setDescription(tr("FolderView top panel"));
  colorSelectorText->setDescription(tr("Text color"));
  colorSelectorWidget->setDescription(tr("Widget background"));
  colorSelectorWidgetBorder->setDescription(tr("Widget border"));
  colorSelectorOverlay->setDescription(tr("Overlay background"));
  colorSelectorOverlayText->setDescription(tr("Overlay text"));
  colorSelectorScrollbar->setDescription(tr("Scrollbars"));
  colorSelectorThumbpanel->setDescription(tr("Thumbnail panel"));
  colorSelectorThumbpanel->setShowAlpha(true);

  fillComboBox(scalingQualityComboBox, SettingsOptions::scalingFilters());

  casContainerWidget = new QWidget(this);
  QGridLayout *casLayout = new QGridLayout(casContainerWidget);
  casLayout->setContentsMargins(12, 0, 0, 0);
  casLayout->setSpacing(6);

  QLabel *sharpLabel = new QLabel(tr("Sharpness:"), this);
  casSharpeningSlider = new QSlider(Qt::Horizontal, this);
  setRange(casSharpeningSlider, ranges.casSharpening);
  casSharpeningSlider->setFixedWidth(170);
  casSharpeningLabel = new QLabel(this);
  casSharpeningLabel->setFixedWidth(30);

  QLabel *contrastLabel = new QLabel(tr("Contrast:"), this);
  casContrastSlider = new QSlider(Qt::Horizontal, this);
  setRange(casContrastSlider, ranges.casContrast);
  casContrastSlider->setFixedWidth(170);
  casContrastLabel = new QLabel(this);
  casContrastLabel->setFixedWidth(30);

  casLayout->addWidget(sharpLabel, 0, 0);
  casLayout->addWidget(casSharpeningSlider, 0, 1);
  casLayout->addWidget(casSharpeningLabel, 0, 2);
  casLayout->addWidget(contrastLabel, 1, 0);
  casLayout->addWidget(casContrastSlider, 1, 1);
  casLayout->addWidget(casContrastLabel, 1, 2);

  casLayout->setColumnStretch(3, 1);

  int layoutIdx = verticalLayout_24->indexOf(horizontalLayout_5);
  verticalLayout_24->insertWidget(layoutIdx + 1, casContainerWidget);

  auto updateCasVisibility = [this]() {
    bool isCas = (scalingQualityComboBox->currentData().toInt() == QI_FILTER_CAS);
    casContainerWidget->setVisible(isCas);
  };

  connect(scalingQualityComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [updateCasVisibility](int) {
    updateCasVisibility();
  });

  connect(casSharpeningSlider, &QSlider::valueChanged, this, [this](int val) {
    casSharpeningLabel->setText(SettingsScales::casValueText(val));
  });
  connect(casContrastSlider, &QSlider::valueChanged, this, [this](int val) {
    casContrastLabel->setText(SettingsScales::casValueText(val));
  });

  setupSidebar();

  // setup radioBtn groups
  fitModeGrp.addButton(fitModeWindow);
  fitModeGrp.addButton(fitModeWidth);
  fitModeGrp.addButton(fitMode1to1);
  fitModeGrp.addButton(fitModeHeight);
  folderEndGrp.addButton(folderEndSwitchFolder);
  folderEndGrp.addButton(folderEndNoAction);
  folderEndGrp.addButton(folderEndLoop);
  zoomIndGrp.addButton(zoomIndicatorAuto);
  zoomIndGrp.addButton(zoomIndicatorOff);
  zoomIndGrp.addButton(zoomIndicatorOn);

  // readable language names, the system language first
  fillComboBox(langComboBox, SettingsOptions::languages());
  fillComboBox(sortingComboBox, SettingsOptions::sortingModes());
  fillComboBox(panelPositionComboBox, SettingsOptions::panelPositions());
  fillComboBox(imageScrollingComboBox, SettingsOptions::imageScrollingModes());

  setRange(autoResizeLimitSlider, ranges.autoResizeLimit);
  slideshowIntervalSpinBox->setRange(ranges.slideshowInterval.from,
                                     ranges.slideshowInterval.to);
  setRange(expandLimitSlider, ranges.expandLimit);
  setRange(zoomStepSlider, ranges.zoomStep);
  setRange(mouseScrollingSpeedSlider, ranges.mouseScrollingSpeed);
  setRange(thumbnailerThreadsSlider, ranges.thumbnailerThreads);
  setRange(thumbnailResolutionSlider, ranges.thumbnailResolution);
  setRange(thumbnailCacheQuotaSpinBox, ranges.thumbnailCacheSize);
  setRange(JPEGQualitySlider, ranges.quality);
  setRange(memoryLimitSpinBox, ranges.memoryLimit);

  connect(thumbnailResolutionSlider, &QSlider::valueChanged, this,
          &SettingsDialog::onThumbnailResolutionSliderChanged);

  // Modern formats quality row
  QHBoxLayout *modernLayout = new QHBoxLayout();
  modernLayout->setContentsMargins(0, 0, 0, 0);
  QLabel *modernTitleLabel =
      new QLabel(tr("Modern formats quality (WebP, JXL, AVIF):"), this);
  modernQualitySlider = new QSlider(Qt::Horizontal, this);
  modernQualitySlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  modernQualitySlider->setMinimumSize(180, 25);
  setRange(modernQualitySlider, ranges.quality);
  modernQualitySlider->setTickPosition(QSlider::TicksBelow);
  modernQualitySlider->setTickInterval(10);
  modernQualityLabel = new QLabel(this);
  QSpacerItem *modernSpacer =
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

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
  setRange(pngQualitySlider, ranges.pngCompression);
  pngQualitySlider->setTickPosition(QSlider::TicksBelow);
  pngQualitySlider->setTickInterval(1);
  pngQualityLabel = new QLabel(this);
  QSpacerItem *pngSpacer =
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

  pngLayout->addWidget(pngTitleLabel);
  pngLayout->addWidget(pngQualitySlider);
  pngLayout->addWidget(pngQualityLabel);
  pngLayout->addSpacerItem(pngSpacer);

  // Connect signals
  connect(pngQualitySlider, &QSlider::valueChanged, this,
          &SettingsDialog::onPNGQualitySliderChanged);
  connect(modernQualitySlider, &QSlider::valueChanged, this,
          &SettingsDialog::onModernQualitySliderChanged);

  // Insert into vertical layout right after JPEG save quality row
  // (horizontalLayout_10)
  int idx = verticalLayout_34->indexOf(horizontalLayout_10);
  if (idx != -1) {
    verticalLayout_34->insertLayout(idx + 1, modernLayout);
    verticalLayout_34->insertLayout(idx + 2, pngLayout);
  } else {
    verticalLayout_34->addLayout(modernLayout);
    verticalLayout_34->addLayout(pngLayout);
  }

  // --- Color Management GroupBox ---
  colorManagementGroupBox = new QGroupBox(tr("Color Management"), this);
  QVBoxLayout *cmLayout = new QVBoxLayout(colorManagementGroupBox);
  cmLayout->setContentsMargins(10, 10, 10, 10);
  cmLayout->setSpacing(8);

  colorManagementCheckBox = new QCheckBox(tr("Enable color management"), this);
  cmLayout->addWidget(colorManagementCheckBox);

  // Monitor Profile row
  QHBoxLayout *profileRowLayout = new QHBoxLayout();
  QLabel *profileLabel = new QLabel(tr("Monitor profile:"), this);
  monitorProfileComboBox = new QComboBox(this);
  fillComboBox(monitorProfileComboBox, SettingsOptions::monitorProfiles());
  profileRowLayout->addWidget(profileLabel);
  profileRowLayout->addWidget(monitorProfileComboBox);
  profileRowLayout->addStretch(1);
  cmLayout->addLayout(profileRowLayout);

  // Custom Profile selection controls
  customProfileContainer = new QWidget(this);
  QHBoxLayout *customProfileLayout = new QHBoxLayout(customProfileContainer);
  customProfileLayout->setContentsMargins(0, 0, 0, 0);
  customProfileLayout->setSpacing(6);

  QLabel *customLabel = new QLabel(tr("Profile file:"), this);
  customProfilePathEdit = new QLineEdit(this);
  customProfilePathEdit->setReadOnly(true);
  customProfileBrowseButton = new QPushButton(tr("Browse..."), this);

  customProfileLayout->addWidget(customLabel);
  customProfileLayout->addWidget(customProfilePathEdit);
  customProfileLayout->addWidget(customProfileBrowseButton);
  cmLayout->addWidget(customProfileContainer);

  // Add the whole GroupBox to the View page scroll area contents layout
  if (scrollAreaWidgetContents_3->layout()) {
    scrollAreaWidgetContents_3->layout()->addWidget(
        colorManagementGroupBox);
  }

  connect(colorManagementCheckBox, &QCheckBox::toggled, this,
          &SettingsDialog::updateColorManagementControls);
  connect(monitorProfileComboBox,
          qOverload<int>(&QComboBox::currentIndexChanged), this,
          &SettingsDialog::updateColorManagementControls);

  connect(customProfileBrowseButton, &QPushButton::clicked, this, [this]() {
    QString path = QFileDialog::getOpenFileName(
        this, SettingsEditorModel::colorProfileDialogTitle(), QString(),
        SettingsEditorModel::colorProfileFilters().join(QStringLiteral(";;")));
    if (!path.isEmpty()) {
      customProfilePathEdit->setText(path);
    }
  });

  // --- HDR Tone-Mapping GroupBox ---
  hdrGroupBox = new QGroupBox(tr("HDR Tone-Mapping"), this);
  QVBoxLayout *hdrLayout = new QVBoxLayout(hdrGroupBox);
  hdrLayout->setContentsMargins(10, 10, 10, 10);
  hdrLayout->setSpacing(8);

  hdrToneMappingCheckBox = new QCheckBox(tr("Enable HDR Tone-Mapping"), this);
  hdrLayout->addWidget(hdrToneMappingCheckBox);

  // Tone-Mapping Operator row
  QHBoxLayout *operatorRowLayout = new QHBoxLayout();
  QLabel *operatorLabel = new QLabel(tr("Tone-mapping operator:"), this);
  hdrOperatorComboBox = new QComboBox(this);
  fillComboBox(hdrOperatorComboBox, SettingsOptions::hdrOperators());
  operatorRowLayout->addWidget(operatorLabel);
  operatorRowLayout->addWidget(hdrOperatorComboBox);
  operatorRowLayout->addStretch(1);
  hdrLayout->addLayout(operatorRowLayout);

  // Target White Level row
  QHBoxLayout *whiteLevelRowLayout = new QHBoxLayout();
  QLabel *whiteLevelLabel = new QLabel(tr("Target white level:"), this);
  hdrTargetWhiteComboBox = new QComboBox(this);
  fillComboBox(hdrTargetWhiteComboBox, SettingsOptions::hdrTargetWhiteLevels());
  whiteLevelRowLayout->addWidget(whiteLevelLabel);
  whiteLevelRowLayout->addWidget(hdrTargetWhiteComboBox);
  whiteLevelRowLayout->addStretch(1);
  hdrLayout->addLayout(whiteLevelRowLayout);

  if (scrollAreaWidgetContents_3->layout()) {
    scrollAreaWidgetContents_3->layout()->addWidget(hdrGroupBox);
  }

  connect(hdrToneMappingCheckBox, &QCheckBox::toggled, this,
          &SettingsDialog::updateHdrControls);

  readSettings();

  adjustSizeToContents();
}
//------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog() {  }
//------------------------------------------------------------------------------
// an attempt to force minimum width to fit contents
void SettingsDialog::adjustSizeToContents() {
  // general tab
  gridLayout->activate();
  horizontalLayout_28->activate();
  horizontalLayout_19->activate();
  gridLayout_3->activate();
  horizontalLayout_18->activate();
  gridLayout_4->activate();
  horizontalLayout_24->activate();
  gridLayout_5->activate();
  slideshowGroupContents->activate();
  scrollAreaWidgetContents->layout()->activate();
  scrollArea->setMinimumWidth(
      scrollAreaWidgetContents->minimumSizeHint().width());
  // view tab
  horizontalLayout_29->activate();
  horizontalLayout_31->activate();
  widget->layout()->activate();
  scrollAreaWidgetContents_3->layout()->activate();
  scrollArea_3->setMinimumWidth(
      scrollAreaWidgetContents_3->minimumSizeHint().width());
  // container
  this->setMinimumWidth(sizeHint().width() + 22);
}
//------------------------------------------------------------------------------
void SettingsDialog::setupSidebar() {}
//------------------------------------------------------------------------------
void SettingsDialog::updateColorManagementControls() {
  const bool cmEnabled = colorManagementCheckBox->isChecked();
  monitorProfileComboBox->setEnabled(cmEnabled);
  const bool customSelected = monitorProfileComboBox->currentData().toString() ==
                              SettingsOptions::kCustomMonitorProfile;
  customProfileContainer->setVisible(cmEnabled && customSelected);
  customProfilePathEdit->setEnabled(cmEnabled);
  customProfileBrowseButton->setEnabled(cmEnabled);
}
//------------------------------------------------------------------------------
void SettingsDialog::updateHdrControls() {
  const bool hdrEnabled = hdrToneMappingCheckBox->isChecked();
  hdrOperatorComboBox->setEnabled(hdrEnabled);
  hdrTargetWhiteComboBox->setEnabled(hdrEnabled);
}
//------------------------------------------------------------------------------
// Shows the editor's values. The previewed theme controls do not preview
// what is only shown.
void SettingsDialog::readSettings() {
  const QSignalBlocker themeBlocker(themeSelectorComboBox);
  const QSignalBlocker opacityBlocker(thumbOpacitySlider);
  const QSignalBlocker blackBlocker(useBlackBackgroundCheckBox);
  const QSignalBlocker accentBlocker(useCustomAccentCheckBox);

  const GeneralSettings general = mEditor.general();
  const ViewSettings view = mEditor.view();
  const ThemeSettings theme = mEditor.theme();
  const ControlsSettings controls = mEditor.controls();
  const AdvancedSettings advanced = mEditor.advanced();
  const UpscaleSettings upscale = mEditor.upscale();

  // ##### general #####
  selectData(langComboBox, general.language);
  fullscreenCheckBox->setChecked(general.fullscreenMode);
  startInFolderViewCheckBox->setChecked(general.startInFolderView);
  standbyCheckBox->setChecked(general.standbyMode);
  rememberLastFolderCheckBox->setChecked(general.rememberLastFolder);
  showExtendedInfoTitle->setChecked(general.windowTitleExtendedInfo);
  showInfoBarFullscreen->setChecked(general.infoBarFullscreen);
  cursorAutohideCheckBox->setChecked(general.cursorAutohide);
  enableSmoothScrollCheckBox->setChecked(general.smoothScroll);
  enableSmoothZoomCheckBox->setChecked(general.smoothZoom);
  if (general.zoomIndicatorMode == INDICATOR_ENABLED)
    zoomIndicatorOn->setChecked(true);
  else if (general.zoomIndicatorMode == INDICATOR_AUTO)
    zoomIndicatorAuto->setChecked(true);
  else
    zoomIndicatorOff->setChecked(true);
  autoResizeWindowCheckBox->setChecked(general.autoResizeWindow);
  autoResizeLimitSlider->setValue(general.autoResizeLimitStep);
  onAutoResizeLimitSliderChanged(autoResizeLimitSlider->value());

  enablePanelCheckBox->setChecked(general.panelEnabled);
  thumbnailPanelGroupContents->setEnabled(general.panelEnabled);
  squareThumbnailsCheckBox->setChecked(general.squareThumbnails);
  pinPanelCheckBox->setChecked(general.panelPinned);
  panelFullscreenOnlyCheckBox->setChecked(general.panelFullscreenOnly);
  panelCenterSelectionCheckBox->setChecked(general.panelCenterSelection);
  showSubfoldersInPanelCheckBox->setChecked(general.showSubfoldersInPanel);
  panelHideDelaySlider->setValue(general.panelHideDelayMs);
  panelHideDelayValueLabel->setText(
      SettingsScales::panelHideDelayText(panelHideDelaySlider->value()));
  if (general.thumbPanelStyle == TH_PANEL_SIMPLE)
    thumbStyleSimple->setChecked(true);
  else
    thumbStyleExtended->setChecked(true);
  panelSizeSlider->setValue(general.panelSizeStep);
  selectData(panelPositionComboBox, general.panelPosition);

  if (general.folderEndAction == FOLDER_END_NO_ACTION)
    folderEndNoAction->setChecked(true);
  else if (general.folderEndAction == FOLDER_END_LOOP)
    folderEndLoop->setChecked(true);
  else
    folderEndSwitchFolder->setChecked(true);
  selectData(sortingComboBox, general.sortingMode);
  sortFoldersCheckBox->setChecked(general.sortFolders);
  showHiddenFilesCheckBox->setChecked(general.showHiddenFiles);
  slideshowIntervalSpinBox->setValue(general.slideshowIntervalMs);
  loopSlideshowCheckBox->setChecked(general.loopSlideshow);

  // ##### view #####
  if (view.imageFitMode == FIT_WINDOW)
    fitModeWindow->setChecked(true);
  else if (view.imageFitMode == FIT_WIDTH)
    fitModeWidth->setChecked(true);
  else if (view.imageFitMode == FIT_HEIGHT)
    fitModeHeight->setChecked(true);
  else
    fitMode1to1->setChecked(true);
  keepFitModeCheckBox->setChecked(view.keepFitMode);
  if (view.focusPoint == FOCUS_TOP)
    focus1to1Top->setChecked(true);
  else if (view.focusPoint == FOCUS_CENTER)
    focus1to1Center->setChecked(true);
  else
    focus1to1Cursor->setChecked(true);
  transparencyGridCheckBox->setChecked(view.transparencyGrid);
  expandImageCheckBox->setChecked(view.expandImage);
  expandImagesGroupContents->setEnabled(view.expandImage);
  expandLimitSlider->setValue(view.expandLimit);
  onExpandLimitSliderChanged(expandLimitSlider->value());
  unlockMinZoomCheckBox->setChecked(view.unlockMinZoom);
  zoomStepSlider->setValue(view.zoomStepPercent);
  onZoomStepSliderChanged(zoomStepSlider->value());
  useFixedZoomLevelsCheckBox->setChecked(view.useFixedZoomLevels);
  zoomLevels->setText(view.zoomLevels);

  selectData(scalingQualityComboBox, view.scalingFilter);
  casSharpeningSlider->setValue(view.casSharpeningPercent);
  casSharpeningLabel->setText(SettingsScales::casValueText(casSharpeningSlider->value()));
  casContrastSlider->setValue(view.casContrastPercent);
  casContrastLabel->setText(SettingsScales::casValueText(casContrastSlider->value()));
  casContainerWidget->setVisible(mEditor.casOptionsVisible());

  colorManagementCheckBox->setChecked(view.colorManagementEnabled);
  selectData(monitorProfileComboBox, view.monitorProfileType);
  customProfilePathEdit->setText(view.monitorProfilePath);
  updateColorManagementControls();

  hdrToneMappingCheckBox->setChecked(view.hdrToneMappingEnabled);
  selectData(hdrOperatorComboBox, view.hdrOperator);
  selectData(hdrTargetWhiteComboBox, view.hdrTargetWhiteLevel);
  updateHdrControls();

  // ##### theme #####
  selectData(themeSelectorComboBox, theme.themeMode);
  useCustomAccentCheckBox->setChecked(theme.customAccent);
  colorSelectorAccent->setEnabled(theme.customAccent);
  showAccentColor();
  bgOpacitySlider->setValue(theme.backgroundOpacityPercent);
  onBgOpacitySliderChanged(bgOpacitySlider->value());
  thumbOpacitySlider->setValue(theme.thumbnailOpacityPercent);
  thumbOpacityPercentLabel->setText(
      SettingsScales::percentText(thumbOpacitySlider->value()));
  useBlackBackgroundCheckBox->setChecked(theme.useBlackBackground);

  // ##### controls #####
  clickableEdgesCheckBox->setChecked(controls.clickableEdges);
  clickableEdgesVisibleCheckBox->setChecked(controls.clickableEdgesVisible);
  clickableEdgesVisibleCheckBox->setEnabled(controls.clickableEdges);
  selectData(imageScrollingComboBox, controls.imageScrolling);
  mouseScrollingSpeedSlider->setValue(controls.mouseScrollingSpeedStep);
  onMouseScrollingSpeedSliderChanged(mouseScrollingSpeedSlider->value());
  trackpadDetectionCheckBox->setChecked(controls.trackpadDetection);

  // ##### advanced #####
  usePreloaderCheckBox->setChecked(advanced.usePreloader);
  thumbnailerThreadsSlider->setValue(advanced.thumbnailerThreads);
  onThumbnailerThreadsSliderChanged(thumbnailerThreadsSlider->value());
  useThumbnailCacheCheckBox->setChecked(advanced.useThumbnailCache);
  thumbnailResolutionSlider->setValue(advanced.thumbnailResolution);
  onThumbnailResolutionSliderChanged(thumbnailResolutionSlider->value());
  thumbnailCacheQuotaSpinBox->setValue(advanced.thumbnailCacheMaxSizeMB);
  thumbnailCacheSizeValueLabel->setText(mEditor.thumbnailCacheSizeText());
  excludedCachePathsLineEdit->setText(advanced.excludedCachePaths);
  unloadThumbsCheckBox->setChecked(advanced.unloadThumbs);
  saveOverlayCheckBox->setChecked(advanced.showSaveOverlay);
  JPEGQualitySlider->setValue(advanced.jpegQuality);
  onJPEGQualitySliderChanged(JPEGQualitySlider->value());
  modernQualitySlider->setValue(advanced.modernQuality);
  onModernQualitySliderChanged(modernQualitySlider->value());
  pngQualitySlider->setValue(advanced.pngCompression);
  onPNGQualitySliderChanged(pngQualitySlider->value());
  confirmTrashCheckBox->setChecked(advanced.confirmTrash);
  confirmDeleteCheckBox->setChecked(advanced.confirmDelete);
  multiInstanceCheckBox->setChecked(advanced.multiInstance);
  memoryLimitSpinBox->setValue(advanced.memoryLimitMB);

  // ##### AI upscale #####
  const bool available = mEditor.isUpscaylAvailable();
  useUpscaylCheckBox->setChecked(upscale.useUpscayl);
  useUpscaylCheckBox->setEnabled(available);
  if (!available)
    useUpscaylCheckBox->setToolTip(tr("No AI models found in models/ directory."));
  preloadUpscaylCheckBox->setChecked(upscale.preloadUpscayl);
  preloadUpscaylCheckBox->setEnabled(mEditor.upscaylOptionsEnabled());
  upscaylModelComboBox->setCurrentIndex(upscaylModelComboBox->findText(upscale.model));
  upscaylModelComboBox->setEnabled(mEditor.upscaylOptionsEnabled());
  label_upscaylModel->setEnabled(mEditor.upscaylOptionsEnabled());
  upscaylLimitCheckBox->setChecked(upscale.limitEnabled);
  upscaylLimitCheckBox->setEnabled(mEditor.upscaylOptionsEnabled());
  upscaylLimitSlider->setValue(upscale.limitPercent);
  upscaylLimitValueLabel->setText(SettingsScales::percentText(upscale.limitPercent));
  upscaylLimitSlider->setEnabled(mEditor.upscaylLimitSliderEnabled());
  upscaylLimitValueLabel->setEnabled(mEditor.upscaylLimitSliderEnabled());
}
//------------------------------------------------------------------------------
SettingsValues SettingsDialog::collectSettings() const {
  SettingsValues values;

  GeneralSettings &general = values.general;
  general.language = langComboBox->currentData().toString();
  general.fullscreenMode = fullscreenCheckBox->isChecked();
  general.startInFolderView = startInFolderViewCheckBox->isChecked();
  general.standbyMode = standbyCheckBox->isChecked();
  general.rememberLastFolder = rememberLastFolderCheckBox->isChecked();
  general.windowTitleExtendedInfo = showExtendedInfoTitle->isChecked();
  general.infoBarFullscreen = showInfoBarFullscreen->isChecked();
  general.cursorAutohide = cursorAutohideCheckBox->isChecked();
  general.smoothScroll = enableSmoothScrollCheckBox->isChecked();
  general.smoothZoom = enableSmoothZoomCheckBox->isChecked();
  if (zoomIndicatorOn->isChecked())
    general.zoomIndicatorMode = INDICATOR_ENABLED;
  else if (zoomIndicatorAuto->isChecked())
    general.zoomIndicatorMode = INDICATOR_AUTO;
  else
    general.zoomIndicatorMode = INDICATOR_DISABLED;
  general.autoResizeWindow = autoResizeWindowCheckBox->isChecked();
  general.autoResizeLimitStep = autoResizeLimitSlider->value();
  general.panelEnabled = enablePanelCheckBox->isChecked();
  general.squareThumbnails = squareThumbnailsCheckBox->isChecked();
  general.panelPinned = pinPanelCheckBox->isChecked();
  general.panelFullscreenOnly = panelFullscreenOnlyCheckBox->isChecked();
  general.panelCenterSelection = panelCenterSelectionCheckBox->isChecked();
  general.showSubfoldersInPanel = showSubfoldersInPanelCheckBox->isChecked();
  general.panelHideDelayMs = panelHideDelaySlider->value();
  general.thumbPanelStyle =
      thumbStyleSimple->isChecked() ? TH_PANEL_SIMPLE : TH_PANEL_EXTENDED;
  general.panelSizeStep = panelSizeSlider->value();
  general.panelPosition = panelPositionComboBox->currentData().toInt();
  if (folderEndNoAction->isChecked())
    general.folderEndAction = FOLDER_END_NO_ACTION;
  else if (folderEndLoop->isChecked())
    general.folderEndAction = FOLDER_END_LOOP;
  else
    general.folderEndAction = FOLDER_END_GOTO_ADJACENT;
  general.sortingMode = sortingComboBox->currentData().toInt();
  general.sortFolders = sortFoldersCheckBox->isChecked();
  general.showHiddenFiles = showHiddenFilesCheckBox->isChecked();
  general.slideshowIntervalMs = slideshowIntervalSpinBox->value();
  general.loopSlideshow = loopSlideshowCheckBox->isChecked();

  ViewSettings &view = values.view;
  if (fitModeWindow->isChecked())
    view.imageFitMode = FIT_WINDOW;
  else if (fitModeWidth->isChecked())
    view.imageFitMode = FIT_WIDTH;
  else if (fitModeHeight->isChecked())
    view.imageFitMode = FIT_HEIGHT;
  else
    view.imageFitMode = FIT_ORIGINAL;
  view.keepFitMode = keepFitModeCheckBox->isChecked();
  if (focus1to1Top->isChecked())
    view.focusPoint = FOCUS_TOP;
  else if (focus1to1Center->isChecked())
    view.focusPoint = FOCUS_CENTER;
  else
    view.focusPoint = FOCUS_CURSOR;
  view.transparencyGrid = transparencyGridCheckBox->isChecked();
  view.expandImage = expandImageCheckBox->isChecked();
  view.expandLimit = expandLimitSlider->value();
  view.unlockMinZoom = unlockMinZoomCheckBox->isChecked();
  view.zoomStepPercent = zoomStepSlider->value();
  view.useFixedZoomLevels = useFixedZoomLevelsCheckBox->isChecked();
  view.zoomLevels = zoomLevels->text();
  view.scalingFilter = scalingQualityComboBox->currentData().toInt();
  view.casSharpeningPercent = casSharpeningSlider->value();
  view.casContrastPercent = casContrastSlider->value();
  view.colorManagementEnabled = colorManagementCheckBox->isChecked();
  view.monitorProfileType = monitorProfileComboBox->currentData().toString();
  view.monitorProfilePath = customProfilePathEdit->text();
  view.hdrToneMappingEnabled = hdrToneMappingCheckBox->isChecked();
  view.hdrOperator = hdrOperatorComboBox->currentData().toInt();
  view.hdrTargetWhiteLevel = hdrTargetWhiteComboBox->currentData().toInt();

  ThemeSettings &theme = values.theme;
  theme.themeMode = themeSelectorComboBox->currentData().toInt();
  theme.customAccent = useCustomAccentCheckBox->isChecked();
  theme.accentColor = colorSelectorAccent->color();
  theme.backgroundOpacityPercent = bgOpacitySlider->value();
  theme.thumbnailOpacityPercent = thumbOpacitySlider->value();
  theme.useBlackBackground = useBlackBackgroundCheckBox->isChecked();

  ControlsSettings &controls = values.controls;
  controls.clickableEdges = clickableEdgesCheckBox->isChecked();
  controls.clickableEdgesVisible = clickableEdgesVisibleCheckBox->isChecked();
  controls.imageScrolling = imageScrollingComboBox->currentData().toInt();
  controls.mouseScrollingSpeedStep = mouseScrollingSpeedSlider->value();
  controls.trackpadDetection = trackpadDetectionCheckBox->isChecked();

  AdvancedSettings &advanced = values.advanced;
  advanced.usePreloader = usePreloaderCheckBox->isChecked();
  advanced.thumbnailerThreads = thumbnailerThreadsSlider->value();
  advanced.useThumbnailCache = useThumbnailCacheCheckBox->isChecked();
  advanced.thumbnailResolution = thumbnailResolutionSlider->value();
  advanced.thumbnailCacheMaxSizeMB = thumbnailCacheQuotaSpinBox->value();
  advanced.excludedCachePaths = excludedCachePathsLineEdit->text();
  advanced.unloadThumbs = unloadThumbsCheckBox->isChecked();
  advanced.showSaveOverlay = saveOverlayCheckBox->isChecked();
  advanced.jpegQuality = JPEGQualitySlider->value();
  advanced.modernQuality = modernQualitySlider->value();
  advanced.pngCompression = pngQualitySlider->value();
  advanced.confirmTrash = confirmTrashCheckBox->isChecked();
  advanced.confirmDelete = confirmDeleteCheckBox->isChecked();
  advanced.multiInstance = multiInstanceCheckBox->isChecked();
  advanced.memoryLimitMB = memoryLimitSpinBox->value();

  UpscaleSettings &upscale = values.upscale;
  upscale.useUpscayl = useUpscaylCheckBox->isChecked();
  upscale.model = mEditor.isUpscaylAvailable() ? upscaylModelComboBox->currentText()
                                               : mEditor.upscale().model;
  upscale.preloadUpscayl = preloadUpscaylCheckBox->isChecked();
  upscale.limitEnabled = upscaylLimitCheckBox->isChecked();
  upscale.limitPercent = upscaylLimitSlider->value();
  return values;
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettings() {
  mEditor.setValues(collectSettings());
  mEditor.apply();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettingsAndClose() {
  saveSettings();
  this->close();
}
//------------------------------------------------------------------------------
void SettingsDialog::showAccentColor() {
  QColor accent = mEditor.theme().accentColor;
  const QSignalBlocker blocker(colorSelectorAccent);
  colorSelectorAccent->setColor(accent);
}
//------------------------------------------------------------------------------
int SettingsDialog::selectedShortcutRow() const {
  return shortcutsTableView->currentIndex().row();
}
//------------------------------------------------------------------------------
int SettingsDialog::selectedScriptRow() const {
  return scriptsListView->currentIndex().row();
}
//------------------------------------------------------------------------------
void SettingsDialog::runShortcutEditor() {
  ShortcutCreatorDialog dialog(*mEditor.shortcutEditor(), this);
  dialog.exec();
}
//------------------------------------------------------------------------------
void SettingsDialog::runScriptEditor() {
  ScriptEditorDialog dialog(*mEditor.scriptEditor(), this);
  dialog.exec();
}
//------------------------------------------------------------------------------
void SettingsDialog::addScript() {
  mEditor.addScript();
  runScriptEditor();
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript() {
  editScript(scriptsListView->currentIndex());
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(const QModelIndex &index) {
  if (!index.isValid())
    return;
  mEditor.editScript(index.row());
  runScriptEditor();
}
//------------------------------------------------------------------------------
void SettingsDialog::removeScript() {
  const int row = selectedScriptRow();
  if (row >= 0)
    mEditor.removeScript(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::addShortcut() {
  mEditor.addShortcut();
  runShortcutEditor();
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut(int row) {
  if (row < 0)
    return;
  mEditor.editShortcut(row);
  runShortcutEditor();
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut() {
  editShortcut(selectedShortcutRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcut() {
  const int row = selectedShortcutRow();
  if (row >= 0)
    mEditor.removeShortcut(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::resetShortcuts() {
  mEditor.resetShortcuts();
}
//------------------------------------------------------------------------------
void SettingsDialog::resetZoomLevels() {
  mEditor.resetZoomLevels();
  zoomLevels->setText(mEditor.view().zoomLevels);
}
//------------------------------------------------------------------------------
void SettingsDialog::onClearThumbnailCacheClicked() {
  // The live cache is owned by Core; the store requests the clear, which
  // Core performs synchronously (direct connection).
  mEditor.clearThumbnailCache();
  thumbnailCacheSizeValueLabel->setText(mEditor.thumbnailCacheSizeText());
}
//------------------------------------------------------------------------------
void SettingsDialog::onExpandLimitSliderChanged(int value) {
  expandLimitLabel->setText(SettingsScales::expandLimitText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onJPEGQualitySliderChanged(int value) {
  JPEGQualityLabel->setText(SettingsScales::percentText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onPNGQualitySliderChanged(int value) {
  pngQualityLabel->setText(SettingsScales::pngCompressionText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onModernQualitySliderChanged(int value) {
  modernQualityLabel->setText(SettingsScales::percentText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onZoomStepSliderChanged(int value) {
  zoomStepLabel->setText(SettingsScales::zoomStepText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onMouseScrollingSpeedSliderChanged(int value) {
  mouseScrollingSpeedLabel->setText(SettingsScales::mouseScrollingSpeedText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailResolutionSliderChanged(int value) {
  const int snapped =
      SettingsScales::snapped(value, thumbnailResolutionSlider->singleStep());
  if (snapped != value) {
    thumbnailResolutionSlider->setValue(snapped);
    return;
  }
  thumbnailResolutionValueLabel->setText(
      SettingsScales::thumbnailResolutionText(snapped));
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailerThreadsSliderChanged(int value) {
  thumbnailerThreadsLabel->setText(QString::number(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onBgOpacitySliderChanged(int value) {
  bgOpacityPercentLabel->setText(SettingsScales::percentText(value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbOpacitySliderChanged(int value) {
  thumbOpacityPercentLabel->setText(SettingsScales::percentText(value));
  mEditor.setThumbnailOpacityPercent(value, !thumbOpacitySlider->isSliderDown());
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbOpacitySliderReleased() {
  mEditor.setThumbnailOpacityPercent(thumbOpacitySlider->value(), true);
}
//------------------------------------------------------------------------------
void SettingsDialog::onAutoResizeLimitSliderChanged(int value) {
  autoResizeLimit->setText(SettingsScales::autoResizeLimitText(value));
}
//------------------------------------------------------------------------------
int SettingsDialog::exec() {
  adjustSizeToContents();
  resize(sizeHint());
  return QDialog::exec();
}

void SettingsDialog::switchToPage(int number) {
  sideBar2->selectEntry(number);
}

void SettingsDialog::setupUi() {
        if (this->objectName().isEmpty())
            this->setObjectName("SettingsDialog");
        this->setWindowModality(Qt::WindowModality::ApplicationModal);
        this->setEnabled(true);
        this->resize(783, 741);
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(this->sizePolicy().hasHeightForWidth());
        this->setSizePolicy(sizePolicy);
        this->setMinimumSize(QSize(700, 620));
        this->setMaximumSize(QSize(1920, 1080));
        this->setBaseSize(QSize(322, 221));
        this->setFocusPolicy(Qt::FocusPolicy::TabFocus);
        this->setWindowOpacity(1.000000000000000);
        verticalLayout_3 = new QVBoxLayout(this);
        verticalLayout_3->setSpacing(0);
        verticalLayout_3->setObjectName("verticalLayout_3");
        verticalLayout_3->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_22 = new QHBoxLayout();
        horizontalLayout_22->setSpacing(0);
        horizontalLayout_22->setObjectName("horizontalLayout_22");
        horizontalLayout_22->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        horizontalLayout_22->setContentsMargins(-1, 0, 0, -1);
        widget_17 = new QWidget(this);
        widget_17->setObjectName("widget_17");
        widget_17->setMinimumSize(QSize(0, 0));
#if QT_CONFIG(accessibility)
        widget_17->setAccessibleName(QString::fromUtf8("SSideBarContainer"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_37 = new QVBoxLayout(widget_17);
        verticalLayout_37->setObjectName("verticalLayout_37");
        verticalLayout_37->setContentsMargins(0, 0, 0, 0);
        sideBar2 = new SSideBar(widget_17);
        sideBar2->setObjectName("sideBar2");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(sideBar2->sizePolicy().hasHeightForWidth());
        sideBar2->setSizePolicy(sizePolicy1);
        sideBar2->setMinimumSize(QSize(170, 0));

        verticalLayout_37->addWidget(sideBar2);

        versionInfoWidget = new QWidget(widget_17);
        versionInfoWidget->setObjectName("versionInfoWidget");
        horizontalLayout_41 = new QHBoxLayout(versionInfoWidget);
        horizontalLayout_41->setSpacing(4);
        horizontalLayout_41->setObjectName("horizontalLayout_41");
        horizontalLayout_41->setContentsMargins(8, 8, 8, 8);
        appIconLabel = new QLabel(versionInfoWidget);
        appIconLabel->setObjectName("appIconLabel");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(appIconLabel->sizePolicy().hasHeightForWidth());
        appIconLabel->setSizePolicy(sizePolicy2);

        horizontalLayout_41->addWidget(appIconLabel);

        versionLabel = new QLabel(versionInfoWidget);
        versionLabel->setObjectName("versionLabel");
        QFont font;
        font.setPointSize(10);
        versionLabel->setFont(font);

        horizontalLayout_41->addWidget(versionLabel);

        horizontalSpacer_10 = new QSpacerItem(7, 10, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        horizontalLayout_41->addItem(horizontalSpacer_10);

        qtIconLabel = new QLabel(versionInfoWidget);
        qtIconLabel->setObjectName("qtIconLabel");
        sizePolicy2.setHeightForWidth(qtIconLabel->sizePolicy().hasHeightForWidth());
        qtIconLabel->setSizePolicy(sizePolicy2);

        horizontalLayout_41->addWidget(qtIconLabel);

        qtVersionLabel = new QLabel(versionInfoWidget);
        qtVersionLabel->setObjectName("qtVersionLabel");
        qtVersionLabel->setFont(font);

        horizontalLayout_41->addWidget(qtVersionLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_41->addItem(horizontalSpacer);


        verticalLayout_37->addWidget(versionInfoWidget);


        horizontalLayout_22->addWidget(widget_17);

        verticalLayout_6 = new QVBoxLayout();
        verticalLayout_6->setObjectName("verticalLayout_6");
        verticalLayout_6->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        verticalLayout_6->setContentsMargins(0, 0, 0, 0);
        stackedWidget = new QStackedWidget(this);
        stackedWidget->setObjectName("stackedWidget");
        sizePolicy.setHeightForWidth(stackedWidget->sizePolicy().hasHeightForWidth());
        stackedWidget->setSizePolicy(sizePolicy);
        stackedWidget->setMinimumSize(QSize(480, 0));
        stackedWidget->setFrameShape(QFrame::Shape::NoFrame);
        General = new QWidget();
        General->setObjectName("General");
        verticalLayout_4 = new QVBoxLayout(General);
        verticalLayout_4->setObjectName("verticalLayout_4");
        verticalLayout_4->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        scrollArea = new QScrollArea(General);
        scrollArea->setObjectName("scrollArea");
        sizePolicy.setHeightForWidth(scrollArea->sizePolicy().hasHeightForWidth());
        scrollArea->setSizePolicy(sizePolicy);
        scrollArea->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        scrollArea->setSizeAdjustPolicy(QAbstractScrollArea::SizeAdjustPolicy::AdjustToContents);
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName("scrollAreaWidgetContents");
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 592, 1094));
        verticalLayout_5 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_5->setSpacing(9);
        verticalLayout_5->setObjectName("verticalLayout_5");
        verticalLayout_5->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        verticalLayout_5->setContentsMargins(18, 9, 18, 9);
        label_20 = new QLabel(scrollAreaWidgetContents);
        label_20->setObjectName("label_20");
#if QT_CONFIG(accessibility)
        label_20->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_5->addWidget(label_20);

        widget_18 = new QWidget(scrollAreaWidgetContents);
        widget_18->setObjectName("widget_18");
#if QT_CONFIG(accessibility)
        widget_18->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_5->addWidget(widget_18);

        verticalSpacer = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_5->addItem(verticalSpacer);

        generalGroup = new QWidget(scrollAreaWidgetContents);
        generalGroup->setObjectName("generalGroup");
#if QT_CONFIG(accessibility)
        generalGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_12 = new QVBoxLayout(generalGroup);
        verticalLayout_12->setObjectName("verticalLayout_12");
        verticalLayout_12->setContentsMargins(0, 0, 0, 0);
        verticalLayout_20 = new QVBoxLayout();
        verticalLayout_20->setSpacing(7);
        verticalLayout_20->setObjectName("verticalLayout_20");
        verticalLayout_20->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_18 = new QHBoxLayout();
        horizontalLayout_18->setObjectName("horizontalLayout_18");
        horizontalLayout_18->setContentsMargins(0, 0, 0, -1);
        label_46 = new QLabel(generalGroup);
        label_46->setObjectName("label_46");

        horizontalLayout_18->addWidget(label_46);

        langComboBox = new QComboBox(generalGroup);
        langComboBox->setObjectName("langComboBox");
        QSizePolicy sizePolicy3(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(langComboBox->sizePolicy().hasHeightForWidth());
        langComboBox->setSizePolicy(sizePolicy3);

        horizontalLayout_18->addWidget(langComboBox);

        label_48 = new QLabel(generalGroup);
        label_48->setObjectName("label_48");
#if QT_CONFIG(accessibility)
        label_48->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)
        label_48->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_18->addWidget(label_48);

        horizontalSpacer_12 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_18->addItem(horizontalSpacer_12);


        verticalLayout_20->addLayout(horizontalLayout_18);

        widget_20 = new QWidget(generalGroup);
        widget_20->setObjectName("widget_20");
#if QT_CONFIG(accessibility)
        widget_20->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_20->addWidget(widget_20);

        fullscreenCheckBox = new QCheckBox(generalGroup);
        fullscreenCheckBox->setObjectName("fullscreenCheckBox");
        fullscreenCheckBox->setMinimumSize(QSize(250, 0));

        verticalLayout_20->addWidget(fullscreenCheckBox);

        startInFolderViewCheckBox = new QCheckBox(generalGroup);
        startInFolderViewCheckBox->setObjectName("startInFolderViewCheckBox");

        verticalLayout_20->addWidget(startInFolderViewCheckBox);

        standbyCheckBox = new QCheckBox(generalGroup);
        standbyCheckBox->setObjectName("standbyCheckBox");

        verticalLayout_20->addWidget(standbyCheckBox);

        rememberLastFolderCheckBox = new QCheckBox(generalGroup);
        rememberLastFolderCheckBox->setObjectName("rememberLastFolderCheckBox");

        verticalLayout_20->addWidget(rememberLastFolderCheckBox);


        verticalLayout_12->addLayout(verticalLayout_20);


        verticalLayout_5->addWidget(generalGroup);

        verticalSpacer_19 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_5->addItem(verticalSpacer_19);

        UIOptionsGroup = new QWidget(scrollAreaWidgetContents);
        UIOptionsGroup->setObjectName("UIOptionsGroup");
#if QT_CONFIG(accessibility)
        UIOptionsGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_11 = new QVBoxLayout(UIOptionsGroup);
        verticalLayout_11->setSpacing(7);
        verticalLayout_11->setObjectName("verticalLayout_11");
        verticalLayout_11->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        verticalLayout_11->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_16 = new QHBoxLayout();
        horizontalLayout_16->setObjectName("horizontalLayout_16");
        horizontalLayout_16->setContentsMargins(0, 0, 0, 0);
        label_15 = new QLabel(UIOptionsGroup);
        label_15->setObjectName("label_15");
        QFont font1;
        font1.setBold(true);
        label_15->setFont(font1);
        label_15->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_16->addWidget(label_15);


        verticalLayout_11->addLayout(horizontalLayout_16);

        verticalLayout_7 = new QVBoxLayout();
        verticalLayout_7->setSpacing(7);
        verticalLayout_7->setObjectName("verticalLayout_7");
        verticalLayout_7->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        verticalLayout_7->setContentsMargins(0, 0, 0, 0);
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        gridLayout->setSizeConstraint(QLayout::SizeConstraint::SetDefaultConstraint);
        gridLayout->setHorizontalSpacing(6);
        gridLayout->setVerticalSpacing(7);
        gridLayout->setContentsMargins(0, 0, 0, 0);
        showExtendedInfoTitle = new QCheckBox(UIOptionsGroup);
        showExtendedInfoTitle->setObjectName("showExtendedInfoTitle");

        gridLayout->addWidget(showExtendedInfoTitle, 1, 0, 1, 1);

        showInfoBarFullscreen = new QCheckBox(UIOptionsGroup);
        showInfoBarFullscreen->setObjectName("showInfoBarFullscreen");
        showInfoBarFullscreen->setMinimumSize(QSize(250, 0));

        gridLayout->addWidget(showInfoBarFullscreen, 0, 0, 1, 1);

        cursorAutohideCheckBox = new QCheckBox(UIOptionsGroup);
        cursorAutohideCheckBox->setObjectName("cursorAutohideCheckBox");

        gridLayout->addWidget(cursorAutohideCheckBox, 2, 0, 1, 1);

        enableSmoothScrollCheckBox = new QCheckBox(UIOptionsGroup);
        enableSmoothScrollCheckBox->setObjectName("enableSmoothScrollCheckBox");

        gridLayout->addWidget(enableSmoothScrollCheckBox, 0, 1, 1, 1);

        enableSmoothZoomCheckBox = new QCheckBox(UIOptionsGroup);
        enableSmoothZoomCheckBox->setObjectName("enableSmoothZoomCheckBox");

        gridLayout->addWidget(enableSmoothZoomCheckBox, 1, 1, 1, 1);


        verticalLayout_7->addLayout(gridLayout);

        widget_9 = new QWidget(UIOptionsGroup);
        widget_9->setObjectName("widget_9");
#if QT_CONFIG(accessibility)
        widget_9->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_7->addWidget(widget_9);

        horizontalLayout_23 = new QHBoxLayout();
        horizontalLayout_23->setObjectName("horizontalLayout_23");
        horizontalLayout_23->setContentsMargins(0, 0, 0, -1);
        label = new QLabel(UIOptionsGroup);
        label->setObjectName("label");
        QSizePolicy sizePolicy4(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Preferred);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy4);
        label->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_23->addWidget(label);

        zoomIndicatorOn = new QRadioButton(UIOptionsGroup);
        zoomIndicatorOn->setObjectName("zoomIndicatorOn");

        horizontalLayout_23->addWidget(zoomIndicatorOn);

        zoomIndicatorOff = new QRadioButton(UIOptionsGroup);
        zoomIndicatorOff->setObjectName("zoomIndicatorOff");

        horizontalLayout_23->addWidget(zoomIndicatorOff);

        zoomIndicatorAuto = new QRadioButton(UIOptionsGroup);
        zoomIndicatorAuto->setObjectName("zoomIndicatorAuto");

        horizontalLayout_23->addWidget(zoomIndicatorAuto);

        horizontalSpacer_6 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_23->addItem(horizontalSpacer_6);


        verticalLayout_7->addLayout(horizontalLayout_23);

        widget_19 = new QWidget(UIOptionsGroup);
        widget_19->setObjectName("widget_19");
#if QT_CONFIG(accessibility)
        widget_19->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_7->addWidget(widget_19);

        horizontalLayout_28 = new QHBoxLayout();
        horizontalLayout_28->setObjectName("horizontalLayout_28");
        horizontalLayout_28->setContentsMargins(0, 0, 0, -1);
        autoResizeWindowCheckBox = new QCheckBox(UIOptionsGroup);
        autoResizeWindowCheckBox->setObjectName("autoResizeWindowCheckBox");

        horizontalLayout_28->addWidget(autoResizeWindowCheckBox);

        label_50 = new QLabel(UIOptionsGroup);
        label_50->setObjectName("label_50");
#if QT_CONFIG(accessibility)
        label_50->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        horizontalLayout_28->addWidget(label_50);

        horizontalSpacer_19 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_28->addItem(horizontalSpacer_19);


        verticalLayout_7->addLayout(horizontalLayout_28);

        horizontalLayout_19 = new QHBoxLayout();
        horizontalLayout_19->setObjectName("horizontalLayout_19");
        horizontalLayout_19->setContentsMargins(0, 0, 0, 0);
        label_39 = new QLabel(UIOptionsGroup);
        label_39->setObjectName("label_39");

        horizontalLayout_19->addWidget(label_39);

        autoResizeLimitSlider = new QSlider(UIOptionsGroup);
        autoResizeLimitSlider->setObjectName("autoResizeLimitSlider");
        QSizePolicy sizePolicy5(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Preferred);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(autoResizeLimitSlider->sizePolicy().hasHeightForWidth());
        autoResizeLimitSlider->setSizePolicy(sizePolicy5);
        autoResizeLimitSlider->setMinimumSize(QSize(200, 0));
        autoResizeLimitSlider->setMinimum(6);
        autoResizeLimitSlider->setMaximum(20);
        autoResizeLimitSlider->setSingleStep(1);
        autoResizeLimitSlider->setPageStep(2);
        autoResizeLimitSlider->setValue(18);
        autoResizeLimitSlider->setOrientation(Qt::Orientation::Horizontal);
        autoResizeLimitSlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        autoResizeLimitSlider->setTickInterval(1);

        horizontalLayout_19->addWidget(autoResizeLimitSlider);

        autoResizeLimit = new QLabel(UIOptionsGroup);
        autoResizeLimit->setObjectName("autoResizeLimit");
        sizePolicy5.setHeightForWidth(autoResizeLimit->sizePolicy().hasHeightForWidth());
        autoResizeLimit->setSizePolicy(sizePolicy5);
        autoResizeLimit->setMinimumSize(QSize(60, 0));
        autoResizeLimit->setMaximumSize(QSize(60, 16777215));
        autoResizeLimit->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_19->addWidget(autoResizeLimit);

        horizontalSpacer_17 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_19->addItem(horizontalSpacer_17);


        verticalLayout_7->addLayout(horizontalLayout_19);


        verticalLayout_11->addLayout(verticalLayout_7);


        verticalLayout_5->addWidget(UIOptionsGroup);

        verticalSpacer_2 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_5->addItem(verticalSpacer_2);

        thumbnailPanelGroup = new QWidget(scrollAreaWidgetContents);
        thumbnailPanelGroup->setObjectName("thumbnailPanelGroup");
#if QT_CONFIG(accessibility)
        thumbnailPanelGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_14 = new QVBoxLayout(thumbnailPanelGroup);
        verticalLayout_14->setSpacing(7);
        verticalLayout_14->setObjectName("verticalLayout_14");
        verticalLayout_14->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        horizontalLayout_6->setContentsMargins(0, 0, 0, -1);
        enablePanelCheckBox = new QCheckBox(thumbnailPanelGroup);
        enablePanelCheckBox->setObjectName("enablePanelCheckBox");
        sizePolicy3.setHeightForWidth(enablePanelCheckBox->sizePolicy().hasHeightForWidth());
        enablePanelCheckBox->setSizePolicy(sizePolicy3);
        enablePanelCheckBox->setMinimumSize(QSize(0, 0));
        enablePanelCheckBox->setFont(font1);

        horizontalLayout_6->addWidget(enablePanelCheckBox);


        verticalLayout_14->addLayout(horizontalLayout_6);

        thumbnailPanelGroupContents = new QWidget(thumbnailPanelGroup);
        thumbnailPanelGroupContents->setObjectName("thumbnailPanelGroupContents");
        verticalLayout_15 = new QVBoxLayout(thumbnailPanelGroupContents);
        verticalLayout_15->setSpacing(7);
        verticalLayout_15->setObjectName("verticalLayout_15");
        verticalLayout_15->setContentsMargins(0, 0, 0, 0);
        gridLayout_3 = new QGridLayout();
        gridLayout_3->setObjectName("gridLayout_3");
        gridLayout_3->setVerticalSpacing(7);
        gridLayout_3->setContentsMargins(0, 0, 0, 0);
        squareThumbnailsCheckBox = new QCheckBox(thumbnailPanelGroupContents);
        squareThumbnailsCheckBox->setObjectName("squareThumbnailsCheckBox");
        sizePolicy3.setHeightForWidth(squareThumbnailsCheckBox->sizePolicy().hasHeightForWidth());
        squareThumbnailsCheckBox->setSizePolicy(sizePolicy3);
        squareThumbnailsCheckBox->setMinimumSize(QSize(0, 0));

        gridLayout_3->addWidget(squareThumbnailsCheckBox, 0, 1, 1, 1);

        pinPanelCheckBox = new QCheckBox(thumbnailPanelGroupContents);
        pinPanelCheckBox->setObjectName("pinPanelCheckBox");
        pinPanelCheckBox->setMinimumSize(QSize(250, 0));

        gridLayout_3->addWidget(pinPanelCheckBox, 0, 0, 1, 1);

        panelFullscreenOnlyCheckBox = new QCheckBox(thumbnailPanelGroupContents);
        panelFullscreenOnlyCheckBox->setObjectName("panelFullscreenOnlyCheckBox");
        sizePolicy3.setHeightForWidth(panelFullscreenOnlyCheckBox->sizePolicy().hasHeightForWidth());
        panelFullscreenOnlyCheckBox->setSizePolicy(sizePolicy3);
        panelFullscreenOnlyCheckBox->setMinimumSize(QSize(0, 0));

        gridLayout_3->addWidget(panelFullscreenOnlyCheckBox, 1, 0, 1, 1);

        panelCenterSelectionCheckBox = new QCheckBox(thumbnailPanelGroupContents);
        panelCenterSelectionCheckBox->setObjectName("panelCenterSelectionCheckBox");

        gridLayout_3->addWidget(panelCenterSelectionCheckBox, 1, 1, 1, 1);

        showSubfoldersInPanelCheckBox = new QCheckBox(thumbnailPanelGroupContents);
        showSubfoldersInPanelCheckBox->setObjectName("showSubfoldersInPanelCheckBox");

        gridLayout_3->addWidget(showSubfoldersInPanelCheckBox, 2, 0, 1, 1);

        horizontalSpacer_8 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_3->addItem(horizontalSpacer_8, 0, 2, 1, 1);


        verticalLayout_15->addLayout(gridLayout_3);

        widget_6 = new QWidget(thumbnailPanelGroupContents);
        widget_6->setObjectName("widget_6");
        QSizePolicy sizePolicy6(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
        sizePolicy6.setHorizontalStretch(0);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(widget_6->sizePolicy().hasHeightForWidth());
        widget_6->setSizePolicy(sizePolicy6);
#if QT_CONFIG(accessibility)
        widget_6->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_15->addWidget(widget_6);

        gridLayout_4 = new QGridLayout();
        gridLayout_4->setObjectName("gridLayout_4");
        gridLayout_4->setVerticalSpacing(7);
        gridLayout_4->setContentsMargins(0, 0, 0, 0);
        thumbStyleExtended = new QRadioButton(thumbnailPanelGroupContents);
        thumbStyleExtended->setObjectName("thumbStyleExtended");

        gridLayout_4->addWidget(thumbStyleExtended, 1, 1, 1, 1);

        label_8 = new QLabel(thumbnailPanelGroupContents);
        label_8->setObjectName("label_8");
#if QT_CONFIG(accessibility)
        label_8->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        gridLayout_4->addWidget(label_8, 0, 2, 1, 1);

        label_18 = new QLabel(thumbnailPanelGroupContents);
        label_18->setObjectName("label_18");
        label_18->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        gridLayout_4->addWidget(label_18, 0, 0, 1, 1);

        label_25 = new QLabel(thumbnailPanelGroupContents);
        label_25->setObjectName("label_25");
#if QT_CONFIG(accessibility)
        label_25->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        gridLayout_4->addWidget(label_25, 1, 2, 1, 1);

        horizontalSpacer_26 = new QSpacerItem(40, 10, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_4->addItem(horizontalSpacer_26, 0, 3, 1, 1);

        thumbStyleSimple = new QRadioButton(thumbnailPanelGroupContents);
        thumbStyleSimple->setObjectName("thumbStyleSimple");

        gridLayout_4->addWidget(thumbStyleSimple, 0, 1, 1, 1);


        verticalLayout_15->addLayout(gridLayout_4);

        panelHideDelayLayout = new QHBoxLayout();
        panelHideDelayLayout->setObjectName("panelHideDelayLayout");
        panelHideDelayLayout->setContentsMargins(0, 0, 0, 0);
        panelHideDelayLabel = new QLabel(thumbnailPanelGroupContents);
        panelHideDelayLabel->setObjectName("panelHideDelayLabel");

        panelHideDelayLayout->addWidget(panelHideDelayLabel);

        panelHideDelaySlider = new QSlider(thumbnailPanelGroupContents);
        panelHideDelaySlider->setObjectName("panelHideDelaySlider");
        sizePolicy5.setHeightForWidth(panelHideDelaySlider->sizePolicy().hasHeightForWidth());
        panelHideDelaySlider->setSizePolicy(sizePolicy5);
        panelHideDelaySlider->setMinimumSize(QSize(kPanelSliderMinimumWidth, 0));
        panelHideDelaySlider->setRange(Settings::MinPanelHideDelayMs,
                                       Settings::MaxPanelHideDelayMs);
        panelHideDelaySlider->setSingleStep(Settings::PanelHideDelayStepMs);
        panelHideDelaySlider->setPageStep(Settings::PanelHideDelayStepMs);
        panelHideDelaySlider->setOrientation(Qt::Orientation::Horizontal);
        panelHideDelaySlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        panelHideDelaySlider->setTickInterval(Settings::PanelHideDelayStepMs);

        panelHideDelayLayout->addWidget(panelHideDelaySlider);

        panelHideDelayValueLabel = new QLabel(thumbnailPanelGroupContents);
        panelHideDelayValueLabel->setObjectName("panelHideDelayValueLabel");

        panelHideDelayLayout->addWidget(panelHideDelayValueLabel);

        panelHideDelayLayout->addStretch();

        verticalLayout_15->addLayout(panelHideDelayLayout);

        widget_2 = new QWidget(thumbnailPanelGroupContents);
        widget_2->setObjectName("widget_2");
#if QT_CONFIG(accessibility)
        widget_2->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_15->addWidget(widget_2);

        horizontalLayout_24 = new QHBoxLayout();
        horizontalLayout_24->setObjectName("horizontalLayout_24");
        horizontalLayout_24->setContentsMargins(0, 0, -1, -1);
        label_4 = new QLabel(thumbnailPanelGroupContents);
        label_4->setObjectName("label_4");

        horizontalLayout_24->addWidget(label_4);

        panelSizeSlider = new QSlider(thumbnailPanelGroupContents);
        panelSizeSlider->setObjectName("panelSizeSlider");
        sizePolicy5.setHeightForWidth(panelSizeSlider->sizePolicy().hasHeightForWidth());
        panelSizeSlider->setSizePolicy(sizePolicy5);
        panelSizeSlider->setMinimumSize(QSize(kPanelSliderMinimumWidth, 0));
        panelSizeSlider->setMinimum(10);
        panelSizeSlider->setMaximum(26);
        panelSizeSlider->setSingleStep(1);
        panelSizeSlider->setPageStep(5);
        panelSizeSlider->setValue(14);
        panelSizeSlider->setOrientation(Qt::Orientation::Horizontal);
        panelSizeSlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        panelSizeSlider->setTickInterval(1);

        horizontalLayout_24->addWidget(panelSizeSlider);

        horizontalSpacer_11 = new QSpacerItem(10, 10, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        horizontalLayout_24->addItem(horizontalSpacer_11);

        panelPositionLabel = new QLabel(thumbnailPanelGroupContents);
        panelPositionLabel->setObjectName("panelPositionLabel");
        sizePolicy3.setHeightForWidth(panelPositionLabel->sizePolicy().hasHeightForWidth());
        panelPositionLabel->setSizePolicy(sizePolicy3);
        panelPositionLabel->setMinimumSize(QSize(0, 0));

        horizontalLayout_24->addWidget(panelPositionLabel);

        panelPositionComboBox = new QComboBox(thumbnailPanelGroupContents);
        panelPositionComboBox->addItem(QString());
        panelPositionComboBox->addItem(QString());
        panelPositionComboBox->addItem(QString());
        panelPositionComboBox->addItem(QString());
        panelPositionComboBox->setObjectName("panelPositionComboBox");

        horizontalLayout_24->addWidget(panelPositionComboBox);

        horizontalSpacer_24 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_24->addItem(horizontalSpacer_24);


        verticalLayout_15->addLayout(horizontalLayout_24);


        verticalLayout_14->addWidget(thumbnailPanelGroupContents);


        verticalLayout_5->addWidget(thumbnailPanelGroup);

        verticalSpacer_15 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_5->addItem(verticalSpacer_15);

        folderNavGroup = new QWidget(scrollAreaWidgetContents);
        folderNavGroup->setObjectName("folderNavGroup");
#if QT_CONFIG(accessibility)
        folderNavGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_16 = new QVBoxLayout(folderNavGroup);
        verticalLayout_16->setObjectName("verticalLayout_16");
        verticalLayout_16->setContentsMargins(0, 0, 0, 0);
        verticalLayout_17 = new QVBoxLayout();
        verticalLayout_17->setSpacing(7);
        verticalLayout_17->setObjectName("verticalLayout_17");
        verticalLayout_17->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_32 = new QHBoxLayout();
        horizontalLayout_32->setObjectName("horizontalLayout_32");
        horizontalLayout_32->setContentsMargins(0, 0, -1, -1);
        label_30 = new QLabel(folderNavGroup);
        label_30->setObjectName("label_30");
        label_30->setFont(font1);

        horizontalLayout_32->addWidget(label_30);


        verticalLayout_17->addLayout(horizontalLayout_32);

        gridLayout_5 = new QGridLayout();
        gridLayout_5->setObjectName("gridLayout_5");
        gridLayout_5->setVerticalSpacing(7);
        gridLayout_5->setContentsMargins(0, 0, 0, 0);
        folderEndNoAction = new QRadioButton(folderNavGroup);
        folderEndNoAction->setObjectName("folderEndNoAction");
        folderEndNoAction->setChecked(false);

        gridLayout_5->addWidget(folderEndNoAction, 0, 1, 1, 1);

        folderEndLoop = new QRadioButton(folderNavGroup);
        folderEndLoop->setObjectName("folderEndLoop");

        gridLayout_5->addWidget(folderEndLoop, 1, 1, 1, 1);

        folderEndSwitchFolder = new QRadioButton(folderNavGroup);
        folderEndSwitchFolder->setObjectName("folderEndSwitchFolder");

        gridLayout_5->addWidget(folderEndSwitchFolder, 2, 1, 1, 1);

        label_16 = new QLabel(folderNavGroup);
        label_16->setObjectName("label_16");
        label_16->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        gridLayout_5->addWidget(label_16, 0, 0, 1, 1);

        horizontalSpacer_25 = new QSpacerItem(40, 10, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_5->addItem(horizontalSpacer_25, 0, 2, 1, 1);


        verticalLayout_17->addLayout(gridLayout_5);

        widget_7 = new QWidget(folderNavGroup);
        widget_7->setObjectName("widget_7");
        sizePolicy6.setHeightForWidth(widget_7->sizePolicy().hasHeightForWidth());
        widget_7->setSizePolicy(sizePolicy6);
#if QT_CONFIG(accessibility)
        widget_7->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_17->addWidget(widget_7);

        horizontalLayout_33 = new QHBoxLayout();
        horizontalLayout_33->setObjectName("horizontalLayout_33");
        horizontalLayout_33->setContentsMargins(0, 0, 0, 0);
        label_6 = new QLabel(folderNavGroup);
        label_6->setObjectName("label_6");
        label_6->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_33->addWidget(label_6);

        sortingComboBox = new QComboBox(folderNavGroup);
        sortingComboBox->addItem(QString());
        sortingComboBox->addItem(QString());
        sortingComboBox->addItem(QString());
        sortingComboBox->addItem(QString());
        sortingComboBox->addItem(QString());
        sortingComboBox->addItem(QString());
        sortingComboBox->setObjectName("sortingComboBox");
        sizePolicy3.setHeightForWidth(sortingComboBox->sizePolicy().hasHeightForWidth());
        sortingComboBox->setSizePolicy(sizePolicy3);

        horizontalLayout_33->addWidget(sortingComboBox);

        horizontalSpacer_15 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_33->addItem(horizontalSpacer_15);


        verticalLayout_17->addLayout(horizontalLayout_33);

        sortFoldersCheckBox = new QCheckBox(folderNavGroup);
        sortFoldersCheckBox->setObjectName("sortFoldersCheckBox");

        verticalLayout_17->addWidget(sortFoldersCheckBox);

        widget_31 = new QWidget(folderNavGroup);
        widget_31->setObjectName("widget_31");
#if QT_CONFIG(accessibility)
        widget_31->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_17->addWidget(widget_31);

        showHiddenFilesCheckBox = new QCheckBox(folderNavGroup);
        showHiddenFilesCheckBox->setObjectName("showHiddenFilesCheckBox");

        verticalLayout_17->addWidget(showHiddenFilesCheckBox);


        verticalLayout_16->addLayout(verticalLayout_17);


        verticalLayout_5->addWidget(folderNavGroup);

        verticalSpacer_5 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_5->addItem(verticalSpacer_5);

        slideshowGroup = new QWidget(scrollAreaWidgetContents);
        slideshowGroup->setObjectName("slideshowGroup");
#if QT_CONFIG(accessibility)
        slideshowGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_18 = new QVBoxLayout(slideshowGroup);
        verticalLayout_18->setSpacing(7);
        verticalLayout_18->setObjectName("verticalLayout_18");
        verticalLayout_18->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_34 = new QHBoxLayout();
        horizontalLayout_34->setObjectName("horizontalLayout_34");
        horizontalLayout_34->setContentsMargins(0, 0, 0, 0);
        label_42 = new QLabel(slideshowGroup);
        label_42->setObjectName("label_42");
        label_42->setFont(font1);

        horizontalLayout_34->addWidget(label_42);


        verticalLayout_18->addLayout(horizontalLayout_34);

        slideshowGroupContents = new QHBoxLayout();
        slideshowGroupContents->setSpacing(7);
        slideshowGroupContents->setObjectName("slideshowGroupContents");
        slideshowGroupContents->setContentsMargins(0, 0, -1, -1);
        label_27 = new QLabel(slideshowGroup);
        label_27->setObjectName("label_27");
        label_27->setAlignment(Qt::AlignmentFlag::AlignRight|Qt::AlignmentFlag::AlignTrailing|Qt::AlignmentFlag::AlignVCenter);

        slideshowGroupContents->addWidget(label_27);

        slideshowIntervalSpinBox = new QSpinBox(slideshowGroup);
        slideshowIntervalSpinBox->setObjectName("slideshowIntervalSpinBox");
        sizePolicy4.setHeightForWidth(slideshowIntervalSpinBox->sizePolicy().hasHeightForWidth());
        slideshowIntervalSpinBox->setSizePolicy(sizePolicy4);
        slideshowIntervalSpinBox->setMinimumSize(QSize(120, 24));
        slideshowIntervalSpinBox->setFrame(true);
        slideshowIntervalSpinBox->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);
        slideshowIntervalSpinBox->setButtonSymbols(QAbstractSpinBox::ButtonSymbols::NoButtons);
        slideshowIntervalSpinBox->setMinimum(500);
        slideshowIntervalSpinBox->setMaximum(65535);
        slideshowIntervalSpinBox->setValue(2000);

        slideshowGroupContents->addWidget(slideshowIntervalSpinBox);

        horizontalSpacer_27 = new QSpacerItem(10, 10, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        slideshowGroupContents->addItem(horizontalSpacer_27);

        loopSlideshowCheckBox = new QCheckBox(slideshowGroup);
        loopSlideshowCheckBox->setObjectName("loopSlideshowCheckBox");

        slideshowGroupContents->addWidget(loopSlideshowCheckBox);

        horizontalSpacer_16 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        slideshowGroupContents->addItem(horizontalSpacer_16);


        verticalLayout_18->addLayout(slideshowGroupContents);


        verticalLayout_5->addWidget(slideshowGroup);

        verticalSpacer_3 = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::MinimumExpanding);

        verticalLayout_5->addItem(verticalSpacer_3);

        scrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_4->addWidget(scrollArea);

        stackedWidget->addWidget(General);
        View = new QWidget();
        View->setObjectName("View");
        verticalLayout_19 = new QVBoxLayout(View);
        verticalLayout_19->setObjectName("verticalLayout_19");
        verticalLayout_19->setContentsMargins(0, 0, 0, 0);
        scrollArea_3 = new QScrollArea(View);
        scrollArea_3->setObjectName("scrollArea_3");
        scrollArea_3->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea_3->setWidgetResizable(true);
        scrollAreaWidgetContents_3 = new QWidget();
        scrollAreaWidgetContents_3->setObjectName("scrollAreaWidgetContents_3");
        scrollAreaWidgetContents_3->setGeometry(QRect(0, 0, 592, 747));
        verticalLayout_27 = new QVBoxLayout(scrollAreaWidgetContents_3);
        verticalLayout_27->setSpacing(9);
        verticalLayout_27->setObjectName("verticalLayout_27");
        verticalLayout_27->setContentsMargins(18, 9, 18, 9);
        label_9 = new QLabel(scrollAreaWidgetContents_3);
        label_9->setObjectName("label_9");
#if QT_CONFIG(accessibility)
        label_9->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_27->addWidget(label_9);

        widget_4 = new QWidget(scrollAreaWidgetContents_3);
        widget_4->setObjectName("widget_4");
#if QT_CONFIG(accessibility)
        widget_4->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_27->addWidget(widget_4);

        verticalSpacer_8 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_27->addItem(verticalSpacer_8);

        displayGroup = new QWidget(scrollAreaWidgetContents_3);
        displayGroup->setObjectName("displayGroup");
#if QT_CONFIG(accessibility)
        displayGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_13 = new QVBoxLayout(displayGroup);
        verticalLayout_13->setSpacing(7);
        verticalLayout_13->setObjectName("verticalLayout_13");
        verticalLayout_13->setContentsMargins(13, 10, 13, 10);
        label_40 = new QLabel(displayGroup);
        label_40->setObjectName("label_40");
        label_40->setFont(font1);

        verticalLayout_13->addWidget(label_40);

        horizontalLayout_29 = new QHBoxLayout();
        horizontalLayout_29->setObjectName("horizontalLayout_29");
        horizontalLayout_29->setContentsMargins(0, 0, 0, -1);
        label_2 = new QLabel(displayGroup);
        label_2->setObjectName("label_2");
        sizePolicy3.setHeightForWidth(label_2->sizePolicy().hasHeightForWidth());
        label_2->setSizePolicy(sizePolicy3);

        horizontalLayout_29->addWidget(label_2);

        fitModeWindow = new QRadioButton(displayGroup);
        fitModeWindow->setObjectName("fitModeWindow");
        fitModeWindow->setChecked(false);

        horizontalLayout_29->addWidget(fitModeWindow);

        fitModeWidth = new QRadioButton(displayGroup);
        fitModeWidth->setObjectName("fitModeWidth");

        horizontalLayout_29->addWidget(fitModeWidth);

        fitMode1to1 = new QRadioButton(displayGroup);
        fitMode1to1->setObjectName("fitMode1to1");

        horizontalLayout_29->addWidget(fitMode1to1);

        fitModeHeight = new QRadioButton(displayGroup);
        fitModeHeight->setObjectName("fitModeHeight");

        horizontalLayout_29->addWidget(fitModeHeight);

        horizontalSpacer_21 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_29->addItem(horizontalSpacer_21);


        verticalLayout_13->addLayout(horizontalLayout_29);

        keepFitModeCheckBox = new QCheckBox(displayGroup);
        keepFitModeCheckBox->setObjectName("keepFitModeCheckBox");

        verticalLayout_13->addWidget(keepFitModeCheckBox);

        widget_3 = new QWidget(displayGroup);
        widget_3->setObjectName("widget_3");
#if QT_CONFIG(accessibility)
        widget_3->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_13->addWidget(widget_3);

        horizontalLayout_31 = new QHBoxLayout();
        horizontalLayout_31->setObjectName("horizontalLayout_31");
        horizontalLayout_31->setContentsMargins(0, 0, 0, -1);
        label_26 = new QLabel(displayGroup);
        label_26->setObjectName("label_26");
        sizePolicy4.setHeightForWidth(label_26->sizePolicy().hasHeightForWidth());
        label_26->setSizePolicy(sizePolicy4);

        horizontalLayout_31->addWidget(label_26);

        focus1to1Top = new QRadioButton(displayGroup);
        focus1to1Top->setObjectName("focus1to1Top");

        horizontalLayout_31->addWidget(focus1to1Top);

        focus1to1Center = new QRadioButton(displayGroup);
        focus1to1Center->setObjectName("focus1to1Center");

        horizontalLayout_31->addWidget(focus1to1Center);

        focus1to1Cursor = new QRadioButton(displayGroup);
        focus1to1Cursor->setObjectName("focus1to1Cursor");

        horizontalLayout_31->addWidget(focus1to1Cursor);

        horizontalSpacer_23 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_31->addItem(horizontalSpacer_23);


        verticalLayout_13->addLayout(horizontalLayout_31);

        label_10 = new QLabel(displayGroup);
        label_10->setObjectName("label_10");
#if QT_CONFIG(accessibility)
        label_10->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_13->addWidget(label_10);

        widget_8 = new QWidget(displayGroup);
        widget_8->setObjectName("widget_8");
#if QT_CONFIG(accessibility)
        widget_8->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_13->addWidget(widget_8);

        transparencyGridCheckBox = new QCheckBox(displayGroup);
        transparencyGridCheckBox->setObjectName("transparencyGridCheckBox");
        transparencyGridCheckBox->setEnabled(true);
        sizePolicy3.setHeightForWidth(transparencyGridCheckBox->sizePolicy().hasHeightForWidth());
        transparencyGridCheckBox->setSizePolicy(sizePolicy3);

        verticalLayout_13->addWidget(transparencyGridCheckBox);

        widget_21 = new QWidget(displayGroup);
        widget_21->setObjectName("widget_21");
#if QT_CONFIG(accessibility)
        widget_21->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_13->addWidget(widget_21);

        horizontalLayout_30 = new QHBoxLayout();
        horizontalLayout_30->setObjectName("horizontalLayout_30");
        horizontalLayout_30->setContentsMargins(0, 0, 0, -1);
        expandImageCheckBox = new QCheckBox(displayGroup);
        expandImageCheckBox->setObjectName("expandImageCheckBox");
        sizePolicy3.setHeightForWidth(expandImageCheckBox->sizePolicy().hasHeightForWidth());
        expandImageCheckBox->setSizePolicy(sizePolicy3);
        expandImageCheckBox->setMinimumSize(QSize(0, 0));

        horizontalLayout_30->addWidget(expandImageCheckBox);

        expandLimitSlider = new QSlider(displayGroup);
        expandLimitSlider->setObjectName("expandLimitSlider");
        sizePolicy5.setHeightForWidth(expandLimitSlider->sizePolicy().hasHeightForWidth());
        expandLimitSlider->setSizePolicy(sizePolicy5);
        expandLimitSlider->setMinimumSize(QSize(160, 0));
        expandLimitSlider->setMinimum(0);
        expandLimitSlider->setMaximum(4);
        expandLimitSlider->setPageStep(1);
        expandLimitSlider->setValue(2);
        expandLimitSlider->setOrientation(Qt::Orientation::Horizontal);
        expandLimitSlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        expandLimitSlider->setTickInterval(1);

        horizontalLayout_30->addWidget(expandLimitSlider);

        expandImagesGroupContents = new QWidget(displayGroup);
        expandImagesGroupContents->setObjectName("expandImagesGroupContents");
        horizontalLayout_17 = new QHBoxLayout(expandImagesGroupContents);
        horizontalLayout_17->setObjectName("horizontalLayout_17");
        horizontalLayout_17->setContentsMargins(0, 0, 0, 0);
        expandLimitLabel = new QLabel(expandImagesGroupContents);
        expandLimitLabel->setObjectName("expandLimitLabel");
        sizePolicy5.setHeightForWidth(expandLimitLabel->sizePolicy().hasHeightForWidth());
        expandLimitLabel->setSizePolicy(sizePolicy5);
        expandLimitLabel->setMinimumSize(QSize(35, 0));
        expandLimitLabel->setMaximumSize(QSize(35, 16777215));
        expandLimitLabel->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_17->addWidget(expandLimitLabel);


        horizontalLayout_30->addWidget(expandImagesGroupContents);

        horizontalSpacer_22 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_30->addItem(horizontalSpacer_22);


        verticalLayout_13->addLayout(horizontalLayout_30);

        label_13 = new QLabel(displayGroup);
        label_13->setObjectName("label_13");
#if QT_CONFIG(accessibility)
        label_13->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_13->addWidget(label_13);


        verticalLayout_27->addWidget(displayGroup);

        verticalSpacer_20 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_27->addItem(verticalSpacer_20);

        zoomGroup = new QWidget(scrollAreaWidgetContents_3);
        zoomGroup->setObjectName("zoomGroup");
#if QT_CONFIG(accessibility)
        zoomGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_8 = new QVBoxLayout(zoomGroup);
        verticalLayout_8->setSpacing(7);
        verticalLayout_8->setObjectName("verticalLayout_8");
        verticalLayout_8->setContentsMargins(13, 10, 13, 10);
        label_19 = new QLabel(zoomGroup);
        label_19->setObjectName("label_19");
        label_19->setFont(font1);

        verticalLayout_8->addWidget(label_19);

        unlockMinZoomCheckBox = new QCheckBox(zoomGroup);
        unlockMinZoomCheckBox->setObjectName("unlockMinZoomCheckBox");

        verticalLayout_8->addWidget(unlockMinZoomCheckBox);

        label_44 = new QLabel(zoomGroup);
        label_44->setObjectName("label_44");
#if QT_CONFIG(accessibility)
        label_44->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_8->addWidget(label_44);

        widget_11 = new QWidget(zoomGroup);
        widget_11->setObjectName("widget_11");
#if QT_CONFIG(accessibility)
        widget_11->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_8->addWidget(widget_11);

        horizontalLayout_20 = new QHBoxLayout();
        horizontalLayout_20->setSpacing(8);
        horizontalLayout_20->setObjectName("horizontalLayout_20");
        horizontalLayout_20->setContentsMargins(-1, 0, 0, -1);
        label_12 = new QLabel(zoomGroup);
        label_12->setObjectName("label_12");

        horizontalLayout_20->addWidget(label_12);

        zoomStepSlider = new QSlider(zoomGroup);
        zoomStepSlider->setObjectName("zoomStepSlider");
        QSizePolicy sizePolicy7(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy7.setHorizontalStretch(0);
        sizePolicy7.setVerticalStretch(0);
        sizePolicy7.setHeightForWidth(zoomStepSlider->sizePolicy().hasHeightForWidth());
        zoomStepSlider->setSizePolicy(sizePolicy7);
        zoomStepSlider->setMinimumSize(QSize(280, 0));
        zoomStepSlider->setMaximumSize(QSize(300, 16777215));
        zoomStepSlider->setMinimum(1);
        zoomStepSlider->setMaximum(50);
        zoomStepSlider->setPageStep(10);
        zoomStepSlider->setValue(20);
        zoomStepSlider->setOrientation(Qt::Orientation::Horizontal);
        zoomStepSlider->setTickPosition(QSlider::TickPosition::NoTicks);
        zoomStepSlider->setTickInterval(5);

        horizontalLayout_20->addWidget(zoomStepSlider);

        zoomStepLabel = new QLabel(zoomGroup);
        zoomStepLabel->setObjectName("zoomStepLabel");
        sizePolicy.setHeightForWidth(zoomStepLabel->sizePolicy().hasHeightForWidth());
        zoomStepLabel->setSizePolicy(sizePolicy);
        zoomStepLabel->setMinimumSize(QSize(60, 0));
        zoomStepLabel->setMaximumSize(QSize(60, 16777215));

        horizontalLayout_20->addWidget(zoomStepLabel);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_20->addItem(horizontalSpacer_4);


        verticalLayout_8->addLayout(horizontalLayout_20);

        widget_22 = new QWidget(zoomGroup);
        widget_22->setObjectName("widget_22");
#if QT_CONFIG(accessibility)
        widget_22->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_8->addWidget(widget_22);

        useFixedZoomLevelsCheckBox = new QCheckBox(zoomGroup);
        useFixedZoomLevelsCheckBox->setObjectName("useFixedZoomLevelsCheckBox");

        verticalLayout_8->addWidget(useFixedZoomLevelsCheckBox);

        widget = new QWidget(zoomGroup);
        widget->setObjectName("widget");
        widget->setEnabled(false);
        horizontalLayout_26 = new QHBoxLayout(widget);
        horizontalLayout_26->setSpacing(8);
        horizontalLayout_26->setObjectName("horizontalLayout_26");
        horizontalLayout_26->setContentsMargins(0, 0, 0, 0);
        zoomLevels = new QLineEdit(widget);
        zoomLevels->setObjectName("zoomLevels");
        QSizePolicy sizePolicy8(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);
        sizePolicy8.setHorizontalStretch(0);
        sizePolicy8.setVerticalStretch(0);
        sizePolicy8.setHeightForWidth(zoomLevels->sizePolicy().hasHeightForWidth());
        zoomLevels->setSizePolicy(sizePolicy8);
        zoomLevels->setMinimumSize(QSize(380, 24));

        horizontalLayout_26->addWidget(zoomLevels);

        resetZoomLevelsButton = new QPushButton(widget);
        resetZoomLevelsButton->setObjectName("resetZoomLevelsButton");

        horizontalLayout_26->addWidget(resetZoomLevelsButton);

        horizontalSpacer_30 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_26->addItem(horizontalSpacer_30);


        verticalLayout_8->addWidget(widget);


        verticalLayout_27->addWidget(zoomGroup);

        verticalSpacer_13 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_27->addItem(verticalSpacer_13);

        scalingGroup = new QWidget(scrollAreaWidgetContents_3);
        scalingGroup->setObjectName("scalingGroup");
#if QT_CONFIG(accessibility)
        scalingGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_24 = new QVBoxLayout(scalingGroup);
        verticalLayout_24->setSpacing(7);
        verticalLayout_24->setObjectName("verticalLayout_24");
        verticalLayout_24->setContentsMargins(13, 10, 13, 10);
        title3 = new QLabel(scalingGroup);
        title3->setObjectName("title3");
        title3->setFont(font1);

        verticalLayout_24->addWidget(title3);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        scalingQualityLabel = new QLabel(scalingGroup);
        scalingQualityLabel->setObjectName("scalingQualityLabel");
        scalingQualityLabel->setEnabled(true);
        sizePolicy3.setHeightForWidth(scalingQualityLabel->sizePolicy().hasHeightForWidth());
        scalingQualityLabel->setSizePolicy(sizePolicy3);
        scalingQualityLabel->setMinimumSize(QSize(0, 0));

        horizontalLayout_5->addWidget(scalingQualityLabel);

        scalingQualityComboBox = new QComboBox(scalingGroup);
        scalingQualityComboBox->addItem(QString());
        scalingQualityComboBox->addItem(QString());
        scalingQualityComboBox->setObjectName("scalingQualityComboBox");
        scalingQualityComboBox->setEnabled(true);
        sizePolicy3.setHeightForWidth(scalingQualityComboBox->sizePolicy().hasHeightForWidth());
        scalingQualityComboBox->setSizePolicy(sizePolicy3);
        scalingQualityComboBox->setMinimumSize(QSize(0, 0));
        scalingQualityComboBox->setToolTipDuration(-1);

        horizontalLayout_5->addWidget(scalingQualityComboBox);

        horizontalSpacer_29 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_29);


        verticalLayout_24->addLayout(horizontalLayout_5);

        widget_10 = new QWidget(scalingGroup);
        widget_10->setObjectName("widget_10");
#if QT_CONFIG(accessibility)
        widget_10->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        QSizePolicy sizePolicy9(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::Minimum);
        sizePolicy9.setHorizontalStretch(0);
        sizePolicy9.setVerticalStretch(0);

        verticalLayout_27->addWidget(scalingGroup);

        verticalSpacer_7 = new QSpacerItem(20, 215, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_27->addItem(verticalSpacer_7);

        scrollArea_3->setWidget(scrollAreaWidgetContents_3);

        verticalLayout_19->addWidget(scrollArea_3);

        stackedWidget->addWidget(View);
        Theme = new QWidget();
        Theme->setObjectName("Theme");
        verticalLayout = new QVBoxLayout(Theme);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        scrollArea_2 = new QScrollArea(Theme);
        scrollArea_2->setObjectName("scrollArea_2");
        scrollArea_2->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea_2->setWidgetResizable(true);
        scrollAreaWidgetContents_2 = new QWidget();
        scrollAreaWidgetContents_2->setObjectName("scrollAreaWidgetContents_2");
        scrollAreaWidgetContents_2->setGeometry(QRect(0, 0, 609, 699));
        verticalLayout_25 = new QVBoxLayout(scrollAreaWidgetContents_2);
        verticalLayout_25->setSpacing(9);
        verticalLayout_25->setObjectName("verticalLayout_25");
        verticalLayout_25->setContentsMargins(18, 9, 18, 9);
        label_45 = new QLabel(scrollAreaWidgetContents_2);
        label_45->setObjectName("label_45");
#if QT_CONFIG(accessibility)
        label_45->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_25->addWidget(label_45);

        widget_23 = new QWidget(scrollAreaWidgetContents_2);
        widget_23->setObjectName("widget_23");
#if QT_CONFIG(accessibility)
        widget_23->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_25->addWidget(widget_23);

        verticalSpacer_4 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_25->addItem(verticalSpacer_4);

        colorsPresetGroup = new QWidget(scrollAreaWidgetContents_2);
        colorsPresetGroup->setObjectName("colorsPresetGroup");
#if QT_CONFIG(accessibility)
        colorsPresetGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_38 = new QVBoxLayout(colorsPresetGroup);
        verticalLayout_38->setSpacing(7);
        verticalLayout_38->setObjectName("verticalLayout_38");
        verticalLayout_38->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        horizontalLayout_3->setContentsMargins(0, 0, -1, -1);
        loadPresetLabel = new QLabel(colorsPresetGroup);
        loadPresetLabel->setObjectName("loadPresetLabel");

        horizontalLayout_3->addWidget(loadPresetLabel);

        themeSelectorComboBox = new QComboBox(colorsPresetGroup);
        themeSelectorComboBox->addItem(QString());
        themeSelectorComboBox->addItem(QString());
        themeSelectorComboBox->addItem(QString());
        themeSelectorComboBox->addItem(QString());
        themeSelectorComboBox->setObjectName("themeSelectorComboBox");
        sizePolicy3.setHeightForWidth(themeSelectorComboBox->sizePolicy().hasHeightForWidth());
        themeSelectorComboBox->setSizePolicy(sizePolicy3);

        horizontalLayout_3->addWidget(themeSelectorComboBox);

        horizontalSpacer_13 = new QSpacerItem(10, 10, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_13);

        useSystemColorsCheckBox = new QCheckBox(colorsPresetGroup);
        useSystemColorsCheckBox->setObjectName("useSystemColorsCheckBox");

        horizontalLayout_3->addWidget(useSystemColorsCheckBox);

        modifySystemSchemeLabel = new ClickableLabel(colorsPresetGroup);
        modifySystemSchemeLabel->setObjectName("modifySystemSchemeLabel");
        modifySystemSchemeLabel->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
        modifySystemSchemeLabel->setTextFormat(Qt::TextFormat::RichText);

        horizontalLayout_3->addWidget(modifySystemSchemeLabel);

        horizontalSpacer_18 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_18);


        verticalLayout_38->addLayout(horizontalLayout_3);


        verticalLayout_25->addWidget(colorsPresetGroup);

        verticalSpacer_9 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_25->addItem(verticalSpacer_9);

        colorsGroup = new QWidget(scrollAreaWidgetContents_2);
        colorsGroup->setObjectName("colorsGroup");
#if QT_CONFIG(accessibility)
        colorsGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_23 = new QVBoxLayout(colorsGroup);
        verticalLayout_23->setSpacing(7);
        verticalLayout_23->setObjectName("verticalLayout_23");
        verticalLayout_23->setContentsMargins(13, 10, 13, 10);
        colorConfigSubgroup = new QWidget(colorsGroup);
        colorConfigSubgroup->setObjectName("colorConfigSubgroup");
        gridLayout_2 = new QGridLayout(colorConfigSubgroup);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout_2->setHorizontalSpacing(8);
        gridLayout_2->setVerticalSpacing(7);
        gridLayout_2->setContentsMargins(1, 1, 1, 1);
        colorSelectorAccent = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorAccent->setObjectName("colorSelectorAccent");
        sizePolicy2.setHeightForWidth(colorSelectorAccent->sizePolicy().hasHeightForWidth());
        colorSelectorAccent->setSizePolicy(sizePolicy2);
        colorSelectorAccent->setMinimumSize(QSize(40, 22));
        colorSelectorAccent->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorAccent->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorAccent->setAutoFillBackground(true);
        colorSelectorAccent->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorAccent, 0, 0, 1, 1);

        colorSelectorIcons = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorIcons->setObjectName("colorSelectorIcons");
        sizePolicy2.setHeightForWidth(colorSelectorIcons->sizePolicy().hasHeightForWidth());
        colorSelectorIcons->setSizePolicy(sizePolicy2);
        colorSelectorIcons->setMinimumSize(QSize(40, 22));
#if QT_CONFIG(accessibility)
        colorSelectorIcons->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorIcons->setAutoFillBackground(true);
        colorSelectorIcons->setFrameShape(QFrame::Shape::Box);
        colorSelectorIcons->setText(QString::fromUtf8(""));

        gridLayout_2->addWidget(colorSelectorIcons, 3, 2, 1, 1);

        colorSelectorFullscreen = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorFullscreen->setObjectName("colorSelectorFullscreen");
        sizePolicy2.setHeightForWidth(colorSelectorFullscreen->sizePolicy().hasHeightForWidth());
        colorSelectorFullscreen->setSizePolicy(sizePolicy2);
        colorSelectorFullscreen->setMinimumSize(QSize(40, 22));
        colorSelectorFullscreen->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorFullscreen->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorFullscreen->setAutoFillBackground(true);
        colorSelectorFullscreen->setFrameShape(QFrame::Shape::Box);
        colorSelectorFullscreen->setText(QString::fromUtf8(""));

        gridLayout_2->addWidget(colorSelectorFullscreen, 1, 2, 1, 1);

        colorSelectorBackground = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorBackground->setObjectName("colorSelectorBackground");
        sizePolicy2.setHeightForWidth(colorSelectorBackground->sizePolicy().hasHeightForWidth());
        colorSelectorBackground->setSizePolicy(sizePolicy2);
        colorSelectorBackground->setMinimumSize(QSize(40, 22));
        colorSelectorBackground->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorBackground->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorBackground->setAutoFillBackground(true);
        colorSelectorBackground->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorBackground, 1, 0, 1, 1);

        horizontalSpacer_28 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        gridLayout_2->addItem(horizontalSpacer_28, 3, 4, 1, 1);

        label_33 = new QLabel(colorConfigSubgroup);
        label_33->setObjectName("label_33");
        label_33->setMinimumSize(QSize(180, 0));

        gridLayout_2->addWidget(label_33, 0, 1, 1, 1);

        label_34 = new QLabel(colorConfigSubgroup);
        label_34->setObjectName("label_34");

        gridLayout_2->addWidget(label_34, 1, 1, 1, 1);

        label_35 = new QLabel(colorConfigSubgroup);
        label_35->setObjectName("label_35");

        gridLayout_2->addWidget(label_35, 1, 3, 1, 1);

        colorSelectorOverlayText = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorOverlayText->setObjectName("colorSelectorOverlayText");
        sizePolicy2.setHeightForWidth(colorSelectorOverlayText->sizePolicy().hasHeightForWidth());
        colorSelectorOverlayText->setSizePolicy(sizePolicy2);
        colorSelectorOverlayText->setMinimumSize(QSize(40, 22));
        colorSelectorOverlayText->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorOverlayText->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorOverlayText->setAutoFillBackground(true);
        colorSelectorOverlayText->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorOverlayText, 6, 2, 1, 1);

        label_11 = new QLabel(colorConfigSubgroup);
        label_11->setObjectName("label_11");

        gridLayout_2->addWidget(label_11, 3, 1, 1, 1);

        label_14 = new QLabel(colorConfigSubgroup);
        label_14->setObjectName("label_14");

        gridLayout_2->addWidget(label_14, 3, 3, 1, 1);

        colorSelectorFolderview = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorFolderview->setObjectName("colorSelectorFolderview");
        sizePolicy2.setHeightForWidth(colorSelectorFolderview->sizePolicy().hasHeightForWidth());
        colorSelectorFolderview->setSizePolicy(sizePolicy2);
        colorSelectorFolderview->setMinimumSize(QSize(40, 22));
        colorSelectorFolderview->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorFolderview->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorFolderview->setAutoFillBackground(true);
        colorSelectorFolderview->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorFolderview, 4, 0, 1, 1);

        colorSelectorText = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorText->setObjectName("colorSelectorText");
        sizePolicy2.setHeightForWidth(colorSelectorText->sizePolicy().hasHeightForWidth());
        colorSelectorText->setSizePolicy(sizePolicy2);
        colorSelectorText->setMinimumSize(QSize(40, 22));
        colorSelectorText->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorText->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorText->setAutoFillBackground(true);
        colorSelectorText->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorText, 3, 0, 1, 1);

        colorSelectorWidgetBorder = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorWidgetBorder->setObjectName("colorSelectorWidgetBorder");
        sizePolicy2.setHeightForWidth(colorSelectorWidgetBorder->sizePolicy().hasHeightForWidth());
        colorSelectorWidgetBorder->setSizePolicy(sizePolicy2);
        colorSelectorWidgetBorder->setMinimumSize(QSize(40, 22));
        colorSelectorWidgetBorder->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorWidgetBorder->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorWidgetBorder->setAutoFillBackground(true);
        colorSelectorWidgetBorder->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorWidgetBorder, 5, 2, 1, 1);

        label_36 = new QLabel(colorConfigSubgroup);
        label_36->setObjectName("label_36");

        gridLayout_2->addWidget(label_36, 6, 1, 1, 1);

        colorSelectorOverlay = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorOverlay->setObjectName("colorSelectorOverlay");
        sizePolicy2.setHeightForWidth(colorSelectorOverlay->sizePolicy().hasHeightForWidth());
        colorSelectorOverlay->setSizePolicy(sizePolicy2);
        colorSelectorOverlay->setMinimumSize(QSize(40, 22));
        colorSelectorOverlay->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorOverlay->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorOverlay->setAutoFillBackground(true);
        colorSelectorOverlay->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorOverlay, 6, 0, 1, 1);

        label_21 = new QLabel(colorConfigSubgroup);
        label_21->setObjectName("label_21");

        gridLayout_2->addWidget(label_21, 5, 1, 1, 1);

        label_31 = new QLabel(colorConfigSubgroup);
        label_31->setObjectName("label_31");

        gridLayout_2->addWidget(label_31, 4, 3, 1, 1);

        label_22 = new QLabel(colorConfigSubgroup);
        label_22->setObjectName("label_22");

        gridLayout_2->addWidget(label_22, 5, 3, 1, 1);

        colorSelectorScrollbar = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorScrollbar->setObjectName("colorSelectorScrollbar");
        sizePolicy2.setHeightForWidth(colorSelectorScrollbar->sizePolicy().hasHeightForWidth());
        colorSelectorScrollbar->setSizePolicy(sizePolicy2);
        colorSelectorScrollbar->setMinimumSize(QSize(40, 22));
        colorSelectorScrollbar->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorScrollbar->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorScrollbar->setAutoFillBackground(true);
        colorSelectorScrollbar->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorScrollbar, 7, 0, 1, 1);

        colorSelectorFolderviewPanel = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorFolderviewPanel->setObjectName("colorSelectorFolderviewPanel");
        sizePolicy2.setHeightForWidth(colorSelectorFolderviewPanel->sizePolicy().hasHeightForWidth());
        colorSelectorFolderviewPanel->setSizePolicy(sizePolicy2);
        colorSelectorFolderviewPanel->setMinimumSize(QSize(40, 22));
        colorSelectorFolderviewPanel->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorFolderviewPanel->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorFolderviewPanel->setAutoFillBackground(true);
        colorSelectorFolderviewPanel->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorFolderviewPanel, 4, 2, 1, 1);

        label_37 = new QLabel(colorConfigSubgroup);
        label_37->setObjectName("label_37");

        gridLayout_2->addWidget(label_37, 6, 3, 1, 1);

        colorSelectorWidget = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorWidget->setObjectName("colorSelectorWidget");
        sizePolicy2.setHeightForWidth(colorSelectorWidget->sizePolicy().hasHeightForWidth());
        colorSelectorWidget->setSizePolicy(sizePolicy2);
        colorSelectorWidget->setMinimumSize(QSize(40, 22));
        colorSelectorWidget->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorWidget->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorWidget->setAutoFillBackground(true);
        colorSelectorWidget->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorWidget, 5, 0, 1, 1);

        label_32 = new QLabel(colorConfigSubgroup);
        label_32->setObjectName("label_32");

        gridLayout_2->addWidget(label_32, 7, 1, 1, 1);

        colorSelectorThumbpanel = new ColorSelectorButton(colorConfigSubgroup);
        colorSelectorThumbpanel->setObjectName("colorSelectorThumbpanel");
        sizePolicy2.setHeightForWidth(colorSelectorThumbpanel->sizePolicy().hasHeightForWidth());
        colorSelectorThumbpanel->setSizePolicy(sizePolicy2);
        colorSelectorThumbpanel->setMinimumSize(QSize(40, 22));
        colorSelectorThumbpanel->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
#if QT_CONFIG(accessibility)
        colorSelectorThumbpanel->setAccessibleName(QString::fromUtf8(""));
#endif // QT_CONFIG(accessibility)
        colorSelectorThumbpanel->setAutoFillBackground(true);
        colorSelectorThumbpanel->setFrameShape(QFrame::Shape::Box);

        gridLayout_2->addWidget(colorSelectorThumbpanel, 7, 2, 1, 1);

        label_thumbpanel = new QLabel(colorConfigSubgroup);
        label_thumbpanel->setObjectName("label_thumbpanel");

        gridLayout_2->addWidget(label_thumbpanel, 7, 3, 1, 1);

        label_23 = new QLabel(colorConfigSubgroup);
        label_23->setObjectName("label_23");

        gridLayout_2->addWidget(label_23, 4, 1, 1, 1);

        widget_24 = new QWidget(colorConfigSubgroup);
        widget_24->setObjectName("widget_24");
        widget_24->setMinimumSize(QSize(0, 1));
#if QT_CONFIG(accessibility)
        widget_24->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        gridLayout_2->addWidget(widget_24, 2, 0, 1, 5);


        verticalLayout_23->addWidget(colorConfigSubgroup);


        verticalLayout_25->addWidget(colorsGroup);

        verticalSpacer_14 = new QSpacerItem(20, 12, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_25->addItem(verticalSpacer_14);

        windowTweaksGroup = new QWidget(scrollAreaWidgetContents_2);
        windowTweaksGroup->setObjectName("windowTweaksGroup");
#if QT_CONFIG(accessibility)
        windowTweaksGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_39 = new QVBoxLayout(windowTweaksGroup);
        verticalLayout_39->setSpacing(7);
        verticalLayout_39->setObjectName("verticalLayout_39");
        verticalLayout_39->setContentsMargins(13, 10, 13, 10);
        label_38 = new QLabel(windowTweaksGroup);
        label_38->setObjectName("label_38");
        label_38->setFont(font1);
        label_38->setFrameShape(QFrame::Shape::NoFrame);
        label_38->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        verticalLayout_39->addWidget(label_38);

        opacitySlidersGridLayout = new QGridLayout();
        opacitySlidersGridLayout->setObjectName("opacitySlidersGridLayout");
        opacitySlidersGridLayout->setHorizontalSpacing(6);
        opacitySlidersGridLayout->setVerticalSpacing(7);
        opacitySlidersGridLayout->setContentsMargins(0, 0, 0, 0);
        label_5 = new QLabel(windowTweaksGroup);
        label_5->setObjectName("label_5");
        label_5->setMinimumSize(QSize(140, 0));

        opacitySlidersGridLayout->addWidget(label_5, 0, 0, 1, 1);

        bgOpacitySlider = new QSlider(windowTweaksGroup);
        bgOpacitySlider->setObjectName("bgOpacitySlider");
        sizePolicy.setHeightForWidth(bgOpacitySlider->sizePolicy().hasHeightForWidth());
        bgOpacitySlider->setSizePolicy(sizePolicy);
        bgOpacitySlider->setMinimumSize(QSize(190, 10));
        bgOpacitySlider->setMaximum(100);
        bgOpacitySlider->setOrientation(Qt::Orientation::Horizontal);
        bgOpacitySlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        bgOpacitySlider->setTickInterval(10);

        opacitySlidersGridLayout->addWidget(bgOpacitySlider, 0, 1, 1, 1);

        bgOpacityPercentLabel = new QLabel(windowTweaksGroup);
        bgOpacityPercentLabel->setObjectName("bgOpacityPercentLabel");
        sizePolicy5.setHeightForWidth(bgOpacityPercentLabel->sizePolicy().hasHeightForWidth());
        bgOpacityPercentLabel->setSizePolicy(sizePolicy5);
        bgOpacityPercentLabel->setMinimumSize(QSize(60, 0));
        bgOpacityPercentLabel->setMaximumSize(QSize(60, 16777215));
        bgOpacityPercentLabel->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        opacitySlidersGridLayout->addWidget(bgOpacityPercentLabel, 0, 2, 1, 1);

        horizontalSpacer_14 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        opacitySlidersGridLayout->addItem(horizontalSpacer_14, 0, 3, 1, 1);

        label_5_thumb = new QLabel(windowTweaksGroup);
        label_5_thumb->setObjectName("label_5_thumb");
        label_5_thumb->setMinimumSize(QSize(140, 0));

        opacitySlidersGridLayout->addWidget(label_5_thumb, 1, 0, 1, 1);

        thumbOpacitySlider = new QSlider(windowTweaksGroup);
        thumbOpacitySlider->setObjectName("thumbOpacitySlider");
        sizePolicy.setHeightForWidth(thumbOpacitySlider->sizePolicy().hasHeightForWidth());
        thumbOpacitySlider->setSizePolicy(sizePolicy);
        thumbOpacitySlider->setMinimumSize(QSize(190, 10));
        thumbOpacitySlider->setMaximum(100);
        thumbOpacitySlider->setOrientation(Qt::Orientation::Horizontal);
        thumbOpacitySlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        thumbOpacitySlider->setTickInterval(10);

        opacitySlidersGridLayout->addWidget(thumbOpacitySlider, 1, 1, 1, 1);

        thumbOpacityPercentLabel = new QLabel(windowTweaksGroup);
        thumbOpacityPercentLabel->setObjectName("thumbOpacityPercentLabel");
        sizePolicy5.setHeightForWidth(thumbOpacityPercentLabel->sizePolicy().hasHeightForWidth());
        thumbOpacityPercentLabel->setSizePolicy(sizePolicy5);
        thumbOpacityPercentLabel->setMinimumSize(QSize(60, 0));
        thumbOpacityPercentLabel->setMaximumSize(QSize(60, 16777215));
        thumbOpacityPercentLabel->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        opacitySlidersGridLayout->addWidget(thumbOpacityPercentLabel, 1, 2, 1, 1);

        horizontalSpacer_14_thumb = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        opacitySlidersGridLayout->addItem(horizontalSpacer_14_thumb, 1, 3, 1, 1);


        verticalLayout_39->addLayout(opacitySlidersGridLayout);

        useBlackBackgroundCheckBox = new QCheckBox(windowTweaksGroup);
        useBlackBackgroundCheckBox->setObjectName("useBlackBackgroundCheckBox");

        verticalLayout_39->addWidget(useBlackBackgroundCheckBox);


        verticalLayout_25->addWidget(windowTweaksGroup);

        verticalSpacer_11 = new QSpacerItem(20, 152, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_25->addItem(verticalSpacer_11);

        scrollArea_2->setWidget(scrollAreaWidgetContents_2);

        verticalLayout->addWidget(scrollArea_2);

        stackedWidget->addWidget(Theme);
        Controls = new QWidget();
        Controls->setObjectName("Controls");
        verticalLayout_28 = new QVBoxLayout(Controls);
        verticalLayout_28->setSpacing(9);
        verticalLayout_28->setObjectName("verticalLayout_28");
        verticalLayout_28->setContentsMargins(18, 9, 18, 9);
        label_29 = new QLabel(Controls);
        label_29->setObjectName("label_29");
#if QT_CONFIG(accessibility)
        label_29->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_28->addWidget(label_29);

        widget_5 = new QWidget(Controls);
        widget_5->setObjectName("widget_5");
#if QT_CONFIG(accessibility)
        widget_5->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_28->addWidget(widget_5);

        verticalSpacer_10 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_28->addItem(verticalSpacer_10);

        verticalLayout_33 = new QVBoxLayout();
        verticalLayout_33->setSpacing(7);
        verticalLayout_33->setObjectName("verticalLayout_33");
        verticalLayout_33->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        pushButton_2 = new QPushButton(Controls);
        pushButton_2->setObjectName("pushButton_2");
        sizePolicy3.setHeightForWidth(pushButton_2->sizePolicy().hasHeightForWidth());
        pushButton_2->setSizePolicy(sizePolicy3);
        pushButton_2->setMinimumSize(QSize(0, 0));

        horizontalLayout_2->addWidget(pushButton_2);

        pushButton_8 = new QPushButton(Controls);
        pushButton_8->setObjectName("pushButton_8");

        horizontalLayout_2->addWidget(pushButton_8);

        pushButton_4 = new QPushButton(Controls);
        pushButton_4->setObjectName("pushButton_4");
        sizePolicy3.setHeightForWidth(pushButton_4->sizePolicy().hasHeightForWidth());
        pushButton_4->setSizePolicy(sizePolicy3);

        horizontalLayout_2->addWidget(pushButton_4);

        horizontalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_2);

        pushButton_3 = new QPushButton(Controls);
        pushButton_3->setObjectName("pushButton_3");
        sizePolicy.setHeightForWidth(pushButton_3->sizePolicy().hasHeightForWidth());
        pushButton_3->setSizePolicy(sizePolicy);

        horizontalLayout_2->addWidget(pushButton_3);


        verticalLayout_33->addLayout(horizontalLayout_2);

        shortcutsTableView = new QTableView(Controls);
        shortcutsTableView->setObjectName("shortcutsTableView");
        QSizePolicy sizePolicy10(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy10.setHorizontalStretch(0);
        sizePolicy10.setVerticalStretch(1);
        sizePolicy10.setHeightForWidth(shortcutsTableView->sizePolicy().hasHeightForWidth());
        shortcutsTableView->setSizePolicy(sizePolicy10);
        shortcutsTableView->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
        shortcutsTableView->setAlternatingRowColors(true);
        shortcutsTableView->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
        shortcutsTableView->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
        shortcutsTableView->setSortingEnabled(false);
        shortcutsTableView->verticalHeader()->setVisible(false);

        verticalLayout_33->addWidget(shortcutsTableView);


        verticalLayout_28->addLayout(verticalLayout_33);

        verticalSpacer_18 = new QSpacerItem(20, 9, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_28->addItem(verticalSpacer_18);

        widget_12 = new QWidget(Controls);
        widget_12->setObjectName("widget_12");
#if QT_CONFIG(accessibility)
        widget_12->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_21 = new QVBoxLayout(widget_12);
        verticalLayout_21->setSpacing(7);
        verticalLayout_21->setObjectName("verticalLayout_21");
        verticalLayout_21->setContentsMargins(13, 10, 13, 10);
        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        horizontalLayout_8->setContentsMargins(0, 0, -1, -1);
        clickableEdgesCheckBox = new QCheckBox(widget_12);
        clickableEdgesCheckBox->setObjectName("clickableEdgesCheckBox");

        horizontalLayout_8->addWidget(clickableEdgesCheckBox);

        clickableEdgesVisibleCheckBox = new QCheckBox(widget_12);
        clickableEdgesVisibleCheckBox->setObjectName("clickableEdgesVisibleCheckBox");

        horizontalLayout_8->addWidget(clickableEdgesVisibleCheckBox);


        verticalLayout_21->addLayout(horizontalLayout_8);

        widget_30 = new QWidget(widget_12);
        widget_30->setObjectName("widget_30");
#if QT_CONFIG(accessibility)
        widget_30->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_21->addWidget(widget_30);

        horizontalLayout_12 = new QHBoxLayout();
        horizontalLayout_12->setObjectName("horizontalLayout_12");
        horizontalLayout_12->setContentsMargins(0, 0, 0, 0);
        label_28 = new QLabel(widget_12);
        label_28->setObjectName("label_28");

        horizontalLayout_12->addWidget(label_28);

        imageScrollingComboBox = new QComboBox(widget_12);
        imageScrollingComboBox->addItem(QString());
        imageScrollingComboBox->addItem(QString());
        imageScrollingComboBox->addItem(QString());
        imageScrollingComboBox->setObjectName("imageScrollingComboBox");

        horizontalLayout_12->addWidget(imageScrollingComboBox);

        horizontalSpacer_31 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_12->addItem(horizontalSpacer_31);


        verticalLayout_21->addLayout(horizontalLayout_12);

        label_52 = new QLabel(widget_12);
        label_52->setObjectName("label_52");
#if QT_CONFIG(accessibility)
        label_52->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_21->addWidget(label_52);

        widget_29 = new QWidget(widget_12);
        widget_29->setObjectName("widget_29");
#if QT_CONFIG(accessibility)
        widget_29->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_21->addWidget(widget_29);

        horizontalLayout_9 = new QHBoxLayout();
        horizontalLayout_9->setObjectName("horizontalLayout_9");
        horizontalLayout_9->setContentsMargins(0, 0, 0, 0);
        label_54 = new QLabel(widget_12);
        label_54->setObjectName("label_54");

        horizontalLayout_9->addWidget(label_54);

        mouseScrollingSpeedSlider = new QSlider(widget_12);
        mouseScrollingSpeedSlider->setObjectName("mouseScrollingSpeedSlider");
        mouseScrollingSpeedSlider->setMinimumSize(QSize(180, 0));
        mouseScrollingSpeedSlider->setMinimum(0);
        mouseScrollingSpeedSlider->setMaximum(6);
        mouseScrollingSpeedSlider->setSingleStep(1);
        mouseScrollingSpeedSlider->setPageStep(1);
        mouseScrollingSpeedSlider->setValue(3);
        mouseScrollingSpeedSlider->setSliderPosition(3);
        mouseScrollingSpeedSlider->setOrientation(Qt::Orientation::Horizontal);
        mouseScrollingSpeedSlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        mouseScrollingSpeedSlider->setTickInterval(1);

        horizontalLayout_9->addWidget(mouseScrollingSpeedSlider);

        mouseScrollingSpeedLabel = new QLabel(widget_12);
        mouseScrollingSpeedLabel->setObjectName("mouseScrollingSpeedLabel");

        horizontalLayout_9->addWidget(mouseScrollingSpeedLabel);

        horizontalSpacer_34 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_9->addItem(horizontalSpacer_34);


        verticalLayout_21->addLayout(horizontalLayout_9);

        widget_31_v2 = new QWidget(widget_12);
        widget_31_v2->setObjectName("widget_31_v2");
#if QT_CONFIG(accessibility)
        widget_31_v2->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_21->addWidget(widget_31_v2);

        trackpadDetectionCheckBox = new QCheckBox(widget_12);
        trackpadDetectionCheckBox->setObjectName("trackpadDetectionCheckBox");

        verticalLayout_21->addWidget(trackpadDetectionCheckBox);

        label_7 = new QLabel(widget_12);
        label_7->setObjectName("label_7");
#if QT_CONFIG(accessibility)
        label_7->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_21->addWidget(label_7);


        verticalLayout_28->addWidget(widget_12);

        stackedWidget->addWidget(Controls);
        Scripts = new QWidget();
        Scripts->setObjectName("Scripts");
        verticalLayout_29 = new QVBoxLayout(Scripts);
        verticalLayout_29->setSpacing(9);
        verticalLayout_29->setObjectName("verticalLayout_29");
        verticalLayout_29->setContentsMargins(18, 9, 18, 9);
        label_47 = new QLabel(Scripts);
        label_47->setObjectName("label_47");
#if QT_CONFIG(accessibility)
        label_47->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_29->addWidget(label_47);

        widget_14 = new QWidget(Scripts);
        widget_14->setObjectName("widget_14");
        sizePolicy6.setHeightForWidth(widget_14->sizePolicy().hasHeightForWidth());
        widget_14->setSizePolicy(sizePolicy6);
#if QT_CONFIG(accessibility)
        widget_14->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_29->addWidget(widget_14);

        verticalSpacer_12 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_29->addItem(verticalSpacer_12);

        verticalLayout_35 = new QVBoxLayout();
        verticalLayout_35->setSpacing(7);
        verticalLayout_35->setObjectName("verticalLayout_35");
        verticalLayout_35->setContentsMargins(0, 0, 0, 0);
        label_24 = new QLabel(Scripts);
        label_24->setObjectName("label_24");
#if QT_CONFIG(accessibility)
        label_24->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_35->addWidget(label_24);

        label_43 = new QLabel(Scripts);
        label_43->setObjectName("label_43");
#if QT_CONFIG(accessibility)
        label_43->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_35->addWidget(label_43);

        horizontalLayout_21 = new QHBoxLayout();
        horizontalLayout_21->setObjectName("horizontalLayout_21");
        horizontalLayout_21->setContentsMargins(-1, 0, -1, -1);
        pushButton_9 = new QPushButton(Scripts);
        pushButton_9->setObjectName("pushButton_9");

        horizontalLayout_21->addWidget(pushButton_9);

        pushButton_6 = new QPushButton(Scripts);
        pushButton_6->setObjectName("pushButton_6");

        horizontalLayout_21->addWidget(pushButton_6);

        pushButton_7 = new QPushButton(Scripts);
        pushButton_7->setObjectName("pushButton_7");

        horizontalLayout_21->addWidget(pushButton_7);

        horizontalSpacer_5 = new QSpacerItem(20, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_21->addItem(horizontalSpacer_5);


        verticalLayout_35->addLayout(horizontalLayout_21);

        scriptsListView = new QListView(Scripts);
        scriptsListView->setObjectName("scriptsListView");
        scriptsListView->setAlternatingRowColors(true);
        scriptsListView->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);

        verticalLayout_35->addWidget(scriptsListView);


        verticalLayout_29->addLayout(verticalLayout_35);

        stackedWidget->addWidget(Scripts);
        Advanced = new QWidget();
        Advanced->setObjectName("Advanced");
        verticalLayout_30 = new QVBoxLayout(Advanced);
        verticalLayout_30->setObjectName("verticalLayout_30");
        verticalLayout_30->setContentsMargins(0, 0, 0, 0);
        scrollArea_4 = new QScrollArea(Advanced);
        scrollArea_4->setObjectName("scrollArea_4");
        scrollArea_4->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea_4->setWidgetResizable(true);
        scrollAreaWidgetContents_4 = new QWidget();
        scrollAreaWidgetContents_4->setObjectName("scrollAreaWidgetContents_4");
        scrollAreaWidgetContents_4->setGeometry(QRect(0, 0, 609, 699));
        verticalLayout_31 = new QVBoxLayout(scrollAreaWidgetContents_4);
        verticalLayout_31->setSpacing(9);
        verticalLayout_31->setObjectName("verticalLayout_31");
        verticalLayout_31->setContentsMargins(18, 9, 18, 9);
        label_49 = new QLabel(scrollAreaWidgetContents_4);
        label_49->setObjectName("label_49");
#if QT_CONFIG(accessibility)
        label_49->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_31->addWidget(label_49);

        widget_13 = new QWidget(scrollAreaWidgetContents_4);
        widget_13->setObjectName("widget_13");
#if QT_CONFIG(accessibility)
        widget_13->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_31->addWidget(widget_13);

        verticalSpacer_17 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_31->addItem(verticalSpacer_17);

        advancedGroup = new QWidget(scrollAreaWidgetContents_4);
        advancedGroup->setObjectName("advancedGroup");
#if QT_CONFIG(accessibility)
        advancedGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_34 = new QVBoxLayout(advancedGroup);
        verticalLayout_34->setSpacing(7);
        verticalLayout_34->setObjectName("verticalLayout_34");
        verticalLayout_34->setContentsMargins(13, 10, 13, 10);
        usePreloaderCheckBox = new QCheckBox(advancedGroup);
        usePreloaderCheckBox->setObjectName("usePreloaderCheckBox");
        sizePolicy9.setHeightForWidth(usePreloaderCheckBox->sizePolicy().hasHeightForWidth());
        usePreloaderCheckBox->setSizePolicy(sizePolicy9);

        verticalLayout_34->addWidget(usePreloaderCheckBox);

        label_41 = new QLabel(advancedGroup);
        label_41->setObjectName("label_41");
#if QT_CONFIG(accessibility)
        label_41->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(label_41);

        widget_15 = new QWidget(advancedGroup);
        widget_15->setObjectName("widget_15");
#if QT_CONFIG(accessibility)
        widget_15->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(widget_15);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        label_17 = new QLabel(advancedGroup);
        label_17->setObjectName("label_17");

        horizontalLayout->addWidget(label_17);

        thumbnailerThreadsSlider = new QSlider(advancedGroup);
        thumbnailerThreadsSlider->setObjectName("thumbnailerThreadsSlider");
        sizePolicy2.setHeightForWidth(thumbnailerThreadsSlider->sizePolicy().hasHeightForWidth());
        thumbnailerThreadsSlider->setSizePolicy(sizePolicy2);
        thumbnailerThreadsSlider->setMinimumSize(QSize(180, 25));
        thumbnailerThreadsSlider->setMinimum(Settings::MinThumbnailerThreads);
        thumbnailerThreadsSlider->setMaximum(Settings::MaxThumbnailerThreads);
        thumbnailerThreadsSlider->setPageStep(1);
        thumbnailerThreadsSlider->setOrientation(Qt::Orientation::Horizontal);
        thumbnailerThreadsSlider->setTickPosition(QSlider::TickPosition::TicksBelow);

        horizontalLayout->addWidget(thumbnailerThreadsSlider);

        thumbnailerThreadsLabel = new QLabel(advancedGroup);
        thumbnailerThreadsLabel->setObjectName("thumbnailerThreadsLabel");

        horizontalLayout->addWidget(thumbnailerThreadsLabel);

        horizontalSpacer_9 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_9);


        verticalLayout_34->addLayout(horizontalLayout);

        useThumbnailCacheCheckBox = new QCheckBox(advancedGroup);
        useThumbnailCacheCheckBox->setObjectName("useThumbnailCacheCheckBox");
        sizePolicy9.setHeightForWidth(useThumbnailCacheCheckBox->sizePolicy().hasHeightForWidth());
        useThumbnailCacheCheckBox->setSizePolicy(sizePolicy9);

        verticalLayout_34->addWidget(useThumbnailCacheCheckBox);

        horizontalLayout_thumbRes = new QHBoxLayout();
        horizontalLayout_thumbRes->setObjectName("horizontalLayout_thumbRes");
        horizontalLayout_thumbRes->setContentsMargins(0, 0, 0, 0);
        thumbnailResolutionLabel = new QLabel(advancedGroup);
        thumbnailResolutionLabel->setObjectName("thumbnailResolutionLabel");

        horizontalLayout_thumbRes->addWidget(thumbnailResolutionLabel);

        thumbnailResolutionSlider = new QSlider(advancedGroup);
        thumbnailResolutionSlider->setObjectName("thumbnailResolutionSlider");
        sizePolicy2.setHeightForWidth(thumbnailResolutionSlider->sizePolicy().hasHeightForWidth());
        thumbnailResolutionSlider->setSizePolicy(sizePolicy2);
        thumbnailResolutionSlider->setMinimumSize(QSize(180, 25));
        thumbnailResolutionSlider->setMinimum(128);
        thumbnailResolutionSlider->setMaximum(512);
        thumbnailResolutionSlider->setSingleStep(16);
        thumbnailResolutionSlider->setPageStep(16);
        thumbnailResolutionSlider->setOrientation(Qt::Orientation::Horizontal);
        thumbnailResolutionSlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        thumbnailResolutionSlider->setTickInterval(64);

        horizontalLayout_thumbRes->addWidget(thumbnailResolutionSlider);

        thumbnailResolutionValueLabel = new QLabel(advancedGroup);
        thumbnailResolutionValueLabel->setObjectName("thumbnailResolutionValueLabel");

        horizontalLayout_thumbRes->addWidget(thumbnailResolutionValueLabel);

        horizontalSpacer_thumbRes = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_thumbRes->addItem(horizontalSpacer_thumbRes);


        verticalLayout_34->addLayout(horizontalLayout_thumbRes);

        horizontalLayout_thumbCacheQuota = new QHBoxLayout();
        horizontalLayout_thumbCacheQuota->setObjectName("horizontalLayout_thumbCacheQuota");
        horizontalLayout_thumbCacheQuota->setContentsMargins(0, 0, 0, 0);
        thumbnailCacheQuotaLabel = new QLabel(advancedGroup);
        thumbnailCacheQuotaLabel->setObjectName("thumbnailCacheQuotaLabel");

        horizontalLayout_thumbCacheQuota->addWidget(thumbnailCacheQuotaLabel);

        thumbnailCacheQuotaSpinBox = new QSpinBox(advancedGroup);
        thumbnailCacheQuotaSpinBox->setObjectName("thumbnailCacheQuotaSpinBox");
        QSizePolicy sizePolicy_thumbCacheQuota(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);
        sizePolicy_thumbCacheQuota.setHorizontalStretch(0);
        sizePolicy_thumbCacheQuota.setVerticalStretch(0);
        sizePolicy_thumbCacheQuota.setHeightForWidth(thumbnailCacheQuotaSpinBox->sizePolicy().hasHeightForWidth());
        thumbnailCacheQuotaSpinBox->setSizePolicy(sizePolicy_thumbCacheQuota);
        thumbnailCacheQuotaSpinBox->setMinimumSize(QSize(110, 24));
        thumbnailCacheQuotaSpinBox->setMinimum(0);
        thumbnailCacheQuotaSpinBox->setMaximum(102400);
        thumbnailCacheQuotaSpinBox->setSingleStep(128);
        thumbnailCacheQuotaSpinBox->setValue(512);
        thumbnailCacheQuotaSpinBox->setSuffix(" MB");
        thumbnailCacheQuotaSpinBox->setSpecialValueText(QCoreApplication::translate("SettingsDialog", "Unlimited", nullptr));

        horizontalLayout_thumbCacheQuota->addWidget(thumbnailCacheQuotaSpinBox);

        horizontalSpacer_thumbCacheQuota = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_thumbCacheQuota->addItem(horizontalSpacer_thumbCacheQuota);


        verticalLayout_34->addLayout(horizontalLayout_thumbCacheQuota);

        horizontalLayout_thumbCacheSize = new QHBoxLayout();
        horizontalLayout_thumbCacheSize->setObjectName("horizontalLayout_thumbCacheSize");
        horizontalLayout_thumbCacheSize->setContentsMargins(0, 0, 0, 0);
        thumbnailCacheSizeLabel = new QLabel(advancedGroup);
        thumbnailCacheSizeLabel->setObjectName("thumbnailCacheSizeLabel");

        horizontalLayout_thumbCacheSize->addWidget(thumbnailCacheSizeLabel);

        thumbnailCacheSizeValueLabel = new QLabel(advancedGroup);
        thumbnailCacheSizeValueLabel->setObjectName("thumbnailCacheSizeValueLabel");

        horizontalLayout_thumbCacheSize->addWidget(thumbnailCacheSizeValueLabel);

        clearThumbnailCacheButton = new QPushButton(advancedGroup);
        clearThumbnailCacheButton->setObjectName("clearThumbnailCacheButton");

        horizontalLayout_thumbCacheSize->addWidget(clearThumbnailCacheButton);

        horizontalSpacer_thumbCacheSize = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_thumbCacheSize->addItem(horizontalSpacer_thumbCacheSize);


        verticalLayout_34->addLayout(horizontalLayout_thumbCacheSize);

        labelExcludedCachePaths = new QLabel(advancedGroup);
        labelExcludedCachePaths->setObjectName("labelExcludedCachePaths");

        verticalLayout_34->addWidget(labelExcludedCachePaths);

        excludedCachePathsLineEdit = new QLineEdit(advancedGroup);
        excludedCachePathsLineEdit->setObjectName("excludedCachePathsLineEdit");

        verticalLayout_34->addWidget(excludedCachePathsLineEdit);

        unloadThumbsCheckBox = new QCheckBox(advancedGroup);
        unloadThumbsCheckBox->setObjectName("unloadThumbsCheckBox");

        verticalLayout_34->addWidget(unloadThumbsCheckBox);

        label_51 = new QLabel(advancedGroup);
        label_51->setObjectName("label_51");
#if QT_CONFIG(accessibility)
        label_51->setAccessibleName(QString::fromUtf8("SNoteText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(label_51);

        widget_26 = new QWidget(advancedGroup);
        widget_26->setObjectName("widget_26");
#if QT_CONFIG(accessibility)
        widget_26->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(widget_26);

        saveOverlayCheckBox = new QCheckBox(advancedGroup);
        saveOverlayCheckBox->setObjectName("saveOverlayCheckBox");

        verticalLayout_34->addWidget(saveOverlayCheckBox);

        horizontalLayout_10 = new QHBoxLayout();
        horizontalLayout_10->setObjectName("horizontalLayout_10");
        horizontalLayout_10->setContentsMargins(0, 0, -1, -1);
        label_3 = new QLabel(advancedGroup);
        label_3->setObjectName("label_3");

        horizontalLayout_10->addWidget(label_3);

        JPEGQualitySlider = new QSlider(advancedGroup);
        JPEGQualitySlider->setObjectName("JPEGQualitySlider");
        sizePolicy2.setHeightForWidth(JPEGQualitySlider->sizePolicy().hasHeightForWidth());
        JPEGQualitySlider->setSizePolicy(sizePolicy2);
        JPEGQualitySlider->setMinimumSize(QSize(180, 25));
        JPEGQualitySlider->setMinimum(0);
        JPEGQualitySlider->setMaximum(100);
        JPEGQualitySlider->setPageStep(5);
        JPEGQualitySlider->setValue(95);
        JPEGQualitySlider->setOrientation(Qt::Orientation::Horizontal);
        JPEGQualitySlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        JPEGQualitySlider->setTickInterval(10);

        horizontalLayout_10->addWidget(JPEGQualitySlider);

        JPEGQualityLabel = new QLabel(advancedGroup);
        JPEGQualityLabel->setObjectName("JPEGQualityLabel");

        horizontalLayout_10->addWidget(JPEGQualityLabel);

        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_10->addItem(horizontalSpacer_7);


        verticalLayout_34->addLayout(horizontalLayout_10);

        widget_25 = new QWidget(advancedGroup);
        widget_25->setObjectName("widget_25");
#if QT_CONFIG(accessibility)
        widget_25->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(widget_25);

        confirmTrashCheckBox = new QCheckBox(advancedGroup);
        confirmTrashCheckBox->setObjectName("confirmTrashCheckBox");

        verticalLayout_34->addWidget(confirmTrashCheckBox);

        confirmDeleteCheckBox = new QCheckBox(advancedGroup);
        confirmDeleteCheckBox->setObjectName("confirmDeleteCheckBox");
        confirmDeleteCheckBox->setFont(font1);

        verticalLayout_34->addWidget(confirmDeleteCheckBox);

        widget_multi_instance_sep = new QWidget(advancedGroup);
        widget_multi_instance_sep->setObjectName("widget_multi_instance_sep");
#if QT_CONFIG(accessibility)
        widget_multi_instance_sep->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(widget_multi_instance_sep);

        multiInstanceCheckBox = new QCheckBox(advancedGroup);
        multiInstanceCheckBox->setObjectName("multiInstanceCheckBox");

        verticalLayout_34->addWidget(multiInstanceCheckBox);

        widget_28 = new QWidget(advancedGroup);
        widget_28->setObjectName("widget_28");
#if QT_CONFIG(accessibility)
        widget_28->setAccessibleName(QString::fromUtf8("SLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_34->addWidget(widget_28);

        horizontalLayout_25 = new QHBoxLayout();
        horizontalLayout_25->setObjectName("horizontalLayout_25");
        horizontalLayout_25->setContentsMargins(0, 0, 0, 0);
        memoryLimitLabel = new QLabel(advancedGroup);
        memoryLimitLabel->setObjectName("memoryLimitLabel");

        horizontalLayout_25->addWidget(memoryLimitLabel);

        memoryLimitSpinBox = new QSpinBox(advancedGroup);
        memoryLimitSpinBox->setObjectName("memoryLimitSpinBox");
        QSizePolicy sizePolicy11(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);
        sizePolicy11.setHorizontalStretch(0);
        sizePolicy11.setVerticalStretch(0);
        sizePolicy11.setHeightForWidth(memoryLimitSpinBox->sizePolicy().hasHeightForWidth());
        memoryLimitSpinBox->setSizePolicy(sizePolicy11);
        memoryLimitSpinBox->setMinimumSize(QSize(110, 24));
        memoryLimitSpinBox->setMinimum(512);
        memoryLimitSpinBox->setMaximum(8192);
        memoryLimitSpinBox->setSingleStep(512);
        memoryLimitSpinBox->setValue(1024);

        horizontalLayout_25->addWidget(memoryLimitSpinBox);

        horizontalSpacer_32 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_25->addItem(horizontalSpacer_32);


        verticalLayout_34->addLayout(horizontalLayout_25);


        verticalLayout_31->addWidget(advancedGroup);

        verticalSpacer_16 = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_31->addItem(verticalSpacer_16);

        scrollArea_4->setWidget(scrollAreaWidgetContents_4);

        verticalLayout_30->addWidget(scrollArea_4);

        stackedWidget->addWidget(Advanced);
        AIUpscale = new QWidget();
        AIUpscale->setObjectName("AIUpscale");
        verticalLayout_aiUpscale = new QVBoxLayout(AIUpscale);
        verticalLayout_aiUpscale->setObjectName("verticalLayout_aiUpscale");
        verticalLayout_aiUpscale->setContentsMargins(0, 0, 0, 0);
        scrollArea_aiUpscale = new QScrollArea(AIUpscale);
        scrollArea_aiUpscale->setObjectName("scrollArea_aiUpscale");
        scrollArea_aiUpscale->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea_aiUpscale->setWidgetResizable(true);
        scrollAreaWidgetContents_aiUpscale = new QWidget();
        scrollAreaWidgetContents_aiUpscale->setObjectName("scrollAreaWidgetContents_aiUpscale");
        scrollAreaWidgetContents_aiUpscale->setGeometry(QRect(0, 0, 609, 699));
        verticalLayout_aiUpscaleContents = new QVBoxLayout(scrollAreaWidgetContents_aiUpscale);
        verticalLayout_aiUpscaleContents->setSpacing(9);
        verticalLayout_aiUpscaleContents->setObjectName("verticalLayout_aiUpscaleContents");
        verticalLayout_aiUpscaleContents->setContentsMargins(18, 9, 18, 9);
        label_aiUpscaleHeader = new QLabel(scrollAreaWidgetContents_aiUpscale);
        label_aiUpscaleHeader->setObjectName("label_aiUpscaleHeader");
#if QT_CONFIG(accessibility)
        label_aiUpscaleHeader->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_aiUpscaleContents->addWidget(label_aiUpscaleHeader);

        widget_aiUpscaleHeaderLine = new QWidget(scrollAreaWidgetContents_aiUpscale);
        widget_aiUpscaleHeaderLine->setObjectName("widget_aiUpscaleHeaderLine");
#if QT_CONFIG(accessibility)
        widget_aiUpscaleHeaderLine->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_aiUpscaleContents->addWidget(widget_aiUpscaleHeaderLine);

        verticalSpacer_aiUpscaleTop = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_aiUpscaleContents->addItem(verticalSpacer_aiUpscaleTop);

        aiUpscaleGroup = new QWidget(scrollAreaWidgetContents_aiUpscale);
        aiUpscaleGroup->setObjectName("aiUpscaleGroup");
#if QT_CONFIG(accessibility)
        aiUpscaleGroup->setAccessibleName(QString::fromUtf8("SGroup"));
#endif // QT_CONFIG(accessibility)
        verticalLayout_aiUpscaleGroup = new QVBoxLayout(aiUpscaleGroup);
        verticalLayout_aiUpscaleGroup->setSpacing(7);
        verticalLayout_aiUpscaleGroup->setObjectName("verticalLayout_aiUpscaleGroup");
        verticalLayout_aiUpscaleGroup->setContentsMargins(13, 10, 13, 10);
        useUpscaylCheckBox = new QCheckBox(aiUpscaleGroup);
        useUpscaylCheckBox->setObjectName("useUpscaylCheckBox");
        sizePolicy9.setHeightForWidth(useUpscaylCheckBox->sizePolicy().hasHeightForWidth());
        useUpscaylCheckBox->setSizePolicy(sizePolicy9);

        verticalLayout_aiUpscaleGroup->addWidget(useUpscaylCheckBox);

        horizontalLayout_upscaylModel = new QHBoxLayout();
        horizontalLayout_upscaylModel->setObjectName("horizontalLayout_upscaylModel");
        horizontalLayout_upscaylModel->setContentsMargins(0, 0, 0, 0);
        label_upscaylModel = new QLabel(aiUpscaleGroup);
        label_upscaylModel->setObjectName("label_upscaylModel");

        horizontalLayout_upscaylModel->addWidget(label_upscaylModel);

        upscaylModelComboBox = new QComboBox(aiUpscaleGroup);
        upscaylModelComboBox->setObjectName("upscaylModelComboBox");
        sizePolicy2.setHeightForWidth(upscaylModelComboBox->sizePolicy().hasHeightForWidth());
        upscaylModelComboBox->setSizePolicy(sizePolicy2);
        upscaylModelComboBox->setMinimumSize(QSize(180, 24));

        horizontalLayout_upscaylModel->addWidget(upscaylModelComboBox);

        label_upscaylGetModels = new QLabel(aiUpscaleGroup);
        label_upscaylGetModels->setObjectName("label_upscaylGetModels");
        label_upscaylGetModels->setOpenExternalLinks(true);

        horizontalLayout_upscaylModel->addWidget(label_upscaylGetModels);

        horizontalSpacer_upscaylModel = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_upscaylModel->addItem(horizontalSpacer_upscaylModel);


        verticalLayout_aiUpscaleGroup->addLayout(horizontalLayout_upscaylModel);

        preloadUpscaylCheckBox = new QCheckBox(aiUpscaleGroup);
        preloadUpscaylCheckBox->setObjectName("preloadUpscaylCheckBox");
        sizePolicy9.setHeightForWidth(preloadUpscaylCheckBox->sizePolicy().hasHeightForWidth());
        preloadUpscaylCheckBox->setSizePolicy(sizePolicy9);

        verticalLayout_aiUpscaleGroup->addWidget(preloadUpscaylCheckBox);

        upscaylLimitCheckBox = new QCheckBox(aiUpscaleGroup);
        upscaylLimitCheckBox->setObjectName("upscaylLimitCheckBox");
        sizePolicy9.setHeightForWidth(upscaylLimitCheckBox->sizePolicy().hasHeightForWidth());
        upscaylLimitCheckBox->setSizePolicy(sizePolicy9);

        verticalLayout_aiUpscaleGroup->addWidget(upscaylLimitCheckBox);

        horizontalLayout_upscaylLimit = new QHBoxLayout();
        horizontalLayout_upscaylLimit->setObjectName("horizontalLayout_upscaylLimit");
        horizontalLayout_upscaylLimit->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_upscaylLimitIndent = new QSpacerItem(20, 20, QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Minimum);

        horizontalLayout_upscaylLimit->addItem(horizontalSpacer_upscaylLimitIndent);

        upscaylLimitSlider = new QSlider(aiUpscaleGroup);
        upscaylLimitSlider->setObjectName("upscaylLimitSlider");
        sizePolicy.setHeightForWidth(upscaylLimitSlider->sizePolicy().hasHeightForWidth());
        upscaylLimitSlider->setSizePolicy(sizePolicy);
        upscaylLimitSlider->setMinimumSize(QSize(190, 10));
        upscaylLimitSlider->setMinimum(100);
        upscaylLimitSlider->setMaximum(400);
        upscaylLimitSlider->setSingleStep(5);
        upscaylLimitSlider->setPageStep(25);
        upscaylLimitSlider->setValue(200);
        upscaylLimitSlider->setOrientation(Qt::Orientation::Horizontal);
        upscaylLimitSlider->setTickPosition(QSlider::TickPosition::TicksBelow);
        upscaylLimitSlider->setTickInterval(25);

        horizontalLayout_upscaylLimit->addWidget(upscaylLimitSlider);

        upscaylLimitValueLabel = new QLabel(aiUpscaleGroup);
        upscaylLimitValueLabel->setObjectName("upscaylLimitValueLabel");
        sizePolicy5.setHeightForWidth(upscaylLimitValueLabel->sizePolicy().hasHeightForWidth());
        upscaylLimitValueLabel->setSizePolicy(sizePolicy5);
        upscaylLimitValueLabel->setMinimumSize(QSize(60, 0));
        upscaylLimitValueLabel->setMaximumSize(QSize(60, 16777215));
        upscaylLimitValueLabel->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignVCenter);

        horizontalLayout_upscaylLimit->addWidget(upscaylLimitValueLabel);

        horizontalSpacer_upscaylLimitSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_upscaylLimit->addItem(horizontalSpacer_upscaylLimitSpacer);


        verticalLayout_aiUpscaleGroup->addLayout(horizontalLayout_upscaylLimit);


        verticalLayout_aiUpscaleContents->addWidget(aiUpscaleGroup);

        verticalSpacer_aiUpscaleBottom = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout_aiUpscaleContents->addItem(verticalSpacer_aiUpscaleBottom);

        scrollArea_aiUpscale->setWidget(scrollAreaWidgetContents_aiUpscale);

        verticalLayout_aiUpscale->addWidget(scrollArea_aiUpscale);

        stackedWidget->addWidget(AIUpscale);
        About = new QWidget();
        About->setObjectName("About");
        verticalLayout_2 = new QVBoxLayout(About);
        verticalLayout_2->setObjectName("verticalLayout_2");
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        scrollArea_5 = new QScrollArea(About);
        scrollArea_5->setObjectName("scrollArea_5");
        scrollArea_5->setFrameShape(QFrame::Shape::NoFrame);
        scrollArea_5->setWidgetResizable(true);
        scrollAreaWidgetContents_5 = new QWidget();
        scrollAreaWidgetContents_5->setObjectName("scrollAreaWidgetContents_5");
        scrollAreaWidgetContents_5->setGeometry(QRect(0, 0, 609, 699));
        verticalLayout_32 = new QVBoxLayout(scrollAreaWidgetContents_5);
        verticalLayout_32->setSpacing(9);
        verticalLayout_32->setObjectName("verticalLayout_32");
        verticalLayout_32->setContentsMargins(18, 9, 18, 9);
        label_53 = new QLabel(scrollAreaWidgetContents_5);
        label_53->setObjectName("label_53");
#if QT_CONFIG(accessibility)
        label_53->setAccessibleName(QString::fromUtf8("SHeaderText"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_32->addWidget(label_53);

        widget_16 = new QWidget(scrollAreaWidgetContents_5);
        widget_16->setObjectName("widget_16");
        sizePolicy6.setHeightForWidth(widget_16->sizePolicy().hasHeightForWidth());
        widget_16->setSizePolicy(sizePolicy6);
#if QT_CONFIG(accessibility)
        widget_16->setAccessibleName(QString::fromUtf8("SHeaderLine"));
#endif // QT_CONFIG(accessibility)

        verticalLayout_32->addWidget(widget_16);

        verticalSpacer_6 = new QSpacerItem(20, 6, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Fixed);

        verticalLayout_32->addItem(verticalSpacer_6);

        aboutAppTextBrowser = new QTextBrowser(scrollAreaWidgetContents_5);
        aboutAppTextBrowser->setObjectName("aboutAppTextBrowser");
        QSizePolicy sizePolicy12(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::MinimumExpanding);
        sizePolicy12.setHorizontalStretch(0);
        sizePolicy12.setVerticalStretch(0);
        sizePolicy12.setHeightForWidth(aboutAppTextBrowser->sizePolicy().hasHeightForWidth());
        aboutAppTextBrowser->setSizePolicy(sizePolicy12);
        aboutAppTextBrowser->setFrameShape(QFrame::Shape::NoFrame);
        aboutAppTextBrowser->setFrameShadow(QFrame::Shadow::Plain);
        aboutAppTextBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
        aboutAppTextBrowser->setOpenExternalLinks(true);

        verticalLayout_32->addWidget(aboutAppTextBrowser);

        scrollArea_5->setWidget(scrollAreaWidgetContents_5);

        verticalLayout_2->addWidget(scrollArea_5);

        stackedWidget->addWidget(About);

        verticalLayout_6->addWidget(stackedWidget);

        settingsBottomWidget = new QWidget(this);
        settingsBottomWidget->setObjectName("settingsBottomWidget");
        settingsBottomWidget->setEnabled(true);
        horizontalLayout_15 = new QHBoxLayout(settingsBottomWidget);
        horizontalLayout_15->setObjectName("horizontalLayout_15");
        horizontalLayout_15->setContentsMargins(8, 4, 4, 4);
        versionLabelWidget = new QWidget(settingsBottomWidget);
        versionLabelWidget->setObjectName("versionLabelWidget");
        sizePolicy.setHeightForWidth(versionLabelWidget->sizePolicy().hasHeightForWidth());
        versionLabelWidget->setSizePolicy(sizePolicy);
        verticalLayout_22 = new QVBoxLayout(versionLabelWidget);
        verticalLayout_22->setObjectName("verticalLayout_22");
        verticalLayout_22->setContentsMargins(0, 0, 0, 0);

        horizontalLayout_15->addWidget(versionLabelWidget);

        horizontalSpacer_3 = new QSpacerItem(226, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_15->addItem(horizontalSpacer_3);

        OK = new QPushButton(settingsBottomWidget);
        OK->setObjectName("OK");

        horizontalLayout_15->addWidget(OK);

        pushButton = new QPushButton(settingsBottomWidget);
        pushButton->setObjectName("pushButton");

        horizontalLayout_15->addWidget(pushButton);

        Cancel = new QPushButton(settingsBottomWidget);
        Cancel->setObjectName("Cancel");

        horizontalLayout_15->addWidget(Cancel);


        verticalLayout_6->addWidget(settingsBottomWidget);


        horizontalLayout_22->addLayout(verticalLayout_6);

        horizontalLayout_22->setStretch(1, 99);

        verticalLayout_3->addLayout(horizontalLayout_22);


        retranslateUi();
        QObject::connect(Cancel, &QPushButton::clicked, this, qOverload<>(&QWidget::close));
        QObject::connect(bgOpacitySlider, &QSlider::valueChanged, this, &SettingsDialog::onBgOpacitySliderChanged);
        QObject::connect(OK, &QPushButton::clicked, this, &SettingsDialog::saveSettingsAndClose);
        QObject::connect(pushButton, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
        QObject::connect(JPEGQualitySlider, &QSlider::valueChanged, this, &SettingsDialog::onJPEGQualitySliderChanged);
        QObject::connect(pushButton_8, &QPushButton::clicked, this, qOverload<>(&SettingsDialog::editShortcut));
        QObject::connect(expandLimitSlider, &QSlider::valueChanged, this, &SettingsDialog::onExpandLimitSliderChanged);
        QObject::connect(pushButton_9, &QPushButton::clicked, this, &SettingsDialog::addScript);
        QObject::connect(resetZoomLevelsButton, &QPushButton::clicked, this, &SettingsDialog::resetZoomLevels);
        QObject::connect(clearThumbnailCacheButton, &QPushButton::clicked, this, &SettingsDialog::onClearThumbnailCacheClicked);
        QObject::connect(autoResizeLimitSlider, &QSlider::valueChanged, this, &SettingsDialog::onAutoResizeLimitSliderChanged);
        QObject::connect(pushButton_3, &QPushButton::clicked, this, &SettingsDialog::resetShortcuts);
        QObject::connect(zoomStepSlider, &QSlider::valueChanged, this, &SettingsDialog::onZoomStepSliderChanged);
        QObject::connect(useFixedZoomLevelsCheckBox, &QCheckBox::toggled, widget, &QWidget::setEnabled);
        QObject::connect(thumbnailerThreadsSlider, &QSlider::valueChanged, this, &SettingsDialog::onThumbnailerThreadsSliderChanged);
        QObject::connect(pushButton_2, &QPushButton::clicked, this, &SettingsDialog::addShortcut);
        QObject::connect(pushButton_4, &QPushButton::clicked, this, &SettingsDialog::removeShortcut);
        QObject::connect(shortcutsTableView, &QTableView::doubleClicked, this, [this](const QModelIndex &index) { editShortcut(index.row()); });
        QObject::connect(enablePanelCheckBox, &QCheckBox::toggled, thumbnailPanelGroupContents, &QWidget::setEnabled);
        QObject::connect(pushButton_6, &QPushButton::clicked, this, qOverload<>(&SettingsDialog::editScript));
        QObject::connect(scriptsListView, &QListView::doubleClicked, this, qOverload<const QModelIndex &>(&SettingsDialog::editScript));
        QObject::connect(pushButton_7, &QPushButton::clicked, this, &SettingsDialog::removeScript);
        QObject::connect(expandImageCheckBox, &QCheckBox::toggled, expandImagesGroupContents, &QWidget::setEnabled);
        QObject::connect(sideBar2, &SSideBar::entrySelected, stackedWidget, &QStackedWidget::setCurrentIndex);
        QObject::connect(clickableEdgesCheckBox, &QCheckBox::clicked, clickableEdgesVisibleCheckBox, &QCheckBox::setEnabled);
        QObject::connect(mouseScrollingSpeedSlider, &QSlider::valueChanged, this, &SettingsDialog::onMouseScrollingSpeedSliderChanged);

        stackedWidget->setCurrentIndex(0);
        scalingQualityComboBox->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(this);
}

void SettingsDialog::retranslateUi() {
        this->setWindowTitle(QCoreApplication::translate("SettingsDialog", "Preferences", nullptr));
#if QT_CONFIG(accessibility)
        this->setAccessibleName(QCoreApplication::translate("SettingsDialog", "SettingsDialog", nullptr));
#endif // QT_CONFIG(accessibility)
        appIconLabel->setText(QString());
        versionLabel->setText(QString());
        qtIconLabel->setText(QString());
        qtVersionLabel->setText(QString());
        label_20->setText(QCoreApplication::translate("SettingsDialog", "General", nullptr));
        label_46->setText(QCoreApplication::translate("SettingsDialog", "Language:", nullptr));
        label_48->setText(QCoreApplication::translate("SettingsDialog", "Requires application restart", nullptr));
        fullscreenCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Open in fullscreen", nullptr));
        startInFolderViewCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Start in folder view by default", nullptr));
        standbyCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Enable standby mode on close", nullptr));
#if QT_CONFIG(tooltip)
        standbyCheckBox->setToolTip(QCoreApplication::translate("SettingsDialog", "Keeps the application running in the background when closed. Subsequent launches will be instant.", nullptr));
#endif // QT_CONFIG(tooltip)
        rememberLastFolderCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Remember last opened folder", nullptr));
        label_15->setText(QCoreApplication::translate("SettingsDialog", "User interface", nullptr));
        showExtendedInfoTitle->setText(QCoreApplication::translate("SettingsDialog", "Image info in window title", nullptr));
        showInfoBarFullscreen->setText(QCoreApplication::translate("SettingsDialog", "Fullscreen info bar", nullptr));
        cursorAutohideCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Auto-hide cursor", nullptr));
#if QT_CONFIG(tooltip)
        enableSmoothScrollCheckBox->setToolTip(QCoreApplication::translate("SettingsDialog", "Turn this off if you are using a touchpad with libinput driver.", nullptr));
#endif // QT_CONFIG(tooltip)
        enableSmoothScrollCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Smooth thumbnail scrolling", nullptr));
        enableSmoothZoomCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Smooth zooming", nullptr));
        label->setText(QCoreApplication::translate("SettingsDialog", "Zoom indicator:", nullptr));
        zoomIndicatorOn->setText(QCoreApplication::translate("SettingsDialog", "On", nullptr));
        zoomIndicatorOff->setText(QCoreApplication::translate("SettingsDialog", "Off", nullptr));
        zoomIndicatorAuto->setText(QCoreApplication::translate("SettingsDialog", "Auto", nullptr));
        autoResizeWindowCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Automatic window resize", nullptr));
        label_50->setText(QCoreApplication::translate("SettingsDialog", "Match displayed content", nullptr));
        label_39->setText(QCoreApplication::translate("SettingsDialog", "Screen area limit for auto resize:", nullptr));
        autoResizeLimit->setText(QCoreApplication::translate("SettingsDialog", "xx", nullptr));
#if QT_CONFIG(tooltip)
        enablePanelCheckBox->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        enablePanelCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Thumbnail panel", nullptr));
        squareThumbnailsCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Crop previews", nullptr));
        pinPanelCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Pinned", nullptr));
        panelFullscreenOnlyCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Disable in windowed mode", nullptr));
        panelCenterSelectionCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Center selected image", nullptr));
        showSubfoldersInPanelCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Show subfolders", nullptr));
        panelHideDelayLabel->setText(QCoreApplication::translate("SettingsDialog", "Hide delay:", nullptr));
        thumbStyleExtended->setText(QCoreApplication::translate("SettingsDialog", "Extended", nullptr));
        label_8->setText(QCoreApplication::translate("SettingsDialog", "Previews only", nullptr));
        label_18->setText(QCoreApplication::translate("SettingsDialog", "Display style:", nullptr));
        label_25->setText(QCoreApplication::translate("SettingsDialog", "Show filename and resolution", nullptr));
        thumbStyleSimple->setText(QCoreApplication::translate("SettingsDialog", "Simple", nullptr));
        label_4->setText(QCoreApplication::translate("SettingsDialog", "Preview size:", nullptr));
        panelPositionLabel->setText(QCoreApplication::translate("SettingsDialog", "Position:", nullptr));
        panelPositionComboBox->setItemText(0, QCoreApplication::translate("SettingsDialog", "Top", nullptr));
        panelPositionComboBox->setItemText(1, QCoreApplication::translate("SettingsDialog", "Bottom", nullptr));
        panelPositionComboBox->setItemText(2, QCoreApplication::translate("SettingsDialog", "Left", nullptr));
        panelPositionComboBox->setItemText(3, QCoreApplication::translate("SettingsDialog", "Right", nullptr));

        label_30->setText(QCoreApplication::translate("SettingsDialog", "Folder navigation", nullptr));
        folderEndNoAction->setText(QCoreApplication::translate("SettingsDialog", "Stop", nullptr));
        folderEndLoop->setText(QCoreApplication::translate("SettingsDialog", "Loop folder", nullptr));
        folderEndSwitchFolder->setText(QCoreApplication::translate("SettingsDialog", "Go to the next folder", nullptr));
        label_16->setText(QCoreApplication::translate("SettingsDialog", "After reaching the end:", nullptr));
        label_6->setText(QCoreApplication::translate("SettingsDialog", "Default sorting mode:", nullptr));
        sortingComboBox->setItemText(0, QCoreApplication::translate("SettingsDialog", "A - Z", nullptr));
        sortingComboBox->setItemText(1, QCoreApplication::translate("SettingsDialog", "Z - A", nullptr));
        sortingComboBox->setItemText(2, QCoreApplication::translate("SettingsDialog", "Size", nullptr));
        sortingComboBox->setItemText(3, QCoreApplication::translate("SettingsDialog", "Size (desc)", nullptr));
        sortingComboBox->setItemText(4, QCoreApplication::translate("SettingsDialog", "Oldest", nullptr));
        sortingComboBox->setItemText(5, QCoreApplication::translate("SettingsDialog", "Newest", nullptr));

        sortFoldersCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Apply sorting to folders", nullptr));
        showHiddenFilesCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Show hidden files", nullptr));
        label_42->setText(QCoreApplication::translate("SettingsDialog", "Slideshow", nullptr));
        label_27->setText(QCoreApplication::translate("SettingsDialog", "Switch interval:", nullptr));
        slideshowIntervalSpinBox->setSuffix(QCoreApplication::translate("SettingsDialog", "ms", nullptr));
        loopSlideshowCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Loop slideshow", nullptr));
        label_9->setText(QCoreApplication::translate("SettingsDialog", "View", nullptr));
        label_40->setText(QCoreApplication::translate("SettingsDialog", "Display options", nullptr));
        label_2->setText(QCoreApplication::translate("SettingsDialog", "Image fit:", nullptr));
        fitModeWindow->setText(QCoreApplication::translate("SettingsDialog", "Fit to window", nullptr));
        fitModeWidth->setText(QCoreApplication::translate("SettingsDialog", "Fit to width", nullptr));
        fitMode1to1->setText(QCoreApplication::translate("SettingsDialog", "1:1", nullptr));
        fitModeHeight->setText(QCoreApplication::translate("SettingsDialog", "Fit to height", nullptr));
        keepFitModeCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Keep fit mode selected via hotkey when switching images", nullptr));
        label_26->setText(QCoreApplication::translate("SettingsDialog", "Focus in 1:1 mode:", nullptr));
        focus1to1Top->setText(QCoreApplication::translate("SettingsDialog", "Top", nullptr));
        focus1to1Center->setText(QCoreApplication::translate("SettingsDialog", "Center", nullptr));
        focus1to1Cursor->setText(QCoreApplication::translate("SettingsDialog", "At cursor", nullptr));
        label_10->setText(QCoreApplication::translate("SettingsDialog", "Part of image that's focused after switching to 1:1", nullptr));
        transparencyGridCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Grid background on images with transparency", nullptr));
        expandImageCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Expand images, up to:", nullptr));
        expandLimitLabel->setText(QCoreApplication::translate("SettingsDialog", "xx", nullptr));
        label_13->setText(QCoreApplication::translate("SettingsDialog", "Images smaller than window will be zoomed in", nullptr));
        label_19->setText(QCoreApplication::translate("SettingsDialog", "Zoom options", nullptr));
        unlockMinZoomCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Unlock minimum zoom", nullptr));
        label_44->setText(QCoreApplication::translate("SettingsDialog", "Always allow zooming below 100%", nullptr));
        label_12->setText(QCoreApplication::translate("SettingsDialog", "Zoom step:", nullptr));
        zoomStepLabel->setText(QCoreApplication::translate("SettingsDialog", "[step]", nullptr));
        useFixedZoomLevelsCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Use fixed zoom levels:", nullptr));
        resetZoomLevelsButton->setText(QCoreApplication::translate("SettingsDialog", "Load defaults", nullptr));
        title3->setText(QCoreApplication::translate("SettingsDialog", "Scaling quality", nullptr));
#if QT_CONFIG(tooltip)
        scalingQualityLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        scalingQualityLabel->setText(QCoreApplication::translate("SettingsDialog", "Scaling filter:", nullptr));
        scalingQualityComboBox->setItemText(0, QCoreApplication::translate("SettingsDialog", "Nearest neighbor", nullptr));
        scalingQualityComboBox->setItemText(1, QCoreApplication::translate("SettingsDialog", "Bilinear", nullptr));

#if QT_CONFIG(tooltip)
        scalingQualityComboBox->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        label_45->setText(QCoreApplication::translate("SettingsDialog", "Theme", nullptr));
        loadPresetLabel->setText(QCoreApplication::translate("SettingsDialog", "Load preset:", nullptr));
        themeSelectorComboBox->setItemText(0, QCoreApplication::translate("SettingsDialog", "Black", nullptr));
        themeSelectorComboBox->setItemText(1, QCoreApplication::translate("SettingsDialog", "Dark", nullptr));
        themeSelectorComboBox->setItemText(2, QCoreApplication::translate("SettingsDialog", "Dark Blue", nullptr));
        themeSelectorComboBox->setItemText(3, QCoreApplication::translate("SettingsDialog", "Light", nullptr));

        themeSelectorComboBox->setCurrentText(QCoreApplication::translate("SettingsDialog", "Black", nullptr));
        useSystemColorsCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Use system colors", nullptr));
        modifySystemSchemeLabel->setText(QCoreApplication::translate("SettingsDialog", "<html><head/><body><p><span style=\"text-decoration: underline;\">modify</span></p></body></html>", nullptr));
        colorSelectorAccent->setText(QString());
        colorSelectorBackground->setText(QString());
        label_33->setText(QCoreApplication::translate("SettingsDialog", "Accent", nullptr));
        label_34->setText(QCoreApplication::translate("SettingsDialog", "Background", nullptr));
        label_35->setText(QCoreApplication::translate("SettingsDialog", "Background (fullscreen mode)", nullptr));
        colorSelectorOverlayText->setText(QString());
        label_11->setText(QCoreApplication::translate("SettingsDialog", "Text", nullptr));
        label_14->setText(QCoreApplication::translate("SettingsDialog", "Icons", nullptr));
        colorSelectorFolderview->setText(QString());
        colorSelectorText->setText(QString());
        colorSelectorWidgetBorder->setText(QString());
        label_36->setText(QCoreApplication::translate("SettingsDialog", "Overlay background", nullptr));
        colorSelectorOverlay->setText(QString());
        label_21->setText(QCoreApplication::translate("SettingsDialog", "Widget background", nullptr));
        label_31->setText(QCoreApplication::translate("SettingsDialog", "Folder view top panel", nullptr));
        label_22->setText(QCoreApplication::translate("SettingsDialog", "Widget border", nullptr));
        colorSelectorScrollbar->setText(QString());
        colorSelectorFolderviewPanel->setText(QString());
        label_37->setText(QCoreApplication::translate("SettingsDialog", "Overlay text", nullptr));
        colorSelectorWidget->setText(QString());
        label_32->setText(QCoreApplication::translate("SettingsDialog", "Scrollbars", nullptr));
        colorSelectorThumbpanel->setText(QString());
        label_thumbpanel->setText(QCoreApplication::translate("SettingsDialog", "Thumbnail panel", nullptr));
        label_23->setText(QCoreApplication::translate("SettingsDialog", "Folder view background", nullptr));
        label_38->setText(QCoreApplication::translate("SettingsDialog", "Other window tweaks", nullptr));
        label_5->setText(QCoreApplication::translate("SettingsDialog", "Window opacity:", nullptr));
        bgOpacityPercentLabel->setText(QCoreApplication::translate("SettingsDialog", "%", nullptr));
        label_5_thumb->setText(QCoreApplication::translate("SettingsDialog", "Thumbnail bar opacity:", nullptr));
        thumbOpacityPercentLabel->setText(QCoreApplication::translate("SettingsDialog", "%", nullptr));
        useBlackBackgroundCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Use black for background and thumbnail bar", nullptr));
        label_29->setText(QCoreApplication::translate("SettingsDialog", "Controls", nullptr));
        pushButton_2->setText(QCoreApplication::translate("SettingsDialog", "Add", nullptr));
        pushButton_8->setText(QCoreApplication::translate("SettingsDialog", "Edit", nullptr));
        pushButton_4->setText(QCoreApplication::translate("SettingsDialog", "Remove", nullptr));
        pushButton_3->setText(QCoreApplication::translate("SettingsDialog", "Reset to defaults", nullptr));
        clickableEdgesCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Switch image by clicking window edges", nullptr));
        clickableEdgesVisibleCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Visible edges", nullptr));
        label_28->setText(QCoreApplication::translate("SettingsDialog", "Scroll image with:", nullptr));
        imageScrollingComboBox->setItemText(0, QCoreApplication::translate("SettingsDialog", "None", nullptr));
        imageScrollingComboBox->setItemText(1, QCoreApplication::translate("SettingsDialog", "Touchpad", nullptr));
        imageScrollingComboBox->setItemText(2, QCoreApplication::translate("SettingsDialog", "Touchpad & Mouse Wheel", nullptr));

        label_52->setText(QCoreApplication::translate("SettingsDialog", "Note: you can also zoom by holding RMB and moving the mouse", nullptr));
        label_54->setText(QCoreApplication::translate("SettingsDialog", "Mouse scrolling speed:", nullptr));
        mouseScrollingSpeedLabel->setText(QCoreApplication::translate("SettingsDialog", "[x]", nullptr));
        trackpadDetectionCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Trackpad detection", nullptr));
        label_7->setText(QCoreApplication::translate("SettingsDialog", "Disable if you have issues with mouse scrolling", nullptr));
        label_47->setText(QCoreApplication::translate("SettingsDialog", "Scripts", nullptr));
        label_24->setText(QCoreApplication::translate("SettingsDialog", "Note: these will appear in \"Open with\" menu.", nullptr));
        label_43->setText(QCoreApplication::translate("SettingsDialog", "Also, you can assign shortcuts to scripts (in \"Controls\" section).", nullptr));
        pushButton_9->setText(QCoreApplication::translate("SettingsDialog", "Add", nullptr));
        pushButton_6->setText(QCoreApplication::translate("SettingsDialog", "Edit", nullptr));
        pushButton_7->setText(QCoreApplication::translate("SettingsDialog", "Remove", nullptr));
        label_49->setText(QCoreApplication::translate("SettingsDialog", "Advanced", nullptr));
#if QT_CONFIG(tooltip)
        usePreloaderCheckBox->setToolTip(QCoreApplication::translate("SettingsDialog", "Preload the next/previous image.\n"
"Results in a much faster image switching (at the expense of wasting more RAM).", nullptr));
#endif // QT_CONFIG(tooltip)
        usePreloaderCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Use preloader (recommended)", nullptr));
        label_41->setText(QCoreApplication::translate("SettingsDialog", "Load adjacent images in background", nullptr));
        label_17->setText(QCoreApplication::translate("SettingsDialog", "Thumbnailer thread count:", nullptr));
        thumbnailerThreadsLabel->setText(QCoreApplication::translate("SettingsDialog", "2", nullptr));
#if QT_CONFIG(tooltip)
        useThumbnailCacheCheckBox->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        useThumbnailCacheCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Use thumbnail cache (recommended)", nullptr));
        thumbnailResolutionLabel->setText(QCoreApplication::translate("SettingsDialog", "Thumbnail cache resolution:", nullptr));
        thumbnailResolutionValueLabel->setText(QCoreApplication::translate("SettingsDialog", "256 px", nullptr));
        thumbnailCacheQuotaLabel->setText(QCoreApplication::translate("SettingsDialog", "Thumbnail cache size limit:", nullptr));
        thumbnailCacheSizeLabel->setText(QCoreApplication::translate("SettingsDialog", "Current cache size:", nullptr));
        clearThumbnailCacheButton->setText(QCoreApplication::translate("SettingsDialog", "Clear", nullptr));
        labelExcludedCachePaths->setText(QCoreApplication::translate("SettingsDialog", "Exclude paths from caching (separated by semicolon ';'):", nullptr));
#if QT_CONFIG(tooltip)
        excludedCachePathsLineEdit->setToolTip(QCoreApplication::translate("SettingsDialog", "Paths to folders that should not be cached, separated by ';'.\n"
"Example: D:\\Downloads; E:\\Pictures", nullptr));
#endif // QT_CONFIG(tooltip)
        unloadThumbsCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Unload off-screen thumbnails", nullptr));
        label_51->setText(QCoreApplication::translate("SettingsDialog", "Dynamically unload items to save memory", nullptr));
        saveOverlayCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Show save overlay when editing images", nullptr));
        label_3->setText(QCoreApplication::translate("SettingsDialog", "JPEG save quality:", nullptr));
        JPEGQualityLabel->setText(QCoreApplication::translate("SettingsDialog", "q", nullptr));
        confirmTrashCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Confirm moving to trash", nullptr));
        confirmDeleteCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Confirm file delete (!)", nullptr));
        multiInstanceCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Allow multiple instances", nullptr));
        memoryLimitLabel->setText(QCoreApplication::translate("SettingsDialog", "Memory allocation limit per image, MB:", nullptr));
        label_aiUpscaleHeader->setText(QCoreApplication::translate("SettingsDialog", "AI Upscale", nullptr));
        useUpscaylCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Use Upscayl", nullptr));
        label_upscaylModel->setText(QCoreApplication::translate("SettingsDialog", "Model:", nullptr));
        label_upscaylGetModels->setText(QCoreApplication::translate("SettingsDialog", "<a href=\"https://github.com/upscayl/custom-models/tree/main/models\"><span style=\"text-decoration: underline; color:#007af4;\">Get more models</span></a>", nullptr));
        preloadUpscaylCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Load engine at startup and keep ready in video memory", nullptr));
        upscaylLimitCheckBox->setText(QCoreApplication::translate("SettingsDialog", "Enable upscaling only when zoom exceeds:", nullptr));
        upscaylLimitValueLabel->setText(QCoreApplication::translate("SettingsDialog", "200%", nullptr));
        label_53->setText(QCoreApplication::translate("SettingsDialog", "About qimgv-plus", nullptr));
        aboutAppTextBrowser->setMarkdown(SettingsEditorModel::aboutText());
        OK->setText(QCoreApplication::translate("SettingsDialog", "OK", nullptr));
        pushButton->setText(QCoreApplication::translate("SettingsDialog", "Apply", nullptr));
        Cancel->setText(QCoreApplication::translate("SettingsDialog", "Cancel", nullptr));
}
