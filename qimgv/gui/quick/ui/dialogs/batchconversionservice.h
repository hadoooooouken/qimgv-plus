#pragma once

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "components/batchconverter/batchjob.h"

// What the Qt Quick batch converter dialog needs from the application: the
// conversion itself (BatchConverter) and the thumbnails of the queued files
// (Thumbnailer). The application provides one service per dialog request and
// destroys it after the dialog closed; destroying a running service stops
// its batch.
//
// GUI thread only; the signals arrive on the GUI thread.
class BatchConversionService : public QObject {
    Q_OBJECT

public:
    ~BatchConversionService() override;

    // Converts the files at selectedIndices of paths; progressUpdated
    // reports each by its index in paths. finished, cancelled or startFailed
    // ends the batch (startFailed and finished may be emitted before start()
    // returns).
    virtual void start(const QStringList &paths, const QList<int> &selectedIndices,
                       const BatchJob &job) = 0;
    // Stops the running batch; cancelled follows once its workers stopped.
    virtual void cancel() = 0;
    // Square thumbnails of extent device pixels, delivered by thumbnailReady.
    virtual void requestThumbnails(const QStringList &paths, int extent) = 0;

signals:
    void progressUpdated(int index, BatchItemState state, QString details);
    void finished(int succeeded, int failed, int total);
    void cancelled(int succeeded, int failed, int total);
    void startFailed(QString reason);
    void thumbnailReady(QString path, QImage image);

protected:
    explicit BatchConversionService(QObject *parent = nullptr);
};
