#pragma once

#include <QList>
#include <QStringList>

#include "gui/quick/ui/dialogs/batchconversionservice.h"

// In-memory BatchConversionService of the batch converter tests: records
// what the dialog asks for and lets the test report progress and results as
// BatchConverter would. Shared by qimgv_tests and qimgv_qml_tests.
class FakeBatchConversionService final : public BatchConversionService {
public:
    void start(const QStringList &paths, const QList<int> &selectedIndices,
               const BatchJob &job) override {
        ++startCount;
        startedPaths = paths;
        startedIndices = selectedIndices;
        startedJob = job;
    }

    void cancel() override { ++cancelCount; }

    void requestThumbnails(const QStringList &paths, int extent) override {
        thumbnailPaths = paths;
        thumbnailExtent = extent;
    }

    void reportProgress(int index, BatchItemState state, const QString &details = {}) {
        emit progressUpdated(index, state, details);
    }
    void reportFinished(int succeeded, int failed, int total) {
        emit finished(succeeded, failed, total);
    }
    void reportCancelled(int succeeded, int failed, int total) {
        emit cancelled(succeeded, failed, total);
    }
    void reportStartFailed(const QString &reason) { emit startFailed(reason); }
    void deliverThumbnail(const QString &path, const QImage &image) {
        emit thumbnailReady(path, image);
    }

    int startCount = 0;
    int cancelCount = 0;
    QStringList startedPaths;
    QList<int> startedIndices;
    BatchJob startedJob;
    QStringList thumbnailPaths;
    int thumbnailExtent = 0;
};
