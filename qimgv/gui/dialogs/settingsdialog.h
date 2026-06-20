#pragma once

#include <QDialog>
#include <QFileDialog>
#include <QColorDialog>
#include <QThreadPool>
#include <QTableWidget>
#include <QTextBrowser>
#include <QListWidget>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include "gui/customwidgets/colorselectorbutton.h"
#include "gui/dialogs/shortcutcreatordialog.h"
#include "gui/dialogs/scripteditordialog.h"
#include "themestore.h"
#include "components/actionmanager/actionmanager.h"
#include "gui/customwidgets/ssidebar.h"
#include <QScrollArea>
#include <QHeaderView>
#include <QGridLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>


class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();
    void switchToPage(int number);

public slots:
    int exec();

private:
    void setupUi();
    void retranslateUi();
    QVBoxLayout *verticalLayout_3;
    QHBoxLayout *horizontalLayout_22;
    QWidget *widget_17;
    QVBoxLayout *verticalLayout_37;
    SSideBar *sideBar2;
    QWidget *versionInfoWidget;
    QHBoxLayout *horizontalLayout_41;
    QLabel *appIconLabel;
    QLabel *versionLabel;
    QSpacerItem *horizontalSpacer_10;
    QLabel *qtIconLabel;
    QLabel *qtVersionLabel;
    QSpacerItem *horizontalSpacer;
    QVBoxLayout *verticalLayout_6;
    QStackedWidget *stackedWidget;
    QWidget *General;
    QVBoxLayout *verticalLayout_4;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_5;
    QLabel *label_20;
    QWidget *widget_18;
    QSpacerItem *verticalSpacer;
    QWidget *generalGroup;
    QVBoxLayout *verticalLayout_12;
    QVBoxLayout *verticalLayout_20;
    QHBoxLayout *horizontalLayout_18;
    QLabel *label_46;
    QComboBox *langComboBox;
    QLabel *label_48;
    QSpacerItem *horizontalSpacer_12;
    QWidget *widget_20;
    QCheckBox *fullscreenCheckBox;
    QCheckBox *startInFolderViewCheckBox;
    QSpacerItem *verticalSpacer_19;
    QWidget *UIOptionsGroup;
    QVBoxLayout *verticalLayout_11;
    QHBoxLayout *horizontalLayout_16;
    QLabel *label_15;
    QVBoxLayout *verticalLayout_7;
    QGridLayout *gridLayout;
    QCheckBox *showExtendedInfoTitle;
    QCheckBox *showInfoBarFullscreen;
    QCheckBox *cursorAutohideCheckBox;
    QCheckBox *enableSmoothScrollCheckBox;
    QCheckBox *enableSmoothZoomCheckBox;
    QWidget *widget_9;
    QHBoxLayout *horizontalLayout_23;
    QLabel *label;
    QRadioButton *zoomIndicatorOn;
    QRadioButton *zoomIndicatorOff;
    QRadioButton *zoomIndicatorAuto;
    QSpacerItem *horizontalSpacer_6;
    QWidget *widget_19;
    QHBoxLayout *horizontalLayout_28;
    QCheckBox *autoResizeWindowCheckBox;
    QLabel *label_50;
    QSpacerItem *horizontalSpacer_19;
    QHBoxLayout *horizontalLayout_19;
    QLabel *label_39;
    QSlider *autoResizeLimitSlider;
    QLabel *autoResizeLimit;
    QSpacerItem *horizontalSpacer_17;
    QSpacerItem *verticalSpacer_2;
    QWidget *thumbnailPanelGroup;
    QVBoxLayout *verticalLayout_14;
    QHBoxLayout *horizontalLayout_6;
    QCheckBox *enablePanelCheckBox;
    QWidget *thumbnailPanelGroupContents;
    QVBoxLayout *verticalLayout_15;
    QGridLayout *gridLayout_3;
    QCheckBox *squareThumbnailsCheckBox;
    QCheckBox *pinPanelCheckBox;
    QCheckBox *panelFullscreenOnlyCheckBox;
    QCheckBox *panelCenterSelectionCheckBox;
    QCheckBox *showSubfoldersInPanelCheckBox;
    QSpacerItem *horizontalSpacer_8;
    QWidget *widget_6;
    QGridLayout *gridLayout_4;
    QRadioButton *thumbStyleExtended;
    QLabel *label_8;
    QLabel *label_18;
    QLabel *label_25;
    QSpacerItem *horizontalSpacer_26;
    QRadioButton *thumbStyleSimple;
    QWidget *widget_2;
    QHBoxLayout *horizontalLayout_24;
    QLabel *label_4;
    QSlider *panelSizeSlider;
    QSpacerItem *horizontalSpacer_11;
    QLabel *panelPositionLabel;
    QComboBox *panelPositionComboBox;
    QSpacerItem *horizontalSpacer_24;
    QSpacerItem *verticalSpacer_15;
    QWidget *folderNavGroup;
    QVBoxLayout *verticalLayout_16;
    QVBoxLayout *verticalLayout_17;
    QHBoxLayout *horizontalLayout_32;
    QLabel *label_30;
    QGridLayout *gridLayout_5;
    QRadioButton *folderEndNoAction;
    QRadioButton *folderEndLoop;
    QRadioButton *folderEndSwitchFolder;
    QLabel *label_16;
    QSpacerItem *horizontalSpacer_25;
    QWidget *widget_7;
    QHBoxLayout *horizontalLayout_33;
    QLabel *label_6;
    QComboBox *sortingComboBox;
    QSpacerItem *horizontalSpacer_15;
    QCheckBox *sortFoldersCheckBox;
    QWidget *widget_31;
    QCheckBox *showHiddenFilesCheckBox;
    QSpacerItem *verticalSpacer_5;
    QWidget *slideshowGroup;
    QVBoxLayout *verticalLayout_18;
    QHBoxLayout *horizontalLayout_34;
    QLabel *label_42;
    QHBoxLayout *slideshowGroupContents;
    QLabel *label_27;
    QSpinBox *slideshowIntervalSpinBox;
    QSpacerItem *horizontalSpacer_27;
    QCheckBox *loopSlideshowCheckBox;
    QSpacerItem *horizontalSpacer_16;
    QSpacerItem *verticalSpacer_3;
    QWidget *View;
    QVBoxLayout *verticalLayout_19;
    QScrollArea *scrollArea_3;
    QWidget *scrollAreaWidgetContents_3;
    QVBoxLayout *verticalLayout_27;
    QLabel *label_9;
    QWidget *widget_4;
    QSpacerItem *verticalSpacer_8;
    QWidget *displayGroup;
    QVBoxLayout *verticalLayout_13;
    QLabel *label_40;
    QHBoxLayout *horizontalLayout_29;
    QLabel *label_2;
    QRadioButton *fitModeWindow;
    QRadioButton *fitModeWidth;
    QRadioButton *fitMode1to1;
    QRadioButton *fitModeWindowStretch;
    QSpacerItem *horizontalSpacer_21;
    QCheckBox *keepFitModeCheckBox;
    QWidget *widget_3;
    QHBoxLayout *horizontalLayout_31;
    QLabel *label_26;
    QRadioButton *focus1to1Top;
    QRadioButton *focus1to1Center;
    QRadioButton *focus1to1Cursor;
    QSpacerItem *horizontalSpacer_23;
    QLabel *label_10;
    QWidget *widget_8;
    QCheckBox *transparencyGridCheckBox;
    QWidget *widget_21;
    QHBoxLayout *horizontalLayout_30;
    QCheckBox *expandImageCheckBox;
    QSlider *expandLimitSlider;
    QWidget *expandImagesGroupContents;
    QHBoxLayout *horizontalLayout_17;
    QLabel *expandLimitLabel;
    QSpacerItem *horizontalSpacer_22;
    QLabel *label_13;
    QSpacerItem *verticalSpacer_20;
    QWidget *zoomGroup;
    QVBoxLayout *verticalLayout_8;
    QLabel *label_19;
    QCheckBox *unlockMinZoomCheckBox;
    QLabel *label_44;
    QWidget *widget_11;
    QHBoxLayout *horizontalLayout_20;
    QLabel *label_12;
    QSlider *zoomStepSlider;
    QLabel *zoomStepLabel;
    QSpacerItem *horizontalSpacer_4;
    QWidget *widget_22;
    QCheckBox *useFixedZoomLevelsCheckBox;
    QWidget *widget;
    QHBoxLayout *horizontalLayout_26;
    QLineEdit *zoomLevels;
    QPushButton *resetZoomLevelsButton;
    QSpacerItem *horizontalSpacer_30;
    QSpacerItem *verticalSpacer_13;
    QWidget *scalingGroup;
    QVBoxLayout *verticalLayout_24;
    QLabel *title3;
    QHBoxLayout *horizontalLayout_5;
    QLabel *scalingQualityLabel;
    QComboBox *scalingQualityComboBox;
    QSpacerItem *horizontalSpacer_29;
    QWidget *widget_10;
    QCheckBox *applyFilterAt100CheckBox;
    QSpacerItem *verticalSpacer_7;
    QWidget *Theme;
    QVBoxLayout *verticalLayout;
    QScrollArea *scrollArea_2;
    QWidget *scrollAreaWidgetContents_2;
    QVBoxLayout *verticalLayout_25;
    QLabel *label_45;
    QWidget *widget_23;
    QSpacerItem *verticalSpacer_4;
    QWidget *colorsPresetGroup;
    QVBoxLayout *verticalLayout_38;
    QHBoxLayout *horizontalLayout_3;
    QLabel *loadPresetLabel;
    QComboBox *themeSelectorComboBox;
    QSpacerItem *horizontalSpacer_13;
    QCheckBox *useSystemColorsCheckBox;
    ClickableLabel *modifySystemSchemeLabel;
    QSpacerItem *horizontalSpacer_18;
    QSpacerItem *verticalSpacer_9;
    QWidget *colorsGroup;
    QVBoxLayout *verticalLayout_23;
    QWidget *colorConfigSubgroup;
    QGridLayout *gridLayout_2;
    ColorSelectorButton *colorSelectorAccent;
    ColorSelectorButton *colorSelectorIcons;
    ColorSelectorButton *colorSelectorFullscreen;
    ColorSelectorButton *colorSelectorBackground;
    QSpacerItem *horizontalSpacer_28;
    QLabel *label_33;
    QLabel *label_34;
    QLabel *label_35;
    ColorSelectorButton *colorSelectorOverlayText;
    QLabel *label_11;
    QLabel *label_14;
    ColorSelectorButton *colorSelectorFolderview;
    ColorSelectorButton *colorSelectorText;
    ColorSelectorButton *colorSelectorWidgetBorder;
    QLabel *label_36;
    ColorSelectorButton *colorSelectorOverlay;
    QLabel *label_21;
    QLabel *label_31;
    QLabel *label_22;
    ColorSelectorButton *colorSelectorScrollbar;
    ColorSelectorButton *colorSelectorFolderviewPanel;
    QLabel *label_37;
    ColorSelectorButton *colorSelectorWidget;
    QLabel *label_32;
    ColorSelectorButton *colorSelectorThumbpanel;
    QLabel *label_thumbpanel;
    QLabel *label_23;
    QWidget *widget_24;
    QSpacerItem *verticalSpacer_14;
    QWidget *windowTweaksGroup;
    QVBoxLayout *verticalLayout_39;
    QLabel *label_38;
    QGridLayout *opacitySlidersGridLayout;
    QLabel *label_5;
    QSlider *bgOpacitySlider;
    QLabel *bgOpacityPercentLabel;
    QSpacerItem *horizontalSpacer_14;
    QLabel *label_5_thumb;
    QSlider *thumbOpacitySlider;
    QLabel *thumbOpacityPercentLabel;
    QSpacerItem *horizontalSpacer_14_thumb;
    QCheckBox *useBlackBackgroundCheckBox;
    QSpacerItem *verticalSpacer_11;
    QWidget *Controls;
    QVBoxLayout *verticalLayout_28;
    QLabel *label_29;
    QWidget *widget_5;
    QSpacerItem *verticalSpacer_10;
    QVBoxLayout *verticalLayout_33;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *pushButton_2;
    QPushButton *pushButton_8;
    QPushButton *pushButton_4;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pushButton_3;
    QTableWidget *shortcutsTableWidget;
    QSpacerItem *verticalSpacer_18;
    QWidget *widget_12;
    QVBoxLayout *verticalLayout_21;
    QHBoxLayout *horizontalLayout_8;
    QCheckBox *clickableEdgesCheckBox;
    QCheckBox *clickableEdgesVisibleCheckBox;
    QWidget *widget_30;
    QHBoxLayout *horizontalLayout_12;
    QLabel *label_28;
    QComboBox *imageScrollingComboBox;
    QSpacerItem *horizontalSpacer_31;
    QLabel *label_52;
    QWidget *widget_29;
    QHBoxLayout *horizontalLayout_9;
    QLabel *label_54;
    QSlider *mouseScrollingSpeedSlider;
    QLabel *mouseScrollingSpeedLabel;
    QSpacerItem *horizontalSpacer_34;
    QWidget *widget_31_v2;
    QCheckBox *trackpadDetectionCheckBox;
    QLabel *label_7;
    QWidget *Scripts;
    QVBoxLayout *verticalLayout_29;
    QLabel *label_47;
    QWidget *widget_14;
    QSpacerItem *verticalSpacer_12;
    QVBoxLayout *verticalLayout_35;
    QLabel *label_24;
    QLabel *label_43;
    QHBoxLayout *horizontalLayout_21;
    QPushButton *pushButton_9;
    QPushButton *pushButton_6;
    QPushButton *pushButton_7;
    QSpacerItem *horizontalSpacer_5;
    QListWidget *scriptsListWidget;
    QWidget *Advanced;
    QVBoxLayout *verticalLayout_30;
    QScrollArea *scrollArea_4;
    QWidget *scrollAreaWidgetContents_4;
    QVBoxLayout *verticalLayout_31;
    QLabel *label_49;
    QWidget *widget_13;
    QSpacerItem *verticalSpacer_17;
    QWidget *advancedGroup;
    QVBoxLayout *verticalLayout_34;
    QCheckBox *usePreloaderCheckBox;
    QLabel *label_41;
    QWidget *widget_15;
    QHBoxLayout *horizontalLayout;
    QLabel *label_17;
    QSlider *thumbnailerThreadsSlider;
    QLabel *thumbnailerThreadsLabel;
    QSpacerItem *horizontalSpacer_9;
    QCheckBox *useThumbnailCacheCheckBox;
    QHBoxLayout *horizontalLayout_thumbRes;
    QLabel *thumbnailResolutionLabel;
    QSlider *thumbnailResolutionSlider;
    QLabel *thumbnailResolutionValueLabel;
    QSpacerItem *horizontalSpacer_thumbRes;
    QLabel *labelExcludedCachePaths;
    QLineEdit *excludedCachePathsLineEdit;
    QCheckBox *unloadThumbsCheckBox;
    QLabel *label_51;
    QWidget *widget_26;
    QCheckBox *saveOverlayCheckBox;
    QHBoxLayout *horizontalLayout_10;
    QLabel *label_3;
    QSlider *JPEGQualitySlider;
    QLabel *JPEGQualityLabel;
    QSpacerItem *horizontalSpacer_7;
    QWidget *widget_25;
    QCheckBox *confirmTrashCheckBox;
    QCheckBox *confirmDeleteCheckBox;
    QWidget *widget_27;
    QCheckBox *animatedJxlCheckBox;
    QWidget *widget_multi_instance_sep;
    QCheckBox *multiInstanceCheckBox;
    QWidget *widget_28;
    QHBoxLayout *horizontalLayout_25;
    QLabel *memoryLimitLabel;
    QSpinBox *memoryLimitSpinBox;
    QSpacerItem *horizontalSpacer_32;
    QSpacerItem *verticalSpacer_16;
    QWidget *AIUpscale;
    QVBoxLayout *verticalLayout_aiUpscale;
    QScrollArea *scrollArea_aiUpscale;
    QWidget *scrollAreaWidgetContents_aiUpscale;
    QVBoxLayout *verticalLayout_aiUpscaleContents;
    QLabel *label_aiUpscaleHeader;
    QWidget *widget_aiUpscaleHeaderLine;
    QSpacerItem *verticalSpacer_aiUpscaleTop;
    QWidget *aiUpscaleGroup;
    QVBoxLayout *verticalLayout_aiUpscaleGroup;
    QCheckBox *useUpscaylCheckBox;
    QHBoxLayout *horizontalLayout_upscaylModel;
    QLabel *label_upscaylModel;
    QComboBox *upscaylModelComboBox;
    QLabel *label_upscaylGetModels;
    QSpacerItem *horizontalSpacer_upscaylModel;
    QCheckBox *preloadUpscaylCheckBox;
    QCheckBox *upscaylLimitCheckBox;
    QHBoxLayout *horizontalLayout_upscaylLimit;
    QSpacerItem *horizontalSpacer_upscaylLimitIndent;
    QSlider *upscaylLimitSlider;
    QLabel *upscaylLimitValueLabel;
    QSpacerItem *horizontalSpacer_upscaylLimitSpacer;
    QSpacerItem *verticalSpacer_aiUpscaleBottom;
    QWidget *About;
    QVBoxLayout *verticalLayout_2;
    QScrollArea *scrollArea_5;
    QWidget *scrollAreaWidgetContents_5;
    QVBoxLayout *verticalLayout_32;
    QLabel *label_53;
    QWidget *widget_16;
    QSpacerItem *verticalSpacer_6;
    QTextBrowser *aboutAppTextBrowser;
    QWidget *settingsBottomWidget;
    QHBoxLayout *horizontalLayout_15;
    QWidget *versionLabelWidget;
    QVBoxLayout *verticalLayout_22;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *OK;
    QPushButton *pushButton;
    QPushButton *Cancel;

    void readColorScheme();
    void setColorScheme(ColorScheme colors);
    void saveColorScheme();
    void readSettings();
    void readShortcuts();
    void readScripts();
    

    void saveShortcuts();
    void addShortcutToTable(const QString &action, const QString &shortcut);
    void addScriptToList(const QString &name);

    void setupSidebar();
    void removeShortcutAt(int row);
    void adjustSizeToContents();
    QMap<QString, QString> langs; // <"en_US", "English">
    QButtonGroup fitModeGrp, folderEndGrp, zoomIndGrp;

private slots:
    void saveSettings();
    void saveSettingsAndClose();

    void addScript();
    void editScript();
    void editScript(QListWidgetItem *item);
    void editScript(QString name);
    void removeScript();

    void addShortcut();
    void editShortcut();
    void editShortcut(int row);
    void removeShortcut();
    void resetShortcuts();
    void onBgOpacitySliderChanged(int value);
    void onThumbOpacitySliderChanged(int value);
    void onThumbOpacitySliderReleased();
    void onThumbnailerThreadsSliderChanged(int value);
    void onExpandLimitSliderChanged(int value);
    void onZoomStepSliderChanged(int value);
    void onJPEGQualitySliderChanged(int value);
    void onPNGQualitySliderChanged(int value);
    void onModernQualitySliderChanged(int value);
    void onAutoResizeLimitSliderChanged(int value);
    void onMouseScrollingSpeedSliderChanged(int value);
    void onThumbnailResolutionSliderChanged(int value);

    void resetZoomLevels();
signals:
    void settingsChanged();
private:
    QSlider *pngQualitySlider = nullptr;
    QLabel *pngQualityLabel = nullptr;
    QSlider *modernQualitySlider = nullptr;
    QLabel *modernQualityLabel = nullptr;

    QWidget *casContainerWidget = nullptr;
    QSlider *casSharpeningSlider = nullptr;
    QLabel *casSharpeningLabel = nullptr;
    QSlider *casContrastSlider = nullptr;
    QLabel *casContrastLabel = nullptr;

    QCheckBox *colorManagementCheckBox = nullptr;
    QComboBox *monitorProfileComboBox = nullptr;
    QLineEdit *customProfilePathEdit = nullptr;
    QPushButton *customProfileBrowseButton = nullptr;
    QWidget *customProfileContainer = nullptr;
    QGroupBox *colorManagementGroupBox = nullptr;
    QCheckBox *useCustomAccentCheckBox = nullptr;
};
