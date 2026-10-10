#include <QDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "components/batchconverter/batchjobrules.h"
#include "components/printing/imageprintsetup.h"
#include "fakebatchconversionservice.h"
#include "gui/quick/ui/dialogs/batchconverterdialogmodel.h"
#include "gui/quick/ui/dialogs/batchqueuemodel.h"
#include "gui/quick/ui/dialogs/printdialogmodel.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;
using namespace BatchJobRules;

constexpr QSize kSourceSize(40, 20);
constexpr int kSourceInfoTimeoutMs = 5000;
// Row of "1920 x 1080" in BatchJobRules::commonSizes().
constexpr int kFullHdRow = 6;
constexpr int kPngRow = 1;
constexpr int kWebpRow = 2;
constexpr int kBmpRow = 6;
constexpr int kContrastPercent = 150;
constexpr double kHalf = 50.0;
constexpr qreal kHighDpi = 2.0;

// Folder with three PNG sources of kSourceSize.
struct SourceFolder {
    QTemporaryDir dir;
    QStringList paths;

    SourceFolder() {
        QImage image(kSourceSize, QImage::Format_RGB32);
        image.fill(Qt::red);
        for (const QString &name : {u"a.png"_s, u"b.png"_s, u"c.png"_s}) {
            const QString path = dir.filePath(name);
            image.save(path);
            paths.append(path);
        }
    }
};

BatchConverterDialogInput batchInput(const QStringList &paths) {
    return {.filePaths = paths,
            .defaultOutputDirectory = {},
            .originalSize = QSize(1000, 500),
            .saveQualities = {.jpeg = 95, .png = 3, .modern = 80},
            .upscaylModels = {u"x4plus"_s, u"anime"_s},
            .useUpscayl = true,
            .upscaylModel = u"anime"_s,
            .devicePixelRatio = kHighDpi};
}

std::shared_ptr<const QImage> redImage(QSize size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(Qt::red);
    return std::make_shared<const QImage>(std::move(image));
}

PrintDialogInput printInput(const QStringList &printers = {}) {
    return {.image = redImage(QSize(400, 200)),
            .pdfOutputPath = {},
            .printers = printers,
            .defaultPrinter = printers.isEmpty() ? QString() : printers.first(),
            .preferences = {.lastPrinter = {},
                            .pdfDefault = false,
                            .options = {.landscape = false, .color = true, .fitToPage = true}},
            .devicePixelRatio = 1.0};
}
} // namespace

class BatchPrintTests : public QObject {
    Q_OBJECT

private slots:
    //--- batch job rules -----------------------------------------------------
    void qualityScalesFollowTheFormat() {
        const SaveQualityDefaults defaults{.jpeg = 95, .png = 3, .modern = 80};
        QCOMPARE(qualityScaleFor(u"png").kind, QualityKind::Compression);
        QCOMPARE(qualityScaleFor(u"png").maximum, 9);
        QCOMPARE(qualityScaleFor(u"jpg").kind, QualityKind::Quality);
        QCOMPARE(qualityScaleFor(u"avif").minimum, 1);
        QCOMPARE(qualityScaleFor(u"bmp").kind, QualityKind::None);
        QCOMPARE(defaultQualityFor(u"jpg", defaults), 95);
        QCOMPARE(defaultQualityFor(u"png", defaults), 3);
        QCOMPARE(defaultQualityFor(u"jxl", defaults), 80);
        QCOMPARE(defaultQualityFor(u"tif", defaults), -1);
        QVERIFY(qualityToolTip(QualityKind::None).isEmpty());
        QCOMPARE(outputFormats().first().extension, u"jpg"_s);
        QCOMPARE(outputFormats().size(), 8);
    }

    void percentTargetsAreTruncatedAndKeptInRange() {
        QCOMPARE(percentTarget(QSize(1001, 501), kHalf), QSize(500, 250));
        QCOMPARE(percentTarget(QSize(50, 50), kMinimumPercent), QSize(kMinimumSide, kMinimumSide));
        QCOMPARE(percentTarget(QSize(100, 100), kMaximumPercent), QSize(1600, 1600));
    }

    void editedSidesFollowTheAspectRatio() {
        const QSize original(1000, 500);
        QCOMPARE(widthEdited(original, original, 300, true), QSize(300, 150));
        QCOMPARE(widthEdited(original, QSize(1000, 400), 300, false), QSize(300, 400));
        QCOMPARE(heightEdited(original, original, 250, true), QSize(500, 250));
        // An unknown original size does not link the sides.
        QCOMPARE(widthEdited(QSize(), QSize(10, 20), 30, true), QSize(30, 20));
    }

    void resolutionLimitsDependOnUpscayl() {
        QVERIFY(!exceedsResolutionLimits(QSize(12288, 100), false));
        QVERIFY(exceedsResolutionLimits(QSize(12289, 100), false));
        QVERIFY(exceedsResolutionLimits(QSize(10001, 10000), false));
        QVERIFY(!exceedsResolutionLimits(QSize(16384, 100), true));
        QVERIFY(exceedsResolutionLimits(QSize(16385, 100), true));
    }

    void startChecksComeInTheWidgetOrder() {
        QTemporaryDir dir;
        StartCheck check{.outputDirectory = u"  "_s,
                         .pattern = u"../x"_s,
                         .selectedCount = 0,
                         .resize = true,
                         .useUpscayl = false,
                         .targetSize = QSize(20000, 10)};
        QCOMPARE(checkStart(check), StartProblem::InvalidOutputDirectory);
        check.outputDirectory = u" "_s + dir.path() + u" "_s;
        QCOMPARE(checkStart(check), StartProblem::UnsafePattern);
        for (const QString &unsafe : {u"/x"_s, u"\\x"_s, u"C:x"_s, u"a/../b"_s}) {
            check.pattern = unsafe;
            QCOMPARE(checkStart(check), StartProblem::UnsafePattern);
        }
        check.pattern = u"{name}_converted"_s;
        QCOMPARE(checkStart(check), StartProblem::NothingSelected);
        check.selectedCount = 1;
        QCOMPARE(checkStart(check), StartProblem::ResolutionLimitExceeded);
        QVERIFY(startProblemMessage(StartProblem::ResolutionLimitExceeded, check)
                    .text.contains(u"12288 px"_s));
        check.resize = false;
        QCOMPARE(checkStart(check), StartProblem::None);
    }

    void finalJobAppliesTheSections() {
        BatchJobDraft draft;
        draft.job.useUpscayl = true;
        draft.job.doResize = false;
        draft.job.outputDir = u"  C:/out  "_s;
        draft.colorValues[static_cast<std::size_t>(ColorSlider::Contrast)] = kContrastPercent;
        BatchJob job = finalJob(draft);
        QVERIFY(!job.useUpscayl);
        QVERIFY(!job.colorAdjustments.hasAdjustments());
        QCOMPARE(job.outputDir, u"C:/out"_s);

        draft.job.doResize = true;
        draft.colorEnabled = true;
        job = finalJob(draft);
        QVERIFY(job.useUpscayl);
        QCOMPARE(job.colorAdjustments.contrast, 1.5f);
        QCOMPARE(job.colorAdjustments.saturation, 1.0f);
    }

    void outputFolderPrefersAnExistingFolder() {
        QTemporaryDir dir;
        QCOMPARE(initialOutputDirectory(dir.path(), u"C:/a/b.png"_s), QDir(dir.path()).absolutePath());
        QCOMPARE(initialOutputDirectory(dir.filePath(u"missing"_s), u"C:/a/b.png"_s), u"C:/a"_s);
        QVERIFY(initialOutputDirectory({}, {}).isEmpty());
        QCOMPARE(scalingFilters()[defaultScalingFilterIndex()].filter, QI_FILTER_MKS2021);
    }

    //--- queue -----------------------------------------------------------------
    void queueReadsTheHeadersOnAWorker() {
        SourceFolder folder;
        BatchQueueModel queue;
        QSignalSpy completed(&queue, &BatchQueueModel::sourceInfoCompleted);
        queue.reset(folder.paths + QStringList{folder.dir.filePath(u"missing.png"_s)});
        QCOMPARE(queue.rowCount(), 4);
        QCOMPARE(queue.selectedCount(), 4);
        QVERIFY(completed.wait(kSourceInfoTimeoutMs));
        const QString info = queue.data(queue.index(0), BatchQueueModel::SourceInfoRole).toString();
        QVERIFY2(info.startsWith(u"PNG \u2022 40x20 \u2022 "_s), qPrintable(info));
        QCOMPARE(queue.data(queue.index(1), BatchQueueModel::NameRole).toString(), u"b.png"_s);
        QCOMPARE(queue.data(queue.index(0), BatchQueueModel::StateRole).toInt(),
                 static_cast<int>(BatchQueueModel::Pending));
        QVERIFY(queue.selectionText().startsWith(u"4 files selected"_s));
    }

    void queueSelectionCountsFilesAndBytes() {
        SourceFolder folder;
        BatchQueueModel queue;
        QSignalSpy completed(&queue, &BatchQueueModel::sourceInfoCompleted);
        queue.reset(folder.paths);
        QVERIFY(completed.wait(kSourceInfoTimeoutMs));
        queue.setChecked(1, false);
        QCOMPARE(queue.selectedCount(), 2);
        QCOMPARE(queue.selectedRows(), (QList<int>{0, 2}));
        queue.setAllChecked(false);
        QCOMPARE(queue.selectedCount(), 0);
        QCOMPARE(queue.selectionText(), u"0 files selected (0.0 MB)"_s);
        queue.setAllChecked(true);
        QCOMPARE(queue.selectedRows(), (QList<int>{0, 1, 2}));
    }

    void queueDropsTheResultsOfAPreviousQueue() {
        SourceFolder folder;
        BatchQueueModel queue;
        QSignalSpy completed(&queue, &BatchQueueModel::sourceInfoCompleted);
        queue.reset(folder.paths);
        queue.reset({folder.paths.first()});
        QVERIFY(completed.wait(kSourceInfoTimeoutMs));
        QCOMPARE(queue.rowCount(), 1);
        QVERIFY(queue.sourceInfoComplete());
        queue.clear();
        QCOMPARE(queue.rowCount(), 0);
    }

    void queueTakesStatesAndThumbnails() {
        BatchQueueModel queue;
        queue.reset({u"C:/a.png"_s, u"C:/b.png"_s, u"C:/a.png"_s});
        queue.setItemState(1, BatchItemState::Failed, u"Load Error"_s);
        QCOMPARE(queue.data(queue.index(1), BatchQueueModel::DetailsRole).toString(), u"Load Error"_s);
        QCOMPARE(queue.data(queue.index(1), BatchQueueModel::StateTextRole).toString(), u"Failed"_s);
        QImage thumbnail(kSourceSize, QImage::Format_RGB32);
        thumbnail.fill(Qt::blue);
        queue.setThumbnail(u"C:/a.png"_s, thumbnail);
        for (const int row : {0, 2})
            QVERIFY(queue.data(queue.index(row), BatchQueueModel::ThumbnailRole)
                        .value<ThumbnailHandle>()
                        .isValid());
        QVERIFY(!queue.data(queue.index(1), BatchQueueModel::ThumbnailRole)
                     .value<ThumbnailHandle>()
                     .isValid());
    }

    //--- batch converter dialog ------------------------------------------------
    void batchDialogStartsFromTheInput() {
        SourceFolder folder;
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QVERIFY(dialog.start(batchInput(folder.paths), service));
        QVERIFY(dialog.isOpen());
        QCOMPARE(dialog.formatIndex(), 0);
        QCOMPARE(dialog.quality(), 95);
        QVERIFY(dialog.qualityAvailable());
        QVERIFY(!dialog.resizeEnabled());
        QVERIFY(!dialog.byPercent());
        QCOMPARE(dialog.targetWidth(), 1000);
        QVERIFY(dialog.useUpscayl());
        QCOMPARE(dialog.upscaylModelIndex(), 1);
        QCOMPARE(dialog.filters()[dialog.filterIndex()], u"Magic Kernel Sharp 2021"_s);
        QCOMPARE(dialog.outputDirectory(), QDir(folder.dir.path()).absolutePath());
        QCOMPARE(dialog.pattern(), u"{name}_converted"_s);
        QCOMPARE(dialog.statusText(), u"Ready to convert."_s);
        QCOMPARE(dialog.queue()->rowCount(), 3);
        QCOMPARE(service.thumbnailPaths, folder.paths);
        QCOMPARE(service.thumbnailExtent, 96);
        QVERIFY(!dialog.start(batchInput(folder.paths), service));
        dialog.reject();
        QVERIFY(!dialog.result().conversionStarted);
        QVERIFY(!dialog.preferencesToStore());
        QCOMPARE(dialog.queue()->rowCount(), 0);
    }

    void batchDialogWithoutModelsHasNoUpscayl() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        BatchConverterDialogInput input = batchInput({u"C:/a.png"_s});
        input.upscaylModels.clear();
        input.originalSize = QSize();
        QVERIFY(dialog.start(input, service));
        QVERIFY(!dialog.upscaylAvailable());
        QVERIFY(!dialog.useUpscayl());
        QCOMPARE(dialog.upscaylModelIndex(), -1);
        dialog.setUseUpscayl(true);
        QVERIFY(!dialog.useUpscayl());
        QCOMPARE(QSize(dialog.originalWidth(), dialog.originalHeight()), fallbackOriginalSize());
        QVERIFY(dialog.job().upscaylModel.isEmpty());
        dialog.reject();
    }

    void batchQualityFollowsTheFormat() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QVERIFY(dialog.start(batchInput({u"C:/a.png"_s}), service));
        dialog.setFormatIndex(kPngRow);
        QCOMPARE(dialog.quality(), 3);
        QCOMPARE(dialog.qualityMaximum(), 9);
        dialog.setQuality(20);
        QCOMPARE(dialog.quality(), 9);
        dialog.setFormatIndex(kBmpRow);
        QVERIFY(!dialog.qualityAvailable());
        QCOMPARE(dialog.quality(), 9);
        dialog.setFormatIndex(kWebpRow);
        QCOMPARE(dialog.quality(), 80);
        QCOMPARE(dialog.job().format, u"webp"_s);
        dialog.reject();
    }

    void batchResizeRules() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QVERIFY(dialog.start(batchInput({u"C:/a.png"_s}), service));
        dialog.setResizeEnabled(true);
        dialog.setKeepAspectRatio(false);
        dialog.setTargetWidth(300);
        QCOMPARE(QSize(dialog.targetWidth(), dialog.targetHeight()), QSize(300, 500));
        dialog.setByPercent(true);
        QVERIFY(dialog.keepAspectRatio());
        dialog.setKeepAspectRatio(false);
        QVERIFY(dialog.keepAspectRatio());
        dialog.setPercent(kHalf);
        QCOMPARE(QSize(dialog.targetWidth(), dialog.targetHeight()), QSize(500, 250));
        dialog.selectCommonSize(kFullHdRow);
        QVERIFY(!dialog.byPercent());
        QCOMPARE(QSize(dialog.targetWidth(), dialog.targetHeight()), QSize(1920, 1080));
        dialog.setTargetHeight(250);
        QCOMPARE(QSize(dialog.targetWidth(), dialog.targetHeight()), QSize(500, 250));
        dialog.resetSize();
        QCOMPARE(dialog.commonSizeIndex(), 0);
        QCOMPARE(QSize(dialog.targetWidth(), dialog.targetHeight()), QSize(1000, 500));
        dialog.setAspectFitMode(static_cast<int>(AspectFitMode::Height));
        dialog.setRotationIndex(2);
        dialog.setFlipVertical(true);
        const BatchJob job = dialog.job();
        QVERIFY(job.doResize);
        QCOMPARE(job.aspectFitMode, AspectFitMode::Height);
        QCOMPARE(job.rotation, RotationAngle::Rotate180);
        QVERIFY(job.flipVertical);
        QCOMPARE(job.upscaylModel, u"anime"_s);
        dialog.reject();
    }

    void batchColorValuesAreClampedAndReset() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QVERIFY(dialog.start(batchInput({u"C:/a.png"_s}), service));
        const int hue = static_cast<int>(ColorSlider::Hue);
        dialog.setColorValue(hue, 500.0);
        QCOMPARE(dialog.colorValues()[hue], 180.0);
        QVERIFY(!dialog.job().colorAdjustments.hasAdjustments());
        dialog.setColorEnabled(true);
        QCOMPARE(dialog.job().colorAdjustments.hue, 180.0f);
        dialog.resetColorValues();
        QCOMPARE(dialog.colorValues()[hue], 0.0);
        dialog.reject();
    }

    void batchConvertShowsTheFirstProblem() {
        SourceFolder folder;
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QVERIFY(dialog.start(batchInput(folder.paths), service));
        dialog.setOutputDirectory(folder.dir.filePath(u"missing"_s));
        dialog.convert();
        QVERIFY(dialog.messageOpen());
        QVERIFY(dialog.messageIsWarning());
        QCOMPARE(dialog.messageTitle(), u"Invalid Directory"_s);
        QCOMPARE(service.startCount, 0);
        dialog.dismissMessage();
        QVERIFY(!dialog.messageOpen());

        dialog.setOutputDirectoryUrl(QUrl::fromLocalFile(folder.dir.path()));
        dialog.queue()->setAllChecked(false);
        dialog.convert();
        QCOMPARE(dialog.messageTitle(), u"No files"_s);
        QCOMPARE(service.startCount, 0);
        QVERIFY(dialog.isOpen());
        dialog.reject();
    }

    void batchConvertRunsAndFinishes() {
        SourceFolder folder;
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QVERIFY(dialog.start(batchInput(folder.paths), service));
        dialog.queue()->setChecked(1, false);
        dialog.setPattern(u"{name}_small"_s);
        dialog.convert();
        QCOMPARE(service.startCount, 1);
        QCOMPARE(service.startedPaths, folder.paths);
        QCOMPARE(service.startedIndices, (QList<int>{0, 2}));
        QCOMPARE(service.startedJob.format, u"jpg"_s);
        QCOMPARE(service.startedJob.pattern, u"{name}_small"_s);
        QVERIFY(!service.startedJob.useUpscayl);
        QVERIFY(dialog.isRunning());
        QCOMPARE(dialog.progressMaximum(), 2);
        QCOMPARE(dialog.statusText(), u"Processing..."_s);
        // Edits and a second start are ignored while it runs.
        dialog.convert();
        QCOMPARE(service.startCount, 1);

        service.reportProgress(0, BatchItemState::Processing);
        QCOMPARE(dialog.progressValue(), 0);
        service.reportProgress(0, BatchItemState::Done, u"JPG \u2022 40x20"_s);
        service.reportProgress(2, BatchItemState::Failed, u"Load Error"_s);
        QCOMPARE(dialog.progressValue(), 2);
        QCOMPARE(dialog.statusText(), u"Processed 2 / 2 files."_s);
        QCOMPARE(dialog.queue()->data(dialog.queue()->index(2), BatchQueueModel::StateRole).toInt(),
                 static_cast<int>(BatchQueueModel::Failed));

        service.reportFinished(1, 1, 2);
        QVERIFY(!dialog.isRunning());
        QCOMPARE(dialog.statusText(), u"Finished. Success: 1, Failed: 1"_s);
        QVERIFY(dialog.messageOpen());
        QVERIFY(!dialog.messageIsWarning());
        QCOMPARE(dialog.messageTitle(), u"Batch Conversion Complete"_s);

        dialog.dismissMessage();
        dialog.stopOrClose();
        QVERIFY(!dialog.isOpen());
        QVERIFY(dialog.result().conversionStarted);
        const std::optional<BatchConverterPreferences> preferences = dialog.preferencesToStore();
        QVERIFY(preferences);
        QVERIFY(preferences->useUpscayl);
        QCOMPARE(preferences->upscaylModel, u"anime"_s);
    }

    void batchStopWaitsForTheCancellation() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QTemporaryDir dir;
        QVERIFY(dialog.start(batchInput({dir.filePath(u"a.png"_s)}), service));
        dialog.convert();
        QVERIFY(dialog.isRunning());
        dialog.stopOrClose();
        QCOMPARE(service.cancelCount, 1);
        QVERIFY(dialog.isCancelling());
        QCOMPARE(dialog.statusText(), u"Stopping..."_s);
        dialog.stopOrClose();
        QCOMPARE(service.cancelCount, 1);
        QVERIFY(dialog.isOpen());
        service.reportCancelled(0, 0, 1);
        QVERIFY(!dialog.isRunning());
        QVERIFY(!dialog.isCancelling());
        QCOMPARE(dialog.statusText(), u"Stopped by user. Success: 0, Failed: 0"_s);
        dialog.reject();
        QVERIFY(dialog.result().conversionStarted);
    }

    void batchClosingWhileRunningStopsIt() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QTemporaryDir dir;
        QVERIFY(dialog.start(batchInput({dir.filePath(u"a.png"_s)}), service));
        dialog.convert();
        dialog.reject();
        QVERIFY(!dialog.isOpen());
        QCOMPARE(service.cancelCount, 1);
        QVERIFY(!dialog.isRunning());
        QVERIFY(dialog.result().conversionStarted);
        // The service is detached: late reports change nothing.
        service.reportProgress(0, BatchItemState::Done);
        service.reportFinished(1, 0, 1);
        QVERIFY(!dialog.messageOpen());
        QCOMPARE(dialog.progressValue(), 0);
    }

    void batchStartFailureIsShown() {
        BatchConverterDialogModel dialog;
        FakeBatchConversionService service;
        QTemporaryDir dir;
        QVERIFY(dialog.start(batchInput({dir.filePath(u"a.png"_s)}), service));
        dialog.convert();
        service.reportStartFailed(u"No subfolder"_s);
        QVERIFY(!dialog.isRunning());
        QCOMPARE(dialog.statusText(), u"Batch aborted."_s);
        QCOMPARE(dialog.messageTitle(), u"Batch Conversion Failed"_s);
        QCOMPARE(dialog.messageText(), u"No subfolder"_s);
        dialog.reject();
    }

    //--- printing --------------------------------------------------------------
    void printRectFitsAndAlignsTopCentre() {
        const QSizeF page(1000, 2000);
        QCOMPARE(imagePrintRect(page, QSizeF(100, 50), false), QRectF(450, 0, 100, 50));
        QCOMPARE(imagePrintRect(page, QSizeF(100, 50), true), QRectF(0, 0, 1000, 500));
        // An image larger than the page is always scaled down.
        QCOMPARE(imagePrintRect(page, QSizeF(500, 4000), false), QRectF(375, 0, 250, 2000));
    }

    void pdfExportWritesAPdf() {
        QTemporaryDir dir;
        ImagePrintSetup setup;
        setup.setPdfOutputPath(dir.filePath(u"suggested.pdf"_s));
        QCOMPARE(setup.pdfOutputPath(), dir.filePath(u"suggested.pdf"_s));
        QVERIFY(!setup.hasPrinter());
        const QString path = dir.filePath(u"out.pdf"_s);
        QVERIFY(setup.exportPdf(path, *redImage(QSize(64, 32)), {}));
        QFile pdf(path);
        QVERIFY(pdf.open(QIODevice::ReadOnly));
        QVERIFY(pdf.read(4) == "%PDF");
        QVERIFY(!setup.exportPdf(dir.filePath(u"missing/out.pdf"_s), *redImage(QSize(64, 32)), {}));
        QVERIFY(!setup.print(*redImage(QSize(64, 32)), {}));
    }

    void previewShowsTheOrientedPage() {
        ImagePrintSetup setup;
        const QImage image = *redImage(QSize(400, 200));
        const PrintPreviewStyle style{.area = QSize(kPrintPreviewExtent, kPrintPreviewExtent),
                                      .devicePixelRatio = kHighDpi,
                                      .pageFrame = false};
        PrintOptions options{.landscape = false, .color = true, .fitToPage = true};
        QImage page = setup.renderPreview(image, options, style);
        QCOMPARE(page.deviceIndependentSize().height(), kPrintPreviewExtent);
        QVERIFY(page.deviceIndependentSize().width() < kPrintPreviewExtent);
        QCOMPARE(page.devicePixelRatio(), kHighDpi);
        // The image is at the top of the printable area.
        const QPoint imagePixel(page.width() / 2, page.height() / 8);
        QCOMPARE(QColor(page.pixel(imagePixel)), QColor(Qt::red));
        QCOMPARE(QColor(page.pixel(page.width() / 2, page.height() - 2)), QColor(Qt::white));

        options.color = false;
        page = setup.renderPreview(image, options, style);
        const QColor gray(page.pixel(imagePixel));
        QCOMPARE(gray.red(), gray.green());
        QCOMPARE(gray.green(), gray.blue());

        setup.setLandscape(true);
        page = setup.renderPreview(image, options, style);
        QCOMPARE(page.deviceIndependentSize().width(), kPrintPreviewExtent);
        QVERIFY(page.deviceIndependentSize().height() < kPrintPreviewExtent);
    }

    void printDialogWithoutPrintersOffersTheExport() {
        PrintDialogModel dialog;
        QVERIFY(dialog.start(printInput()));
        QVERIFY(!dialog.printersAvailable());
        QCOMPARE(dialog.printerIndex(), -1);
        QVERIFY(dialog.exportFirst());
        QVERIFY(dialog.preview().isValid());
        QVERIFY(!dialog.start(printInput()));
        dialog.setFitToPage(false);
        dialog.setLandscape(true);
        // Nothing to print on: the dialog closes as the widget dialog did.
        dialog.print();
        QVERIFY(!dialog.isOpen());
        const std::optional<PrintPreferences> preferences = dialog.preferencesToStore();
        QVERIFY(preferences);
        QVERIFY(!preferences->pdfDefault);
        QVERIFY(preferences->options.landscape);
        QVERIFY(!preferences->options.fitToPage);
        QVERIFY(preferences->lastPrinter.isEmpty());
        QVERIFY(!dialog.preview().isValid());
    }

    void printDialogExportsAndRemembersIt() {
        QTemporaryDir dir;
        PrintDialogModel dialog;
        PrintDialogInput input = printInput();
        input.pdfOutputPath = dir.filePath(u"photo.pdf"_s);
        QVERIFY(dialog.start(input));
        QCOMPARE(dialog.pdfFileUrl(), QUrl::fromLocalFile(input.pdfOutputPath));
        dialog.exportPdf(QUrl(u"https://example.com/photo.pdf"_s));
        QVERIFY(dialog.isOpen());
        dialog.exportPdf(QUrl::fromLocalFile(dir.filePath(u"missing/photo.pdf"_s)));
        QVERIFY(dialog.isOpen());
        QCOMPARE(dialog.errorText(), u"Could not export the PDF."_s);
        dialog.exportPdf(QUrl::fromLocalFile(input.pdfOutputPath));
        QVERIFY(!dialog.isOpen());
        QVERIFY(QFile::exists(input.pdfOutputPath));
        QVERIFY(dialog.preferencesToStore()->pdfDefault);
    }

    void printDialogPrefersTheLastPrinter() {
        PrintDialogModel dialog;
        PrintDialogInput input = printInput({u"First"_s, u"Second"_s});
        input.preferences.lastPrinter = u"Second"_s;
        input.preferences.pdfDefault = true;
        QVERIFY(dialog.start(input));
        QCOMPARE(dialog.printerIndex(), 1);
        QVERIFY(dialog.exportFirst());
        dialog.selectPrinter(0);
        QCOMPARE(dialog.printerIndex(), 0);
        dialog.reject();
        QCOMPARE(dialog.preferencesToStore()->lastPrinter, u"First"_s);
        QVERIFY(dialog.preferencesToStore()->pdfDefault);

        input.preferences.lastPrinter = u"Removed"_s;
        input.defaultPrinter = u"Second"_s;
        QVERIFY(dialog.start(input));
        QCOMPARE(dialog.printerIndex(), 1);
        dialog.reject();
    }

    void printDialogNeedsAnImage() {
        PrintDialogModel dialog;
        PrintDialogInput input = printInput();
        input.image.reset();
        QVERIFY(!dialog.start(input));
        QVERIFY(!dialog.isOpen());
    }
};

int runBatchPrintTests(int argc, char **argv) {
    BatchPrintTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_batchprint.moc"
