#include "batchconverter.h"
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
#include <QFile>
#include <QRandomGenerator>
#include <QSet>
#include <cmath>
#include <string>
#include <windows.h>
#include "components/upscaler/upscaler.h"

namespace {

constexpr DWORD kReplaceFileFlags = 0;

bool commitTemporaryOutput(const QString &tempPath, const QString &destPath, bool overwrite) {
    const std::wstring nativeTempPath = QDir::toNativeSeparators(tempPath).toStdWString();
    const std::wstring nativeDestPath = QDir::toNativeSeparators(destPath).toStdWString();

    if (!overwrite) {
        return MoveFileExW(nativeTempPath.c_str(), nativeDestPath.c_str(), MOVEFILE_WRITE_THROUGH);
    }

    if (ReplaceFileW(nativeDestPath.c_str(), nativeTempPath.c_str(), nullptr,
                     kReplaceFileFlags, nullptr, nullptr)) {
        return true;
    }

    if (GetLastError() != ERROR_FILE_NOT_FOUND) {
        return false;
    }

    // ReplaceFileW requires an existing destination. The temporary output is
    // in the destination directory, so this fallback remains a same-volume
    // atomic rename and refuses to overwrite a file created during the race.
    return MoveFileExW(nativeTempPath.c_str(), nativeDestPath.c_str(), MOVEFILE_WRITE_THROUGH);
}

QString destinationReservationKey(const QString &path) {
    if (path.isEmpty()) {
        return QString();
    }

    const QString absolutePath = QFileInfo(path).absoluteFilePath();
    const QString normalizedPath = QDir::cleanPath(QDir::fromNativeSeparators(absolutePath));
    return normalizedPath.toCaseFolded();
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
        auto notifyStopped = [this, &notifyFinished]() {
            notifyFinished(QCoreApplication::translate("BatchConverter", "Stopped"), QString(), false);
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

        // "Use Upscayl" only makes sense when the target is actually larger
        // than this source image. A stale/persisted checkbox state must not
        // force every file in a downscale batch through the (much slower,
        // single-threaded) AI path - fall through to the regular resize
        // below instead.
        bool wantsUpscayl = m_job.useUpscayl &&
                             (targetSize.width() > srcImg.width() ||
                              targetSize.height() > srcImg.height());

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
                QImage resized = applyResize(processedImg, targetSize, m_job.keepAspectRatio, m_job.scalingFilter);
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
            QImage resized = applyResize(processedImg, targetSize, m_job.keepAspectRatio, m_job.scalingFilter);
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

        if (m_cancelFlag->load()) {
            notifyStopped();
            return;
        }

        QFileInfo destFi(m_destPath);
        QString tempPath = destFi.absolutePath() + "/.qimgv_tmp_"
                           + QString::number(QCoreApplication::applicationPid()) + "_"
                           + QString::number(m_index) + "_"
                           + QString::number(QRandomGenerator::global()->generate());

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
            QFile::remove(tempPath);
            notifyStopped();
            return;
        }

        if (!saved) {
            QFile::remove(tempPath);
            notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                           QCoreApplication::translate("BatchConverter", "Save Error"), false);
            return;
        }

        if (!m_job.overwrite && QFileInfo::exists(m_destPath)) {
            QFile::remove(tempPath);
            notifyFinished(QCoreApplication::translate("BatchConverter", "Done"),
                           QCoreApplication::translate("BatchConverter", "Skipped (Exists)"), true);
            return;
        }

        const bool committed = commitTemporaryOutput(tempPath, m_destPath, m_job.overwrite);
        if (!committed) {
            QFile::remove(tempPath);
            notifyFinished(QCoreApplication::translate("BatchConverter", "Failed"),
                           QCoreApplication::translate("BatchConverter", "Commit Error"), false);
            return;
        }

        QString detailsStr = QString("%1 \u2022 %2x%3")
                                .arg(m_job.format.toUpper())
                                .arg(processedImg.width())
                                .arg(processedImg.height());
        notifyFinished(QCoreApplication::translate("BatchConverter", "Done"), detailsStr, true);
    }

private:
    QImage applyResize(const QImage &img, const QSize &targetSize, bool keepAspect, int filter) {
        if (img.isNull()) return QImage();
        QSize finalSize = targetSize;
        if (keepAspect) {
            finalSize = img.size().scaled(targetSize, Qt::KeepAspectRatio);
        }
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
