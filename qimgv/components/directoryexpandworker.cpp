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
    return isRunning.load(std::memory_order_acquire);
}

bool DirectoryExpandWorker::isCancellationRequested() const {
    return cancellationRequested.load(std::memory_order_acquire);
}

void DirectoryExpandWorker::requestStop() {
    cancellationRequested.store(true, std::memory_order_release);
}

void DirectoryExpandWorker::run() {
    isRunning.store(true, std::memory_order_release);

    const auto finish = [this]() {
        isRunning.store(false, std::memory_order_release);
        emit finished();
    };

    if (isCancellationRequested()) {
        finish();
        return;
    }

    QRegularExpression regex(formatsRegex, QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) {
        emit error(QStringLiteral("DirectoryExpandWorker: invalid supported-formats regex, aborting scan"));
        finish();
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
        if (isCancellationRequested())
            break;

        if (!entry.isDirectory) {
            tryAdd(entry.path);
            continue;
        }

        QDirIterator it(entry.path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (isCancellationRequested())
                break;
            QString filePath = QDir::fromNativeSeparators(it.next());
            if (regex.match(it.fileName()).hasMatch())
                tryAdd(filePath);
        }
    }

    flushBatch();
    finish();
}
