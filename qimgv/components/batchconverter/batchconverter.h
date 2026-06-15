#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSize>
#include <QThreadPool>
#include <QList>
#include <atomic>
#include <memory>

struct BatchJob {
    QString format;
    int quality = 90;
    bool doResize = false;
    QSize targetSize;
    bool keepAspectRatio = true;
    bool useUpscayl = false;
    QString upscaylModel;
    int scalingFilter = 0;
    float exposure = 0.0f;
    float contrast = 1.0f;
    float brightness = 0.0f;
    float temp = 0.0f;
    float tint = 0.0f;
    float saturation = 1.0f;
    float hue = 0.0f;
    QString pattern;
    bool overwrite = false;
    QString outputDir;
    bool createSubfolder = false;
};

class BatchConverter : public QObject {
    Q_OBJECT

public:
    explicit BatchConverter(QObject *parent = nullptr);
    ~BatchConverter();

    void start(const QStringList &allPaths, const QList<int> &selectedIndices, const BatchJob &job);
    void cancel();

    bool isRunning() const { return m_isConverting; }

signals:
    void progressUpdated(int index, QString status, QString details, bool success);
    void finished(int successCount, int failedCount, int totalCount);

private slots:
    void onTaskFinished(int index, QString status, QString details, bool success);

private:
    QString buildDestPath(const QString &srcPath, const QString &pattern, int index, const QString &formatExt, const QString &finalOutDir) const;

    QThreadPool m_threadPool;
    std::atomic<bool> m_isConverting{false};
    std::shared_ptr<std::atomic<bool>> m_cancelFlag;

    int m_totalFiles = 0;
    int m_processedFiles = 0;
    int m_successCount = 0;
    int m_failedCount = 0;
};
