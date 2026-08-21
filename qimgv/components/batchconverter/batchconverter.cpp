#include "batchconverter.h"
#include "utils/fileoperations.h"
#include "utils/imagelib.h"
#include "utils/pngwriter.h"
#include <QImageReader>
#include <QImage>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QDate>
#include <QRunnable>
#include <QCoreApplication>
#include <QThread>
#include <QSet>
#include <cmath>
#include <utility>
#include "components/upscaler/upscaler.h"

namespace {

QString retainedArtifactDetails(const ImageSaveResult &result) {
    QStringList details;
    if (!result.retainedBackupPath.isEmpty()) {
        details.append(QCoreApplication::translate(
                           "BatchConverter",
                           "Original backup retained at:\n%1")
                           .arg(QDir::toNativeSeparators(result.retainedBackupPath)));
    }
    if (!result.retainedTemporaryPath.isEmpty()) {
        const QString nativeTemporaryPath =
            QDir::toNativeSeparators(result.retainedTemporaryPath);
        if (result.cleanupSucceeded()) {
            details.append(QCoreApplication::translate(
                               "BatchConverter", "Staged output retained at:\n%1")
                               .arg(nativeTemporaryPath));
        } else {
            details.append(QCoreApplication::translate(
                               "BatchConverter",
                               "Temporary output could not be removed. Retained at:\n%1")
                               .arg(nativeTemporaryPath));
        }
    }
    return details.join(QLatin1Char('\n'));
}

QString operationDetails(const QString &operation, const ImageSaveResult &result) {
    QStringList details{operation};
    if (result.nativeError != 0) {
        details.append(QCoreApplication::translate("BatchConverter", "Windows error: %1")
                           .arg(result.nativeError));
    }

    const QString artifactDetails = retainedArtifactDetails(result);
    if (!artifactDetails.isEmpty())
        details.append(artifactDetails);
    return details.join(QLatin1Char('\n'));
}

QString destinationReservationKey(const QString &path) {
    if (path.isEmpty()) {
        return QString();
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    const QString normalizedPath = QDir::cleanPath(QDir::fromNativeSeparators(absolutePath));
    return normalizedPath.toCaseFolded();
}

// Resolves the final per-file output size for a given bounding box, honoring
// the selected AspectFitMode. Computed individually for every source image,
// since files in a batch can have different aspect ratios.
QSize computeAspectFitSize(const QSize &srcSize, const QSize &boundingSize,
                           bool keepAspectRatio, AspectFitMode mode) {
    if (!keepAspectRatio) {
        return boundingSize;
    }

    switch (mode) {
    case AspectFitMode::Width: {
        if (srcSize.width() <= 0) {
            return boundingSize;
        }
        const double scale = static_cast<double>(boundingSize.width()) / srcSize.width();
        return QSize(boundingSize.width(), qRound(srcSize.height() * scale));
    }
    case AspectFitMode::Height: {
        if (srcSize.height() <= 0) {
            return boundingSize;
        }
        const double scale = static_cast<double>(boundingSize.height()) / srcSize.height();
        return QSize(qRound(srcSize.width() * scale), boundingSize.height());
    }
    case AspectFitMode::Auto:
    default:
        return srcSize.scaled(boundingSize, Qt::KeepAspectRatio);
    }
}

} // namespace

class BatchConverterRunnable : public QRunnable {
public:
    BatchConverterRunnable(BatchConverter *converter, int index,
                           const QString &srcPath, const QString &destPath,
                           const BatchJob &job,
                           std::shared_ptr<std::atomic<bool>> cancelFlag)
        : m_converter(converter), m_index(index), m_srcPath(srcPath), m_destPath(destPath),
          m_job(job), m_cancelFlag(cancelFlag) {
        m_converter->onRunnableCreated();
    }

    ~BatchConverterRunnable() override {
        m_converter->onRunnableDestroyed();
    }

    void run() override {
        auto notifyFinished = [this](const QString &status, const QString &details, bool success) {
            QMetaObject::invokeMethod(
                m_converter, "onTaskFinished", Qt::QueuedConnection, Q_ARG(int, m_index),
                Q_ARG(QString, status), Q_ARG(QString, details), Q_ARG(bool, success));
        };
        auto notifyStopped = [this, &notifyFinished](const QString &details = QString()) {
            notifyFinished(QCoreApplication::translate("BatchConverter", "Stopped"), details, false);
        };

        if (m_cancelFlag->load()) {
            notifyStopped();
            return;
        }

        QImage srcImg(m_srcPath);
        if (srcImg.isNull()) {
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                           QCoreApplication::translate("BatchConverter", "Load Error"), false);
            return;
        }

        QImage processedImg = srcImg;

        QSize targetSize = m_job.targetSize;
        if (m_job.resizeByPercent) {
            double scale = m_job.resizePercent / 100.0;
            targetSize = QSize(qRound(srcImg.width() * scale), qRound(srcImg.height() * scale));
        }

        bool colorModified = (std::abs(m_job.brightness) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.contrast - 1.0f) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.saturation - 1.0f) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.temp) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.tint) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.exposure) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.hue) > ImageLib::kAdjustEpsilon);
        if (colorModified) {
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            std::shared_ptr<const QImage> srcPtr = std::make_shared<const QImage>(processedImg);
            QImage adj = ImageLib::applyColorAdjustments(
                srcPtr, m_job.exposure, m_job.contrast, m_job.brightness, m_job.temp, m_job.tint, m_job.saturation, m_job.hue);
            if (!adj.isNull()) {
                processedImg = adj;
            }
        }

        // "Upscayl" only makes sense when the target is actually larger
        // than this source image. A stale/persisted checkbox state must not
        // force every file in a downscale batch through the (much slower,
        // single-threaded) AI path - fall through to the regular resize
        // below instead.
        QSize finalTarget = computeAspectFitSize(srcImg.size(), targetSize,
                                                 m_job.keepAspectRatio, m_job.aspectFitMode);
        bool wantsUpscayl = m_job.useUpscayl &&
                             (finalTarget.width() > srcImg.width() ||
                              finalTarget.height() > srcImg.height());

        if (wantsUpscayl) {
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            const UpscaylInferenceResult inferenceResult =
                UpscaylScaler::getInstance()->upscale(
                    UpscaylInferenceRequest{QCoreApplication::applicationDirPath(),
                                            m_job.upscaylModel, processedImg,
                                            m_cancelFlag.get()});
            if (inferenceResult.error == UpscaylInferenceError::ModelLoadFailed) {
                if (m_cancelFlag->load()) {
                    notifyStopped();
                    return;
                }
                notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                               QCoreApplication::translate("BatchConverter", "AI Model Error"), false);
                return;
            }

            QImage upscaled = inferenceResult.image;
            if (upscaled.isNull()) {
                if (m_cancelFlag->load()) {
                    notifyStopped();
                    return;
                }
                notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                               QCoreApplication::translate("BatchConverter", "AI Upscaling Failed"), false);
                return;
            }
            processedImg = upscaled;
            if (m_job.doResize && processedImg.size() != targetSize) {
                if (m_cancelFlag->load()) {
                    notifyStopped();
                    return;
                }
                QImage resized = applyResize(processedImg, targetSize, m_job.keepAspectRatio,
                                             m_job.aspectFitMode, m_job.scalingFilter);
                if (resized.isNull()) {
                    if (m_cancelFlag->load()) {
                        notifyStopped();
                        return;
                    }
                    notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                                   QCoreApplication::translate("BatchConverter", "Resize Error"), false);
                    return;
                }
                processedImg = resized;
            }
        } else if (m_job.doResize) {
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            QImage resized = applyResize(processedImg, targetSize, m_job.keepAspectRatio,
                                         m_job.aspectFitMode, m_job.scalingFilter);
            if (resized.isNull()) {
                if (m_cancelFlag->load()) {
                    notifyStopped();
                    return;
                }
                notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                               QCoreApplication::translate("BatchConverter", "Resize Error"), false);
                return;
            }
            processedImg = resized;
        }

        if (m_job.rotation != RotationAngle::Rotate0) {
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            QImage rotatedImg = ImageLib::rotatedRaw(&processedImg, static_cast<int>(m_job.rotation));
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            if (rotatedImg.isNull()) {
                notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                               QCoreApplication::translate("BatchConverter", "Transform Error"),
                               false);
                return;
            }
            processedImg = std::move(rotatedImg);
        }

        if (m_job.flipHorizontal || m_job.flipVertical) {
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            QImage flippedImg = processedImg.mirrored(m_job.flipHorizontal,
                                                       m_job.flipVertical);
            if (m_cancelFlag->load()) {
                notifyStopped();
                return;
            }
            if (flippedImg.isNull()) {
                notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                               QCoreApplication::translate("BatchConverter", "Transform Error"),
                               false);
                return;
            }
            processedImg = std::move(flippedImg);
        }

        if (m_cancelFlag->load()) {
            notifyStopped();
            return;
        }

        AtomicFileRequest fileRequest;
        fileRequest.destinationPath = m_destPath;
        fileRequest.existingDestinationPolicy =
            m_job.overwrite ? ExistingDestinationPolicy::Replace
                            : ExistingDestinationPolicy::Preserve;
        AtomicFileTransaction stagedOutput(std::move(fileRequest));
        if (!stagedOutput.creationResult().succeeded()) {
            notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                           operationDetails(
                               QCoreApplication::translate("BatchConverter", "Save Error"),
                               stagedOutput.creationResult()),
                           false);
            return;
        }

        const QString &tempPath = stagedOutput.temporaryPath();

        QByteArray formatBa = m_job.format.toLatin1();
        bool saved = false;
        if (formatBa.toUpper() == "PNG") {
            int level = (m_job.quality == 0) ? 0 : qBound(1, (m_job.quality * 12) / 9, 12);
            saved = savePngWithLibdeflate(processedImg, tempPath, level);
            if (!saved) {
                saved = processedImg.save(tempPath, formatBa.constData(), m_job.quality);
            }
        } else {
            saved = processedImg.save(tempPath, formatBa.constData(), m_job.quality);
        }

        if (m_cancelFlag->load()) {
            notifyStopped(retainedArtifactDetails(stagedOutput.discard()));
            return;
        }

        if (!saved) {
            const ImageSaveResult cleanupResult = stagedOutput.discard();
            notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                           operationDetails(
                               QCoreApplication::translate("BatchConverter", "Save Error"),
                               cleanupResult),
                           false);
            return;
        }

        if (!m_job.overwrite && QFileInfo::exists(m_destPath)) {
            const ImageSaveResult cleanupResult = stagedOutput.discard();
            const QString skippedDetails = operationDetails(
                QCoreApplication::translate("BatchConverter", "Skipped (Exists)"), cleanupResult);
            if (!cleanupResult.cleanupSucceeded()) {
                notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                               skippedDetails, false);
            } else {
                notifyFinished(QCoreApplication::translate("BatchConverter", "Done"),
                               skippedDetails, true);
            }
            return;
        }

        const ImageSaveResult commitResult = stagedOutput.commit();
        if (!commitResult.succeeded()) {
            notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                           operationDetails(
                               QCoreApplication::translate("BatchConverter", "Commit Error"),
                               commitResult),
                           false);
            return;
        }

        QString detailsStr = QString("%1 \u2022 %2x%3")
                                .arg(m_job.format.toUpper())
                                .arg(processedImg.width())
                                .arg(processedImg.height());
        const QString artifactDetails = retainedArtifactDetails(commitResult);
        if (!artifactDetails.isEmpty()) {
            detailsStr.append(QLatin1Char('\n'));
            detailsStr.append(artifactDetails);
        }
        notifyFinished(QCoreApplication::translate("BatchConverter", "Done"), detailsStr, true);
    }

private:
    QImage applyResize(const QImage &img, const QSize &targetSize, bool keepAspect,
                       AspectFitMode fitMode, int filter) {
        if (img.isNull()) return QImage();
        QSize finalSize = computeAspectFitSize(img.size(), targetSize, keepAspect, fitMode);
        std::shared_ptr<const QImage> imgPtr = std::make_shared<const QImage>(img);
        return ImageLib::scaled(imgPtr, finalSize, static_cast<ScalingFilter>(filter));
    }

    BatchConverter *m_converter;
    int m_index;
    QString m_srcPath;
    QString m_destPath;
    BatchJob m_job;
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;
};

BatchConverter::BatchConverter(QObject *parent)
    : QObject(parent), m_cancelFlag(std::make_shared<std::atomic<bool>>(false)) {
    m_threadPool.setMaxThreadCount(QThread::idealThreadCount());
}

BatchConverter::~BatchConverter() {
    cancelAndWait();
}

void BatchConverter::start(const QStringList &allPaths, const QList<int> &selectedIndices, const BatchJob &job) {
    if (m_isConverting) return;

    m_isConverting = true;
    m_isCancelling = false;
    *m_cancelFlag = false;
    m_totalFiles = selectedIndices.size();
    m_processedFiles = 0;
    m_successCount = 0;
    m_failedCount = 0;
    m_inFlightTasksCount = 0;

    QString finalOutDir = job.outputDir;
    if (job.createSubfolder) {
        const QString subfolderPath = createUniqueSubfolder(job.outputDir);
        if (subfolderPath.isEmpty()) {
            m_isConverting = false;
            emit startFailed(tr("Could not create a unique batch subfolder in \"%1\". The batch was aborted.").arg(job.outputDir));
            return;
        }
        finalOutDir = subfolderPath;
    }

    // The AI model runs single-threaded, so the pool is capped to 1 thread
    // whenever Upscayl might actually run for some file in this batch. But a
    // percent-mode job at <=100% is guaranteed to be a downscale/no-op for
    // every file (same ratio applied to every source image), so Upscayl can
    // never trigger there - keep full parallelism in that case instead of
    // serializing the whole batch because of a stale checkbox state.
    bool upscaylMayRun = job.useUpscayl && job.doResize &&
                          !(job.resizeByPercent && job.resizePercent <= 100.0);
    m_threadPool.setMaxThreadCount(upscaylMayRun ? 1 : QThread::idealThreadCount());

    QSet<QString> reservedDestinationKeys;
    int activeIndex = 0;
    for (int i : selectedIndices) {
        if (i < 0 || i >= allPaths.size()) continue;

        QString srcPath = allPaths[i];

        QString rawDestPath = buildDestPath(srcPath, job.pattern, activeIndex + 1, job.format, finalOutDir);

        if (rawDestPath.isEmpty()) {
            onTaskFinished(i, tr("Failed"), tr("Invalid destination path"), false);
            continue;
        }

        const QString rawDestinationKey = destinationReservationKey(rawDestPath);
        if (rawDestinationKey.isEmpty()) {
            onTaskFinished(i, tr("Failed"), tr("Destination planning failed: invalid reservation key"), false);
            continue;
        }

        if (!job.overwrite && QFileInfo::exists(rawDestPath)
            && !reservedDestinationKeys.contains(rawDestinationKey)) {
            onTaskFinished(i, tr("Done"), tr("Skipped (Exists)"), true);
            continue;
        }

        const QString destPath = makeUniqueDestPath(rawDestPath, reservedDestinationKeys, job.overwrite);
        if (destPath.isEmpty()) {
            onTaskFinished(i, tr("Failed"),
                           tr("Destination planning failed: no unique output path is available"), false);
            continue;
        }

        if (!job.overwrite && QFileInfo::exists(destPath)) {
            onTaskFinished(i, tr("Done"), tr("Skipped (Exists)"), true);
            continue;
        }

        const QString destinationKey = destinationReservationKey(destPath);
        if (destinationKey.isEmpty()) {
            onTaskFinished(i, tr("Failed"), tr("Destination planning failed: invalid reservation key"), false);
            continue;
        }
        reservedDestinationKeys.insert(destinationKey);

        emit progressUpdated(i, tr("Processing..."), "", true);

        // Start runnable
        BatchConverterRunnable *runnable = new BatchConverterRunnable(
            this, i, srcPath, destPath, job, m_cancelFlag
        );
        m_threadPool.start(runnable);
        activeIndex++;
    }

    if (m_processedFiles >= m_totalFiles) {
        m_isConverting = false;
        emit finished(m_successCount, m_failedCount, m_totalFiles);
    }
}

void BatchConverter::cancel() {
    if (!m_isConverting || m_isCancelling) return;
    m_isCancelling = true;
    *m_cancelFlag = true;
    m_threadPool.clear();
    if (m_inFlightTasksCount == 0) {
        finishCancellation();
    }
}

void BatchConverter::cancelAndWait() {
    if (m_isConverting || m_isCancelling) {
        *m_cancelFlag = true;
        m_isCancelling = true;
        m_threadPool.clear();
    }
    m_threadPool.waitForDone();
    m_isConverting = false;
    m_isCancelling = false;
}

void BatchConverter::finishCancellation() {
    if (!m_isConverting && !m_isCancelling) return;
    m_isConverting = false;
    m_isCancelling = false;
    emit cancelled(m_successCount, m_failedCount, m_totalFiles);
}

void BatchConverter::onRunnableCreated() {
    m_inFlightTasksCount++;
}

void BatchConverter::onRunnableDestroyed() {
    if (--m_inFlightTasksCount == 0) {
        if (m_isCancelling) {
            QMetaObject::invokeMethod(this, "finishCancellation", Qt::QueuedConnection);
        }
        if (m_selfDestructOnFinished) {
            if (!m_deleteLaterCalled.exchange(true)) {
                QMetaObject::invokeMethod(this, "deleteLater", Qt::QueuedConnection);
            }
        }
    }
}

void BatchConverter::enableSelfDestruct() {
    m_selfDestructOnFinished = true;
    if (m_inFlightTasksCount == 0) {
        if (!m_deleteLaterCalled.exchange(true)) {
            QMetaObject::invokeMethod(this, "deleteLater", Qt::QueuedConnection);
        }
    }
}

void BatchConverter::onTaskFinished(int index, QString status, QString details, bool success) {
    if (!m_isConverting && !m_isCancelling) return;

    if (m_isCancelling) {
        if (!details.isEmpty())
            emit progressUpdated(index, status, details, success);
        return;
    }

    if (success) {
        m_successCount++;
    } else {
        m_failedCount++;
    }
    m_processedFiles++;

    emit progressUpdated(index, status, details, success);

    if (m_processedFiles >= m_totalFiles) {
        m_isConverting = false;
        emit finished(m_successCount, m_failedCount, m_totalFiles);
    }
}

QString BatchConverter::buildDestPath(const QString &srcPath, const QString &pattern, int index, const QString &formatExt, const QString &finalOutDir) const {
    QFileInfo srcFi(srcPath);
    QString safeBaseName = srcFi.baseName();
    safeBaseName.replace("/", "_");
    safeBaseName.replace("\\", "_");

    QString targetName = pattern;
    targetName.replace("{name}", safeBaseName);
    targetName.replace("{ext}", formatExt);
    targetName.replace("{date}", QDate::currentDate().toString("yyyy-MM-dd"));
    targetName.replace("{index}", QString::number(index));

    if (!targetName.contains(".")) {
        targetName += "." + formatExt;
    }

    QString canonicalOut = QDir(finalOutDir).canonicalPath();
    if (canonicalOut.isEmpty()) {
        canonicalOut = QDir(finalOutDir).absolutePath();
    }
    QString full = QDir::cleanPath(canonicalOut + "/" + targetName);
    if (!full.startsWith(canonicalOut + "/")) {
        return QString();
    }

    return full;
}

QString BatchConverter::createUniqueSubfolder(const QString &baseDir) const {
    constexpr int kMaxSubfolderAttempts = 1000;

    QDir dir(baseDir);
    const QString baseName = "Batch_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");

    for (int attempt = 0; attempt < kMaxSubfolderAttempts; ++attempt) {
        const QString candidateName = (attempt == 0) ? baseName : baseName + "_" + QString::number(attempt);

        if (dir.mkdir(candidateName)) {
            return baseDir + "/" + candidateName;
        }

        if (!QFileInfo::exists(baseDir + "/" + candidateName)) {
            // mkdir failed for a reason other than the name being taken
            // (permissions, invalid path, read-only filesystem, etc.) - not retryable.
            return QString();
        }
        // Name collision with a previous batch - try the next suffixed candidate.
    }

    return QString();
}

QString BatchConverter::makeUniqueDestPath(const QString &destPath,
                                           const QSet<QString> &reservedDestinationKeys,
                                           bool overwrite) const {
    if (destPath.isEmpty()) {
        return QString();
    }

    const QString destinationKey = destinationReservationKey(destPath);
    if (destinationKey.isEmpty()) {
        return QString();
    }

    if (!reservedDestinationKeys.contains(destinationKey)) {
        if (overwrite || !QFileInfo::exists(destPath)) {
            return destPath;
        }
    }

    QFileInfo fi(destPath);
    QString dir = fi.absolutePath();
    QString baseName = fi.completeBaseName();
    QString suffix = fi.suffix();
    if (!suffix.isEmpty()) {
        suffix = "." + suffix;
    }

    constexpr int kMaxDisambiguationAttempts = 10000;
    for (int counter = 1; counter < kMaxDisambiguationAttempts; ++counter) {
        const QString candidate = QDir::cleanPath(dir + "/" + baseName + "_" + QString::number(counter) + suffix);
        const QString candidateKey = destinationReservationKey(candidate);
        if (!candidateKey.isEmpty() && !reservedDestinationKeys.contains(candidateKey)) {
            if (overwrite || !QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
    }

    return QString();
}
