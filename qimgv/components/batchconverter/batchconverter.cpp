#include "batchconverter.h"
#include "utils/imagelib.h"
#include <QImageReader>
#include <QImage>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QDate>
#include <QRunnable>
#include <QCoreApplication>
#include <QThread>
#include <cmath>

#ifdef USE_UPSCAYL
#include "components/upscaler/upscaler.h"
#endif

class BatchConverterRunnable : public QRunnable {
public:
    BatchConverterRunnable(BatchConverter *converter, int index,
                           const QString &srcPath, const QString &destPath,
                           const BatchJob &job,
                           std::shared_ptr<std::atomic<bool>> cancelFlag)
        : m_converter(converter), m_index(index), m_srcPath(srcPath), m_destPath(destPath),
          m_job(job), m_cancelFlag(cancelFlag) {}

    void run() override {
        if (*m_cancelFlag) return;

        QImage srcImg(m_srcPath);
        if (srcImg.isNull()) {
            if (*m_cancelFlag) return;
            QMetaObject::invokeMethod(
                m_converter, "onTaskFinished", Qt::QueuedConnection, Q_ARG(int, m_index),
                Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Failed")),
                Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Load Error")),
                Q_ARG(bool, false));
            return;
        }

        QImage processedImg = srcImg;

        bool colorModified = (std::abs(m_job.brightness) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.contrast - 1.0f) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.saturation - 1.0f) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.temp) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.tint) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.exposure) > ImageLib::kAdjustEpsilon ||
                              std::abs(m_job.hue) > ImageLib::kAdjustEpsilon);
        if (colorModified) {
            if (*m_cancelFlag) return;
            std::shared_ptr<const QImage> srcPtr = std::make_shared<const QImage>(processedImg);
            QImage adj = ImageLib::applyColorAdjustments(
                srcPtr, m_job.exposure, m_job.contrast, m_job.brightness, m_job.temp, m_job.tint, m_job.saturation, m_job.hue);
            if (!adj.isNull()) {
                processedImg = adj;
            }
        }

        if (m_job.useUpscayl) {
            if (*m_cancelFlag) return;
#ifdef USE_UPSCAYL
            if (!UpscaylScaler::getInstance()->init(QCoreApplication::applicationDirPath())) {
                if (*m_cancelFlag) return;
                QMetaObject::invokeMethod(
                    m_converter, "onTaskFinished", Qt::QueuedConnection, Q_ARG(int, m_index),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Failed")),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverter", "AI Model Error")),
                    Q_ARG(bool, false));
                return;
            }
            QImage upscaled = UpscaylScaler::getInstance()->upscale(processedImg);
            if (upscaled.isNull()) {
                if (*m_cancelFlag) return;
                QMetaObject::invokeMethod(
                    m_converter, "onTaskFinished", Qt::QueuedConnection, Q_ARG(int, m_index),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Failed")),
                    Q_ARG(QString, QCoreApplication::translate("BatchConverter", "AI Upscaling Failed")),
                    Q_ARG(bool, false));
                return;
            }
            processedImg = upscaled;
#endif
            if (m_job.doResize && processedImg.size() != m_job.targetSize) {
                if (*m_cancelFlag) return;
                processedImg = applyResize(processedImg, m_job.targetSize, m_job.keepAspectRatio, m_job.scalingFilter);
            }
        } else if (m_job.doResize) {
            if (*m_cancelFlag) return;
            processedImg = applyResize(processedImg, m_job.targetSize, m_job.keepAspectRatio, m_job.scalingFilter);
        }

        if (*m_cancelFlag) return;
        QByteArray formatBa = m_job.format.toLatin1();
        bool saved = processedImg.save(m_destPath, formatBa.constData(), m_job.quality);
        QString detailsStr = QString("%1 \u2022 %2x%3")
                                .arg(m_job.format.toUpper())
                                .arg(processedImg.width())
                                .arg(processedImg.height());

        if (saved) {
            QMetaObject::invokeMethod(
                m_converter, "onTaskFinished", Qt::QueuedConnection, Q_ARG(int, m_index),
                Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Done")),
                Q_ARG(QString, detailsStr), Q_ARG(bool, true));
        } else {
            QMetaObject::invokeMethod(
                m_converter, "onTaskFinished", Qt::QueuedConnection, Q_ARG(int, m_index),
                Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Failed")),
                Q_ARG(QString, QCoreApplication::translate("BatchConverter", "Save Error")),
                Q_ARG(bool, false));
        }
    }

private:
    QImage applyResize(const QImage &img, const QSize &targetSize, bool keepAspect, int filter) {
        if (img.isNull()) return img;
        QSize finalSize = targetSize;
        if (keepAspect) {
            finalSize = img.size().scaled(targetSize, Qt::KeepAspectRatio);
        }
        std::shared_ptr<const QImage> imgPtr = std::make_shared<const QImage>(img);
        QImage scaledImg = ImageLib::scaled(imgPtr, finalSize, static_cast<ScalingFilter>(filter));
        return scaledImg.isNull() ? img : scaledImg;
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
    cancel();
}

void BatchConverter::start(const QStringList &allPaths, const QList<int> &selectedIndices, const BatchJob &job) {
    if (m_isConverting) return;

    m_isConverting = true;
    *m_cancelFlag = false;
    m_totalFiles = selectedIndices.size();
    m_processedFiles = 0;
    m_successCount = 0;
    m_failedCount = 0;

    QString finalOutDir = job.outputDir;
    if (job.createSubfolder) {
        QString subfolderName = "Batch_" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QDir baseDir(job.outputDir);
        if (baseDir.mkdir(subfolderName)) {
            finalOutDir = job.outputDir + "/" + subfolderName;
        }
    }

    if (job.useUpscayl) {
        m_threadPool.setMaxThreadCount(1);
    } else {
        m_threadPool.setMaxThreadCount(QThread::idealThreadCount());
    }

    int activeIndex = 0;
    for (int i : selectedIndices) {
        if (i < 0 || i >= allPaths.size()) continue;

        QString srcPath = allPaths[i];
        QFileInfo srcFi(srcPath);

        QString destPath = buildDestPath(srcPath, job.pattern, activeIndex + 1, job.format, finalOutDir);

        if (destPath.isEmpty()) {
            onTaskFinished(i, tr("Failed"), tr("Invalid destination path"), false);
            continue;
        }

        if (!job.overwrite && QFileInfo::exists(destPath)) {
            onTaskFinished(i, tr("Done"), tr("Skipped (Exists)"), true);
            continue;
        }

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
    if (!m_isConverting) return;
    *m_cancelFlag = true;
    m_threadPool.clear();
    m_threadPool.waitForDone();
    m_isConverting = false;
}

void BatchConverter::onTaskFinished(int index, QString status, QString details, bool success) {
    if (success) {
        m_successCount++;
    } else {
        m_failedCount++;
    }
    m_processedFiles++;

    emit progressUpdated(index, status, details, success);

    if (m_processedFiles >= m_totalFiles || *m_cancelFlag) {
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
