#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "components/copytargets/copytargetlist.h"
#include "components/shellinfo/fileinfotext.h"
#include "gui/quick/ui/overlays/overlaycoordinator.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

constexpr qint64 kFileSize = 3 * 1024 * 1024;
constexpr int kFileSizePrecision = 1;
// Display time short enough to wait for.
constexpr int kShortMessageMs = 20;
// Waits for a timer of the fullscreen chrome.
constexpr int kChromeWaitMs = 2 * FullscreenChromeController::kHideTimeoutMs;
constexpr float kTolerance = 0.0001f;

ShellFileInfo documentInfo() {
  ShellFileInfo info;
  info.index = 2;
  info.fileCount = 10;
  info.fileName = u"photo.png"_s;
  info.imageSize = QSize(1920, 1080);
  info.fileSize = kFileSize;
  info.format = u"png"_s;
  info.colorProfile = u"sRGB"_s;
  return info;
}

UiSettingsSnapshot settingsWith(bool infoBarFullscreen, bool panelEnabled,
                                SettingsEnums::PanelPosition position) {
  UiSettingsSnapshot settings;
  settings.overlays.infoBarFullscreen = infoBarFullscreen;
  settings.panel.enabled = panelEnabled;
  settings.panel.position = position;
  settings.viewer.casSharpening = CasParameters::kDefaultSharpening;
  settings.viewer.casContrast = CasParameters::kDefaultContrast;
  return settings;
}

UiSettingsSnapshot defaultSettings() {
  return settingsWith(false, false, SettingsEnums::PanelPosition::Top);
}

bool fuzzyEqual(float a, float b) {
  return std::abs(a - b) < kTolerance;
}
} // namespace

class OverlayTests : public QObject {
  Q_OBJECT

private slots:
  //--- fullscreen info texts ------------------------------------------------
  void infoBarShowsPositionNameAndDetails() {
    const FullscreenInfoText text = fullscreenInfoFor(documentInfo(), MODE_DOCUMENT, QLocale::c());
    QCOMPARE(text.position, u"[ 3/10 ]"_s);
    QCOMPARE(text.name, u"photo.png"_s);
    QCOMPARE(text.details, u"1920 x 1080 (16:9)  sRGB  PNG  "_s +
                               QLocale::c().formattedDataSize(kFileSize, kFileSizePrecision));
  }

  void infoBarMarksEditsAndSkipsMissingDetails() {
    ShellFileInfo info = documentInfo();
    info.edited = true;
    info.colorProfile.clear();
    info.fileSize = 0;
    const FullscreenInfoText text = fullscreenInfoFor(info, MODE_DOCUMENT, QLocale::c());
    QCOMPARE(text.name, u"photo.png  *"_s);
    QCOMPARE(text.details, u"1920 x 1080 (16:9)  PNG"_s);
  }

  void infoBarWithoutAFile() {
    const FullscreenInfoText none = fullscreenInfoFor({}, MODE_DOCUMENT, QLocale::c());
    QCOMPARE(none.name, u"No file opened."_s);
    QVERIFY(none.position.isEmpty());
    const FullscreenInfoText folder =
        fullscreenInfoFor(documentInfo(), MODE_FOLDERVIEW, QLocale::c());
    QCOMPARE(folder, none);
  }

  //--- copy targets -----------------------------------------------------------
  void savedTargetsAreKept() {
    const QStringList saved{u"C:/a"_s, u"C:/b"_s};
    QCOMPARE(copyTargetsFrom(saved, u"C:/home"_s), saved);
  }

  void unsetTargetsListTheHomeFolders() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QDir dir(home.path());
    for (const QString &name : {u"b"_s, u"A"_s, u".hidden"_s, u"Links"_s})
      QVERIFY(dir.mkdir(name));
    QFile file(dir.filePath(u"file.txt"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    const QStringList expected{home.path(), home.path() + u"/A"_s, home.path() + u"/b"_s};
    QCOMPARE(copyTargetsFrom({}, home.path()), expected);
    QCOMPARE(copyTargetsFrom({u"@placeholder"_s}, home.path()), expected);
    QCOMPARE(copyTargetsFrom({QString()}, home.path()), expected);
  }

  void homeFoldersAreLimited() {
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QDir dir(home.path());
    constexpr int kFolders = kMaxCopyTargets + 3;
    for (int i = 0; i < kFolders; ++i)
      QVERIFY(dir.mkdir(u"folder%1"_s.arg(i, 2, 10, u'0')));
    QCOMPARE(copyTargetsFrom({}, home.path()).size(), kMaxCopyTargets);
  }

  void savableTargetsDropEmptyAndRepeated() {
    QCOMPARE(savableCopyTargets({u"C:/a"_s, QString(), u"C:/b"_s, u"C:/a"_s}),
             (QStringList{u"C:/a"_s, u"C:/b"_s}));
  }

  //--- notifications ----------------------------------------------------------
  void notificationDefaultsByKind() {
    const NotificationPresentation error =
        notificationPresentationFor({u"failed"_s, NotificationKind::Error, std::nullopt});
    QCOMPARE(error.icon, FluentIcon::ErrorCircle20);
    QVERIFY(error.durationMs > notificationPresentationFor({u"x"_s}).durationMs);
    const NotificationPresentation end =
        notificationPresentationFor({QString(), NotificationKind::DirectoryEnd, std::nullopt});
    QCOMPARE(end.text, u"End of directory"_s);
    QCOMPARE(end.icon, FluentIcon::ArrowNext20);
    QCOMPARE(notificationPresentationFor({u"x"_s, NotificationKind::Info, kShortMessageMs})
                 .durationMs,
             kShortMessageMs);
  }

  void notificationHidesAfterItsDisplayTime() {
    NotificationOverlayModel model;
    QVERIFY(!model.isCreated());
    model.showMessage(u"hello"_s, kShortMessageMs);
    QVERIFY(model.isCreated());
    QVERIFY(model.isVisible());
    QCOMPARE(model.text(), u"hello"_s);
    QTRY_VERIFY(!model.isVisible());

    model.showSuccess(u"done"_s);
    QCOMPARE(model.icon(), FluentIcon::CheckmarkCircle20);
    model.hideNotifications();
    QVERIFY(!model.isVisible());
  }

  //--- sliders ----------------------------------------------------------------
  void sliderValueTexts() {
    QCOMPARE(sliderValueText(120, SliderValueFormat::Percent), u"120%"_s);
    QCOMPARE(sliderValueText(-30, SliderValueFormat::Degrees), u"-30\u00B0"_s);
    QCOMPARE(sliderValueText(150, SliderValueFormat::SignedHundredths), u"+1.50"_s);
    QCOMPARE(sliderValueText(-25, SliderValueFormat::SignedHundredths), u"-0.25"_s);
    QCOMPARE(sliderValueText(5, SliderValueFormat::Hundredths), u"0.05"_s);
    QCOMPARE(sliderValueText(-7, SliderValueFormat::Integer), u"-7"_s);
  }

  void colorAdjustmentsPreviewEdits() {
    ColorAdjustmentsEditor editor;
    QCOMPARE(editor.rowCount(), int(ColorAdjustmentsEditor::RowCount));
    QCOMPARE(editor.adjustments(), ColorAdjustments{});
    QSignalSpy preview(&editor, &ColorAdjustmentsEditor::previewChanged);

    editor.setValue(ColorAdjustmentsEditor::Exposure, 150);
    QCOMPARE(preview.size(), 1);
    QVERIFY(fuzzyEqual(preview.last().at(0).value<ColorAdjustments>().exposure, 1.5f));
    QCOMPARE(editor.data(editor.index(ColorAdjustmentsEditor::Exposure),
                         AdjustmentSliderModel::ValueTextRole)
                 .toString(),
             u"+1.50"_s);

    editor.setValue(ColorAdjustmentsEditor::Hue, 999);
    QCOMPARE(editor.adjustments().hue, 180.0f);
    editor.setValue(ColorAdjustmentsEditor::Hue, 180);
    QCOMPARE(preview.size(), 2);

    editor.resetValue(ColorAdjustmentsEditor::Hue);
    QCOMPARE(editor.adjustments().hue, ColorAdjustments::kNeutralHue);
  }

  void colorAdjustmentsCompareShowsTheOriginal() {
    ColorAdjustmentsEditor editor;
    editor.setValue(ColorAdjustmentsEditor::Saturation, 150);
    QSignalSpy preview(&editor, &ColorAdjustmentsEditor::previewChanged);
    editor.setComparing(true);
    QCOMPARE(preview.last().at(0).value<ColorAdjustments>(), ColorAdjustments{});
    editor.setValue(ColorAdjustmentsEditor::Saturation, 160);
    QCOMPARE(preview.size(), 1);
    editor.setComparing(false);
    QVERIFY(fuzzyEqual(preview.last().at(0).value<ColorAdjustments>().saturation, 1.6f));
  }

  void colorAdjustmentsApplyResets() {
    ColorAdjustmentsEditor editor;
    editor.setValue(ColorAdjustmentsEditor::Contrast, 120);
    QSignalSpy applied(&editor, &ColorAdjustmentsEditor::applyRequested);
    QSignalSpy preview(&editor, &ColorAdjustmentsEditor::previewChanged);
    editor.apply();
    QCOMPARE(applied.size(), 1);
    QVERIFY(fuzzyEqual(applied.last().at(0).value<ColorAdjustments>().contrast, 1.2f));
    QCOMPARE(preview.last().at(0).value<ColorAdjustments>(), ColorAdjustments{});
    QCOMPARE(editor.adjustments(), ColorAdjustments{});
  }

  void casEditorLoadsAndPublishes() {
    CasSettingsEditor editor;
    QSignalSpy edited(&editor, &CasSettingsEditor::parametersEdited);
    editor.load({.sharpening = 0.5f, .contrast = 0.2f});
    QCOMPARE(edited.size(), 0);
    QCOMPARE(editor.data(editor.index(CasSettingsEditor::Sharpening),
                         AdjustmentSliderModel::ValueTextRole)
                 .toString(),
             u"0.50"_s);
    editor.setValue(CasSettingsEditor::Contrast, 40);
    QCOMPARE(edited.size(), 1);
    QVERIFY(fuzzyEqual(edited.last().at(0).value<CasParameters>().contrast, 0.4f));
    editor.resetAll();
    QCOMPARE(edited.last().at(0).value<CasParameters>(), CasParameters{});
  }

  //--- copy targets model -----------------------------------------------------
  void copyTargetsActivateByRowAndShortcut() {
    CopyTargetsModel model;
    QStringList many;
    for (int i = 0; i < kMaxCopyTargets + 2; ++i)
      many << u"C:/target/%1"_s.arg(i);
    model.setTargets(many);
    QVERIFY(model.isLoaded());
    QCOMPARE(model.rowCount(), kMaxCopyTargets);
    QCOMPARE(model.data(model.index(1), CopyTargetsModel::DisplayNameRole).toString(), u"1"_s);
    QCOMPARE(model.data(model.index(1), CopyTargetsModel::ShortcutRole).toString(), u"2"_s);

    QSignalSpy copied(&model, &CopyTargetsModel::copyRequested);
    QSignalSpy moved(&model, &CopyTargetsModel::moveRequested);
    model.activate(0);
    QCOMPARE(copied.last().at(0).toString(), u"C:/target/0"_s);
    model.setMode(CopyTargetsModel::Mode::Move);
    QVERIFY(model.activateShortcut(u"3"_s));
    QCOMPARE(moved.last().at(0).toString(), u"C:/target/2"_s);
    QVERIFY(!model.activateShortcut(u"0"_s));
    QVERIFY(!model.activateShortcut(u"Q"_s));
  }

  void copyTargetEditsArePublished() {
    CopyTargetsModel model;
    model.setTargets({u"C:/a"_s, u"C:/b"_s});
    QSignalSpy edited(&model, &CopyTargetsModel::targetsEdited);
    model.setDirectory(1, QUrl::fromLocalFile(u"C:/a"_s).toString());
    QCOMPARE(model.targets(), (QStringList{u"C:/a"_s, u"C:/a"_s}));
    QCOMPARE(edited.last().at(0).toStringList(), QStringList{u"C:/a"_s});
  }

  //--- rename prompt ----------------------------------------------------------
  void renameSelectsTheBaseName() {
    RenamePromptController prompt;
    prompt.setName(u"a.b.jpg"_s);
    QCOMPARE(prompt.selectionEnd(), 3);
    prompt.setName(u"README"_s);
    QCOMPARE(prompt.selectionEnd(), 6);
  }

  void renameAcceptsOnlyANonEmptyName() {
    RenamePromptController prompt;
    QSignalSpy renamed(&prompt, &RenamePromptController::renameRequested);
    QSignalSpy closed(&prompt, &RenamePromptController::closeRequested);
    QVERIFY(!prompt.accept(QString()));
    QCOMPARE(closed.size(), 0);
    QVERIFY(prompt.accept(u"new.png"_s));
    QCOMPARE(renamed.last().at(0).toString(), u"new.png"_s);
    QCOMPARE(closed.size(), 1);
    prompt.cancel();
    QCOMPARE(closed.size(), 2);
    QCOMPARE(renamed.size(), 1);
  }

  void renamePassThroughShortcuts() {
    RenamePromptController prompt;
    prompt.setPassThroughShortcuts({u"Esc"_s, u"F2"_s});
    QVERIFY(prompt.passesThrough(u"F2"_s));
    QVERIFY(!prompt.passesThrough(u"A"_s));
    QVERIFY(!prompt.passesThrough(QString()));
  }

  //--- fullscreen chrome ------------------------------------------------------
  void chromeIsInactiveWindowed() {
    FullscreenChromeController chrome;
    chrome.applySettings(settingsWith(true, false, SettingsEnums::PanelPosition::Top));
    QVERIFY(!chrome.isInfoBarActive());
    QVERIFY(!chrome.areControlsActive());
    chrome.toggleInfoBar();
    QVERIFY(!chrome.isInfoBarActive());
  }

  void infoBarShowsAndHidesInFullscreen() {
    FullscreenChromeController chrome;
    chrome.applySettings(settingsWith(true, false, SettingsEnums::PanelPosition::Top));
    chrome.setFullscreen(true);
    QVERIFY(chrome.isInfoBarActive());
    QVERIFY(chrome.isInfoBarShown());
    QTRY_VERIFY_WITH_TIMEOUT(!chrome.isInfoBarShown(), kChromeWaitMs);
    chrome.pointerMoved();
    QVERIFY(chrome.isInfoBarShown());
    chrome.setFileInfo(documentInfo());
    QCOMPARE(chrome.nameText(), u"photo.png"_s);

    QSignalSpy toggled(&chrome, &FullscreenChromeController::infoBarSettingToggled);
    chrome.toggleInfoBar();
    QCOMPARE(toggled.last().at(0).toBool(), false);
    QVERIFY(!chrome.isInfoBarActive());
    QVERIFY(!chrome.isInfoBarShown());
  }

  void controlsAvoidATopOrRightPanel() {
    FullscreenChromeController chrome;
    chrome.setFullscreen(true);
    chrome.applySettings(settingsWith(false, true, SettingsEnums::PanelPosition::Top));
    QVERIFY(!chrome.areControlsActive());
    chrome.applySettings(settingsWith(false, true, SettingsEnums::PanelPosition::Right));
    QVERIFY(!chrome.areControlsActive());
    chrome.applySettings(settingsWith(false, true, SettingsEnums::PanelPosition::Left));
    QVERIFY(chrome.areControlsActive());
    chrome.applySettings(settingsWith(false, false, SettingsEnums::PanelPosition::Top));
    QVERIFY(chrome.areControlsActive());
  }

  void hoveredControlsStay() {
    FullscreenChromeController chrome;
    chrome.applySettings(defaultSettings());
    chrome.setFullscreen(true);
    QVERIFY(chrome.areControlsShown());
    chrome.setControlsHovered(true);
    QTest::qWait(FullscreenChromeController::kHideTimeoutMs + kShortMessageMs);
    QVERIFY(chrome.areControlsShown());
    chrome.setControlsHovered(false);
    QTRY_VERIFY_WITH_TIMEOUT(!chrome.areControlsShown(), kChromeWaitMs);
  }

  //--- coordinator ------------------------------------------------------------
  void folderViewClosesDocumentOverlays() {
    OverlayCoordinator overlays(defaultSettings());
    overlays.setDocumentDisplayed(true);
    overlays.toggleImageInfo();
    overlays.toggleCopy();
    overlays.toggleColorAdjustments();
    overlays.toggleCasSettings();
    overlays.toggleRename(u"photo.png"_s);
    QVERIFY(overlays.isKeyboardOverlayOpen());

    overlays.setFolderViewActive(true);
    for (OverlayState *state : {overlays.imageInfo(), overlays.copy(), overlays.rename(),
                                overlays.colorAdjustments(), overlays.casSettings()})
      QVERIFY(!state->isOpen());
    QVERIFY(!overlays.isKeyboardOverlayOpen());

    overlays.toggleImageInfo();
    overlays.toggleCopy();
    overlays.toggleColorAdjustments();
    QVERIFY(!overlays.imageInfo()->isOpen());
    QVERIFY(!overlays.copy()->isOpen());
    QVERIFY(!overlays.colorAdjustments()->isOpen());

    overlays.setFolderViewActive(false);
    QVERIFY(overlays.imageInfo()->isOpen());
  }

  void renameInFolderViewHasABackdrop() {
    OverlayCoordinator overlays(defaultSettings());
    overlays.setFolderViewActive(true);
    overlays.toggleRename(u"a.png"_s);
    QVERIFY(overlays.rename()->isOpen());
    QVERIFY(overlays.renamePrompt()->backdrop());
    QCOMPARE(overlays.renamePrompt()->name(), u"a.png"_s);
    overlays.renamePrompt()->cancel();
    QVERIFY(!overlays.rename()->isOpen());
  }

  void copyNeedsADisplayedImageAndSwitchesMode() {
    OverlayCoordinator overlays(defaultSettings());
    QSignalSpy needed(&overlays, &OverlayCoordinator::copyTargetsNeeded);
    overlays.toggleCopy();
    QVERIFY(!overlays.copy()->isOpen());
    QVERIFY(!overlays.copy()->isCreated());

    overlays.setDocumentDisplayed(true);
    connect(&overlays, &OverlayCoordinator::copyTargetsNeeded, &overlays,
            [&overlays]() { overlays.copyTargets()->setTargets({u"C:/a"_s}); });
    overlays.toggleCopy();
    QVERIFY(overlays.copy()->isOpen());
    overlays.toggleMove();
    QVERIFY(overlays.copy()->isOpen());
    QCOMPARE(overlays.copyTargets()->mode(), CopyTargetsModel::Mode::Move);
    overlays.toggleMove();
    QVERIFY(!overlays.copy()->isOpen());
    QVERIFY(overlays.copy()->isCreated());
    QCOMPARE(needed.size(), 1);
  }

  void saveConfirmFollowsItsSettingAndThePanel() {
    UiSettingsSnapshot settings = settingsWith(false, true, SettingsEnums::PanelPosition::Bottom);
    OverlayCoordinator overlays(settings);
    overlays.setSaveConfirmVisible(true);
    QVERIFY(!overlays.saveConfirm()->isOpen());
    QVERIFY(overlays.isSaveConfirmAtTop());
    QVERIFY(overlays.isCopyAtTop());

    settings.overlays.showSaveOverlay = true;
    settings.panel.position = SettingsEnums::PanelPosition::Top;
    overlays.applySettings(settings);
    QVERIFY(!overlays.isSaveConfirmAtTop());
    QVERIFY(!overlays.isCopyAtTop());
    overlays.setSaveConfirmVisible(true);
    QVERIFY(overlays.saveConfirm()->isOpen());
    overlays.setSaveConfirmVisible(false);
    QVERIFY(!overlays.saveConfirm()->isOpen());
  }

  void adjustmentOverlaysOpenAtThePointer() {
    OverlayCoordinator overlays(defaultSettings());
    overlays.pointerMoved(QPointF(40, 60), false);
    overlays.toggleColorAdjustments();
    QCOMPARE(overlays.colorAdjustments()->anchor(), QPointF(40, 60));
    overlays.pointerMoved(QPointF(70, 80), false);
    overlays.toggleCasSettings();
    QCOMPARE(overlays.casSettings()->anchor(), QPointF(70, 80));
  }

  void casEditsSurviveAnUnchangedSnapshot() {
    UiSettingsSnapshot settings = defaultSettings();
    OverlayCoordinator overlays(settings);
    QSignalSpy edited(&overlays, &OverlayCoordinator::casParametersEdited);
    overlays.toggleCasSettings();
    overlays.casSettingsEditor()->setValue(CasSettingsEditor::Sharpening, 30);
    QCOMPARE(edited.size(), 1);
    overlays.toggleCasSettings();

    overlays.applySettings(settings);
    overlays.toggleCasSettings();
    QVERIFY(fuzzyEqual(overlays.casSettingsEditor()->parameters().sharpening, 0.3f));
    overlays.toggleCasSettings();

    settings.viewer.casSharpening = 0.7;
    overlays.applySettings(settings);
    overlays.toggleCasSettings();
    QVERIFY(fuzzyEqual(overlays.casSettingsEditor()->parameters().sharpening, 0.7f));
  }

  void fileInfoReachesRenameAndChrome() {
    OverlayCoordinator overlays(defaultSettings());
    overlays.setFileInfo(documentInfo());
    QCOMPARE(overlays.renamePrompt()->name(), u"photo.png"_s);
    QCOMPARE(overlays.fullscreenChrome()->positionText(), u"[ 3/10 ]"_s);
    overlays.setFolderViewActive(true);
    QCOMPARE(overlays.fullscreenChrome()->nameText(), u"No file opened."_s);
  }

  void metadataReachesTheImageInfo() {
    OverlayCoordinator overlays(defaultSettings());
    QVERIFY(overlays.imageInfoModel()->isEmpty());
    const QString longValue(ImageInfoModel::kStackedValueThreshold + 1, u'x');
    overlays.setMetadata({{u"Model"_s, u"X100"_s}, {u"Prompt"_s, longValue}});
    ImageInfoModel *model = overlays.imageInfoModel();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->data(model->index(0), ImageInfoModel::NameRole).toString(), u"Model"_s);
    QVERIFY(!model->data(model->index(0), ImageInfoModel::StackedRole).toBool());
    QVERIFY(model->data(model->index(1), ImageInfoModel::StackedRole).toBool());
  }
};

int runOverlayTests(int argc, char **argv) {
  OverlayTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_overlays.moc"
