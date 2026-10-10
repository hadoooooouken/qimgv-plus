#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include <array>

#include "gui/quick/ui/dialogs/dialogcoordinator.h"
#include "testsuites.h"
#include "utils/savefilefilters.h"

namespace {
using namespace Qt::StringLiterals;

const QList<QByteArray> kAllWriterFormats{"bmp", "jpg", "png", "webp", "jxl",
                                          "avif", "qoi", "tif", "gif"};

const QString kJpegFilter = u"JPEG (*.jpg *.jpeg *jpe *jfif)"_s;
const QString kPngFilter = u"PNG (*.png)"_s;
const QString kTiffFilter = u"TIFF (*.tif *.tiff)"_s;

const QSize kOriginal(1000, 500);
const QSize kDesktop(1920, 1080);
// Row of "1920 x 1080 (FullHD)" in ResizeDialogModel::commonSizes().
constexpr int kFullHdRow = 5;
constexpr int kCommonSizeCount = 12;

ResizeDialogInput resizeInput(const QStringList &models = {}, bool useUpscayl = false,
                              const QString &model = {}) {
    return {.originalSize = kOriginal,
            .desktopSize = kDesktop,
            .upscaylModels = models,
            .useUpscayl = useUpscayl,
            .upscaylModel = model};
}
} // namespace

class DialogTests : public QObject {
    Q_OBJECT

private slots:
    //--- save file filters ----------------------------------------------------
    void saveFiltersFollowTheWritableFormatsInFixedOrder() {
        const SaveFileFilters filters = saveFileFiltersFor(kAllWriterFormats, u"C:/a/b.png"_s);
        const QStringList expected{kJpegFilter,           kPngFilter,
                                   u"WebP (*.webp)"_s,    u"JPEG-XL (*.jxl)"_s,
                                   u"AVIF (*.avif *.avifs)"_s, u"QOI (*.qoi)"_s,
                                   u"BMP (*.bmp)"_s,      kTiffFilter};
        QCOMPARE(filters.filters, expected);
        QCOMPARE(filters.selected, kPngFilter);
        QCOMPARE(filters.selectedIndex(), 1);
    }

    void saveFiltersSkipFormatsWithoutAWriter() {
        const SaveFileFilters filters = saveFileFiltersFor({"png", "tif"}, u"C:/a/b.tiff"_s);
        QCOMPARE(filters.filters, (QStringList{kPngFilter, kTiffFilter}));
        QCOMPARE(filters.selected, kTiffFilter);
        QCOMPARE(filters.selectedIndex(), 1);
    }

    void saveFiltersMatchTheSuffixCaseInsensitively() {
        const SaveFileFilters filters = saveFileFiltersFor(kAllWriterFormats, u"C:/a/B.JPG"_s);
        QCOMPARE(filters.selected, kJpegFilter);
        QCOMPARE(filters.selectedIndex(), 0);
    }

    void saveFiltersFallBackToJpeg() {
        const SaveFileFilters known = saveFileFiltersFor(kAllWriterFormats, u"C:/a/b.xyz"_s);
        QCOMPARE(known.selected, kJpegFilter);
        // Without a JPEG writer the first offered filter is selected.
        const SaveFileFilters withoutJpeg = saveFileFiltersFor({"png"}, u"C:/a/b.xyz"_s);
        QCOMPARE(withoutJpeg.selected, kJpegFilter);
        QCOMPARE(withoutJpeg.selectedIndex(), 0);
    }

    //--- dialog session and the modal wait ------------------------------------
    void aRequestOpensAndCreatesTheDialog() {
        ConfirmationDialogModel dialog;
        QSignalSpy created(&dialog, &DialogSession::createdChanged);
        QVERIFY(!dialog.isCreated());
        QVERIFY(dialog.start({.title = u"Delete"_s, .message = u"Delete it?"_s}));
        QVERIFY(dialog.isOpen());
        QVERIFY(dialog.isCreated());
        QCOMPARE(created.count(), 1);
        QCOMPARE(dialog.title(), u"Delete"_s);
        QCOMPARE(dialog.message(), u"Delete it?"_s);
    }

    void theRequestIsPublishedBeforeTheDialogOpens() {
        ConfirmationDialogModel dialog;
        QString titleWhenOpened;
        connect(&dialog, &DialogSession::openChanged, this,
                [&]() { titleWhenOpened = dialog.title(); });
        QVERIFY(dialog.start({.title = u"Delete"_s, .message = u"Delete it?"_s}));
        QCOMPARE(titleWhenOpened, u"Delete"_s);
    }

    void aRequestWhileOpenIsRefused() {
        ConfirmationDialogModel dialog;
        QVERIFY(dialog.start({.title = u"First"_s, .message = {}}));
        QTest::ignoreMessage(QtWarningMsg,
                             "Qt Quick UI: the confirmation dialog is already open; the new "
                             "request was declined");
        QVERIFY(!dialog.start({.title = u"Second"_s, .message = {}}));
        QCOMPARE(dialog.title(), u"First"_s);
    }

    void answeringFinishesTheRequest() {
        ConfirmationDialogModel dialog;
        QSignalSpy finished(&dialog, &DialogSession::finished);
        QVERIFY(dialog.start({}));
        dialog.accept();
        QVERIFY(!dialog.isOpen());
        QVERIFY(dialog.wasAnswered());
        QVERIFY(dialog.result().accepted);
        QCOMPARE(finished.count(), 1);
        // Late answers change nothing.
        dialog.reject();
        QVERIFY(dialog.result().accepted);
        QCOMPARE(finished.count(), 1);
    }

    void anAbandonedRequestKeepsTheDeclinedAnswer() {
        ConfirmationDialogModel dialog;
        QSignalSpy finished(&dialog, &DialogSession::finished);
        QVERIFY(dialog.start({}));
        dialog.abandon();
        QVERIFY(!dialog.isOpen());
        QVERIFY(!dialog.wasAnswered());
        QVERIFY(!dialog.result().accepted);
        QCOMPARE(finished.count(), 1);
    }

    void aNewRequestForgetsThePreviousAnswer() {
        ConfirmationDialogModel dialog;
        QVERIFY(dialog.start({}));
        dialog.accept();
        QVERIFY(dialog.start({}));
        QVERIFY(!dialog.result().accepted);
        QVERIFY(!dialog.wasAnswered());
    }

    void theModalWaitReturnsTheAnswer() {
        ConfirmationDialogModel dialog;
        QVERIFY(dialog.start({}));
        QTimer::singleShot(0, &dialog, [&dialog]() { dialog.accept(); });
        QVERIFY(runModalDialog(dialog));
        QVERIFY(dialog.result().accepted);
    }

    void theModalWaitEndsWithAnAbandonedRequest() {
        TextInputDialogModel dialog;
        QVERIFY(dialog.start({.title = {}, .label = {}, .initialText = u"name"_s}));
        QTimer::singleShot(0, &dialog, [&dialog]() { dialog.abandon(); });
        QVERIFY(!runModalDialog(dialog));
        QVERIFY(!dialog.result().accepted);
        QVERIFY(dialog.result().text.isEmpty());
    }

    void theModalWaitNeedsAnOpenRequest() {
        ConfirmationDialogModel dialog;
        QTest::ignoreMessage(QtWarningMsg,
                             "Qt Quick UI: a modal dialog was awaited without an open request");
        QVERIFY(!runModalDialog(dialog));
    }

    void theCoordinatorOwnsOneModelPerDialog() {
        DialogCoordinator coordinator;
        QVERIFY(coordinator.confirmation());
        QVERIFY(coordinator.fileReplace());
        QVERIFY(coordinator.resize());
        QVERIFY(coordinator.savePath());
        QVERIFY(coordinator.textInput());
        QVERIFY(coordinator.confirmation()->start({}));
        // The dialogs are independent: another one may open meanwhile.
        QVERIFY(coordinator.fileReplace()->start({}));
    }

    //--- confirmation and text input ------------------------------------------
    void confirmationRejects() {
        ConfirmationDialogModel dialog;
        QVERIFY(dialog.start({}));
        dialog.reject();
        QVERIFY(dialog.wasAnswered());
        QVERIFY(!dialog.result().accepted);
    }

    void textInputAcceptsAnyText() {
        TextInputDialogModel dialog;
        QVERIFY(dialog.start({.title = u"Add folder"_s,
                              .label = u"Folder name:"_s,
                              .initialText = u"New"_s}));
        QCOMPARE(dialog.title(), u"Add folder"_s);
        QCOMPARE(dialog.label(), u"Folder name:"_s);
        QCOMPARE(dialog.initialText(), u"New"_s);
        // An empty name is the caller's to reject, as with QInputDialog.
        dialog.accept(QString());
        QVERIFY(dialog.result().accepted);
        QVERIFY(dialog.result().text.isEmpty());

        QVERIFY(dialog.start({}));
        dialog.accept(u"photos"_s);
        QCOMPARE(dialog.result().text, u"photos"_s);

        QVERIFY(dialog.start({}));
        dialog.reject();
        QVERIFY(!dialog.result().accepted);
    }

    //--- file replace ---------------------------------------------------------
    void fileReplaceDescribesTheCollision_data() {
        QTest::addColumn<int>("mode");
        QTest::addColumn<FileReplaceDialogModel::Collision>("collision");
        QTest::newRow("file") << int(FILE_TO_FILE) << FileReplaceDialogModel::Collision::FileOverFile;
        QTest::newRow("dir") << int(DIR_TO_DIR)
                             << FileReplaceDialogModel::Collision::DirectoryOverDirectory;
        QTest::newRow("file to dir") << int(FILE_TO_DIR)
                                     << FileReplaceDialogModel::Collision::FileOverDirectory;
        QTest::newRow("dir to file") << int(DIR_TO_FILE)
                                     << FileReplaceDialogModel::Collision::DirectoryOverFile;
    }

    void fileReplaceDescribesTheCollision() {
        QFETCH(int, mode);
        QFETCH(FileReplaceDialogModel::Collision, collision);
        FileReplaceDialogModel dialog;
        QVERIFY(dialog.start({.sourcePath = u"C:/a/x"_s,
                              .targetPath = u"C:/b/x"_s,
                              .mode = static_cast<FileReplaceMode>(mode),
                              .multiple = true}));
        QCOMPARE(dialog.collision(), collision);
        QCOMPARE(dialog.source(), u"C:/a/x"_s);
        QCOMPARE(dialog.destination(), u"C:/b/x"_s);
        QVERIFY(dialog.isMultiple());
    }

    void fileReplaceAnswers() {
        FileReplaceDialogModel dialog;
        QVERIFY(dialog.start({.multiple = true}));
        dialog.answer(true, true);
        QCOMPARE(dialog.result().yes, true);
        QCOMPARE(dialog.result().all, true);
        QCOMPARE(dialog.result().cancel, false);

        QVERIFY(dialog.start({.multiple = true}));
        dialog.answer(false, false);
        QCOMPARE(dialog.result().yes, false);
        QCOMPARE(dialog.result().all, false);
        QCOMPARE(dialog.result().cancel, false);

        QVERIFY(dialog.start({.multiple = true}));
        dialog.cancel();
        QCOMPARE(dialog.result().cancel, true);
    }

    void fileReplaceAppliesToAllOnlyForMultipleCollisions() {
        FileReplaceDialogModel dialog;
        QVERIFY(dialog.start({.multiple = false}));
        dialog.answer(true, true);
        QCOMPARE(dialog.result().yes, true);
        QCOMPARE(dialog.result().all, false);
    }

    void fileReplaceDismissSkipsTheItem() {
        FileReplaceDialogModel dialog;
        QVERIFY(dialog.start({.multiple = true}));
        dialog.dismiss();
        QCOMPARE(dialog.result().yes, false);
        QCOMPARE(dialog.result().all, false);
        QCOMPARE(dialog.result().cancel, false);
    }

    void fileReplaceAbandonedCancelsTheOperation() {
        FileReplaceDialogModel dialog;
        QVERIFY(dialog.start({.multiple = true}));
        dialog.abandon();
        QCOMPARE(dialog.result().yes, false);
        QCOMPARE(dialog.result().cancel, true);
    }

    //--- save path ------------------------------------------------------------
    void savePathStartsAtTheSuggestedFile() {
        SavePathDialogModel dialog;
        QVERIFY(dialog.start({.suggestedPath = u"C:/images/photo.tif"_s}, kAllWriterFormats));
        QCOMPARE(dialog.nameFilters().size(), 8);
        QCOMPARE(dialog.nameFilters().at(dialog.selectedFilterIndex()), kTiffFilter);
        QCOMPARE(dialog.suggestedFile(), QUrl::fromLocalFile(u"C:/images/photo.tif"_s));
        QCOMPARE(dialog.suggestedFolder(), QUrl::fromLocalFile(u"C:/images"_s));
    }

    void savePathReportsTheLocalFile() {
        SavePathDialogModel dialog;
        QVERIFY(dialog.start({.suggestedPath = u"C:/images/photo.png"_s}, kAllWriterFormats));
        dialog.accept(QUrl::fromLocalFile(u"C:/images/copy.png"_s));
        QVERIFY(dialog.result().accepted());
        QCOMPARE(dialog.result().path, u"C:/images/copy.png"_s);
    }

    void savePathRejectsRemoteLocations() {
        SavePathDialogModel dialog;
        QVERIFY(dialog.start({.suggestedPath = u"C:/images/photo.png"_s}, kAllWriterFormats));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"no local file"_s));
        dialog.accept(QUrl(u"https://example.com/copy.png"_s));
        QVERIFY(!dialog.isOpen());
        QVERIFY(!dialog.result().accepted());
    }

    void savePathCancelledOrAbandonedReportsNothing() {
        SavePathDialogModel dialog;
        QVERIFY(dialog.start({.suggestedPath = u"C:/images/photo.png"_s}, kAllWriterFormats));
        dialog.reject();
        QVERIFY(!dialog.result().accepted());
        QVERIFY(dialog.start({.suggestedPath = u"C:/images/photo.png"_s}, kAllWriterFormats));
        dialog.abandon();
        QVERIFY(!dialog.result().accepted());
    }

    //--- resize ---------------------------------------------------------------
    void resizeStartsAtTheOriginalSize() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        QCOMPARE(dialog.originalWidth(), kOriginal.width());
        QCOMPARE(dialog.originalHeight(), kOriginal.height());
        QCOMPARE(dialog.targetSize(), kOriginal);
        QVERIFY(dialog.byPercent());
        QCOMPARE(dialog.percent(), 100.0);
        QVERIFY(dialog.keepAspectRatio());
        QCOMPARE(dialog.filterIndex(), int(ResizeDialogModel::Filter::MagicKernelSharp2021));
        QCOMPARE(dialog.commonSizeIndex(), -1);
        QCOMPARE(dialog.commonSizes().size(), kCommonSizeCount);
        QCOMPARE(dialog.commonSizes().at(kFullHdRow), u"1920 x 1080 (FullHD)"_s);
    }

    void resizeByPercent() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.setPercent(25);
        QCOMPARE(dialog.targetSize(), QSize(250, 125));
        dialog.setPercent(dialog.maximumPercent() * 2);
        QCOMPARE(dialog.percent(), dialog.maximumPercent());
        dialog.setPercent(0);
        QCOMPARE(dialog.percent(), dialog.minimumPercent());
        QCOMPARE(dialog.targetSize(), QSize(10, 5));
    }

    void editingASideSelectsTheAbsoluteSize() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.setTargetWidth(2000);
        QVERIFY(!dialog.byPercent());
        QCOMPARE(dialog.targetSize(), QSize(2000, 1000));
        dialog.setTargetHeight(250);
        QCOMPARE(dialog.targetSize(), QSize(500, 250));
    }

    void sidesAreIndependentWithoutTheAspectLock() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.setByPercent(false);
        dialog.setKeepAspectRatio(false);
        dialog.setTargetWidth(300);
        QCOMPARE(dialog.targetSize(), QSize(300, 500));
        dialog.setTargetHeight(700);
        QCOMPARE(dialog.targetSize(), QSize(300, 700));
        // Locking again recomputes from the last edited side.
        dialog.setKeepAspectRatio(true);
        QCOMPARE(dialog.targetSize(), QSize(1400, 700));
    }

    void percentModeAlwaysKeepsTheAspectRatio() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.setKeepAspectRatio(false);
        QVERIFY(dialog.keepAspectRatio());
        dialog.setByPercent(false);
        dialog.setKeepAspectRatio(false);
        dialog.setPercent(50);
        QVERIFY(dialog.byPercent());
        QVERIFY(dialog.keepAspectRatio());
        QCOMPARE(dialog.targetSize(), QSize(500, 250));
    }

    void sidesStayWithinTheLimits() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.setTargetWidth(1);
        // 500 * 0.001 truncates to 0.
        QCOMPARE(dialog.targetSize(), QSize(1, dialog.minimumSide()));
        dialog.setTargetWidth(dialog.maximumSide() + 1);
        QCOMPARE(dialog.targetWidth(), dialog.maximumSide());
    }

    void commonSizesFitTheOriginal() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.selectCommonSize(kFullHdRow);
        QVERIFY(!dialog.byPercent());
        QCOMPARE(dialog.commonSizeIndex(), kFullHdRow);
        QCOMPARE(dialog.targetSize(), QSize(1920, 960));

        dialog.setKeepAspectRatio(false);
        // Changing the lock clears the selected common size.
        QCOMPARE(dialog.commonSizeIndex(), -1);
        dialog.selectCommonSize(kFullHdRow);
        QCOMPARE(dialog.targetSize(), QSize(1920, 1080));

        dialog.selectCommonSize(-1);
        QCOMPARE(dialog.targetSize(), kOriginal);
        QCOMPARE(dialog.commonSizeIndex(), -1);
    }

    void desktopSizes() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.fitDesktop();
        QVERIFY(!dialog.byPercent());
        QCOMPARE(dialog.targetSize(), QSize(1920, 960));
        dialog.fillDesktop();
        QCOMPARE(dialog.targetSize(), QSize(2160, 1080));
        dialog.selectCommonSize(kFullHdRow);
        dialog.reset();
        QCOMPARE(dialog.targetSize(), kOriginal);
        QCOMPARE(dialog.commonSizeIndex(), -1);
    }

    void upscaylNeedsModels() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput({}, true, u"modelA"_s)));
        QVERIFY(!dialog.upscaylAvailable());
        QVERIFY(!dialog.useUpscayl());
        QCOMPARE(dialog.upscaylModelIndex(), -1);
        dialog.setUseUpscayl(true);
        QVERIFY(!dialog.useUpscayl());
    }

    void upscaylRestoresThePreferences() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput({u"modelA"_s, u"modelB"_s}, true, u"modelB"_s)));
        QVERIFY(dialog.upscaylAvailable());
        QVERIFY(dialog.useUpscayl());
        QCOMPARE(dialog.upscaylModelIndex(), 1);
        dialog.reject();
        // An unknown stored model selects the first one.
        QVERIFY(dialog.start(resizeInput({u"modelA"_s}, false, u"gone"_s)));
        QCOMPARE(dialog.upscaylModelIndex(), 0);
        QVERIFY(!dialog.useUpscayl());
    }

    void upscaylAppliesToUpscalesOnly() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput({u"modelA"_s}, true)));
        QVERIFY(!dialog.upscaylApplies());
        dialog.setTargetHeight(501);
        QVERIFY(dialog.upscaylApplies());
        dialog.setTargetHeight(400);
        QVERIFY(!dialog.upscaylApplies());
    }

    void acceptingAnUnchangedSizeReportsNothing() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput({u"modelA"_s}, true)));
        dialog.accept();
        QVERIFY(dialog.wasAnswered());
        QVERIFY(!dialog.result());
        QVERIFY(!dialog.preferencesToStore());
    }

    void acceptingADownscaleSkipsUpscayl() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput({u"modelA"_s}, true, u"modelA"_s)));
        dialog.setPercent(50);
        dialog.setFilterIndex(int(ResizeDialogModel::Filter::Bilinear));
        dialog.accept();
        QVERIFY(dialog.result());
        QCOMPARE(dialog.result()->size, QSize(500, 250));
        QCOMPARE(dialog.result()->filter, QI_FILTER_BILINEAR);
        QVERIFY(!dialog.result()->useUpscayl);
        QCOMPARE(dialog.result()->upscaylModel, u"modelA"_s);
        QVERIFY(dialog.preferencesToStore());
        QVERIFY(!dialog.preferencesToStore()->useUpscayl);
        QCOMPARE(dialog.preferencesToStore()->upscaylModel, u"modelA"_s);
    }

    void acceptingAnUpscaleUsesTheSelectedModel() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput({u"modelA"_s, u"modelB"_s}, false)));
        dialog.setPercent(200);
        dialog.setUseUpscayl(true);
        dialog.setUpscaylModelIndex(1);
        dialog.setFilterIndex(int(ResizeDialogModel::Filter::Nearest));
        dialog.accept();
        QVERIFY(dialog.result());
        QCOMPARE(dialog.result()->size, QSize(2000, 1000));
        QCOMPARE(dialog.result()->filter, QI_FILTER_NEAREST);
        QVERIFY(dialog.result()->useUpscayl);
        QCOMPARE(dialog.result()->upscaylModel, u"modelB"_s);
        QVERIFY(dialog.preferencesToStore()->useUpscayl);
    }

    void filtersMapToTheScalingFilters() {
        ResizeDialogModel dialog;
        const std::array expected{QI_FILTER_NEAREST, QI_FILTER_BILINEAR, QI_FILTER_SMART,
                                  QI_FILTER_MKS2021};
        for (std::size_t index = 0; index < expected.size(); ++index) {
            QVERIFY(dialog.start(resizeInput()));
            dialog.setPercent(50);
            dialog.setFilterIndex(static_cast<int>(index));
            dialog.accept();
            QCOMPARE(dialog.result()->filter, expected[index]);
        }
        // An unknown index selects the default filter.
        QVERIFY(dialog.start(resizeInput()));
        dialog.setFilterIndex(int(expected.size()));
        QCOMPARE(dialog.filterIndex(), int(ResizeDialogModel::Filter::MagicKernelSharp2021));
    }

    void rejectingOrAbandoningAResizeReportsNothing() {
        ResizeDialogModel dialog;
        QVERIFY(dialog.start(resizeInput()));
        dialog.setPercent(50);
        dialog.reject();
        QVERIFY(!dialog.result());
        QVERIFY(!dialog.preferencesToStore());
        QVERIFY(dialog.start(resizeInput()));
        dialog.setPercent(50);
        dialog.abandon();
        QVERIFY(!dialog.result());
    }
};

int runDialogTests(int argc, char **argv) {
    DialogTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_dialogs.moc"
