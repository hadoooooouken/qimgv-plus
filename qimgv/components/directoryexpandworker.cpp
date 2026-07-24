#include "directoryexpandworker.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QSet>

DirectoryExpandWorker::DirectoryExpandWorker(QList<SelectedEntry> entries, QString formatsRegex)
    : entries(std::move(entries)), formatsRegex(std::move(formatsRegex)) {
}

bool DirectoryExpandWorker::isWorkerRunning() const {
    return isRunning.load(std::memory_order_relaxed);
}

void DirectoryExpandWorker::requestStop() {
    isRunning.store(false, std::memory_order_relaxed);
}

void DirectoryExpandWorker::run() {
    isRunning.store(true, std::memory_order_relaxed);

    QRegularExpression regex(formatsRegex, QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) {
        emit error(QStringLiteral("DirectoryExpandWorker: invalid supported-formats regex, aborting scan"));
        emit finished();
        return;
    }

    QSet<QString> visited;
    QList<QString> batch;
    batch.reserve(MAX_BATCH_SIZE);

    auto flushBatch = [this, &batch]() {
        if (!batch.isEmpty()) {
            emit pathsReady(batch);
            batch.clear();
        }
    };

    auto tryAdd = [&visited, &batch, &flushBatch](const QString &filePath) {
        if (visited.contains(filePath))
            return;
        visited.insert(filePath);
        batch << filePath;
        if (batch.size() >= MAX_BATCH_SIZE)
            flushBatch();
    };

    for (const SelectedEntry &entry : std::as_const(entries)) {
        if (!isWorkerRunning())
            break;

        if (!entry.isDirectory) {
            tryAdd(entry.path);
            continue;
        }

        QDirIterator it(entry.path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (!isWorkerRunning())
                break;
            QString filePath = QDir::fromNativeSeparators(it.next());
            if (regex.match(it.fileName()).hasMatch())
                tryAdd(filePath);
        }
    }

    flushBatch();
    emit finished();
}
