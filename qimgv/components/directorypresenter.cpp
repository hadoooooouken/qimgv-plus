#include "directorypresenter.h"
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QDebug>
#include "settings.h"

#include <utility>

namespace {

const QString kFolderIconResource =
    QStringLiteral(":/res/icons/common/other/folder32-scalable.svg");
constexpr qreal kDefaultDevicePixelRatio = 1.0;
constexpr qreal kFolderIconRenderedScale = 0.90;
constexpr qreal kFolderWindowLeftOffset = 0.025;
constexpr qreal kFolderWindowTopOffset = 0.17;
constexpr qreal kFolderWindowWidthScale = 0.95;
constexpr qreal kFolderWindowHeightScale = 0.80;
constexpr qreal kFolderThumbnailCornerRadius = 4.0;

ThumbnailSource thumbnailSourceFromEntry(const FSEntry &entry)
{
  ThumbnailSource source{entry.path, std::nullopt};
  if (!entry.path.isEmpty()) {
    source.stamp = ThumbnailSourceStamp::fromMetadata(
        entry.path, entry.size, entry.modifyTime);
  }
  return source;
}

} // namespace

DirectoryPresenter::DirectoryPresenter(QObject *parent)
    : QObject(parent),
      folderCoverResolver(std::make_unique<FolderCoverResolver>()),
      mShowDirs(false),
      folderIconRenderer(kFolderIconResource) {
  if (!folderIconRenderer.isValid()) {
    qWarning() << "[DirectoryPresenter] Could not load the default folder icon"
               << kFolderIconResource;
  }

  // Own instance by default so DirectoryPresenter keeps working
  // standalone; Core replaces it with a shared one via setThumbnailer()
  // so the thumbnail panel and folder view dedupe requests against each
  // other instead of each decoding the same image independently.
  thumbnailer = std::make_shared<Thumbnailer>();
  connect(thumbnailer.get(), &Thumbnailer::thumbnailReady, this,
          &DirectoryPresenter::onThumbnailReady);
  connect(folderCoverResolver.get(), &FolderCoverResolver::resultReady, this,
          &DirectoryPresenter::onFolderCoverResolved, Qt::QueuedConnection);
  connect(settings, &Settings::settingsChanged, this,
          &DirectoryPresenter::onSettingsChanged);
}

DirectoryPresenter::~DirectoryPresenter() {
  cancelExpandedPathsScan();
}

void DirectoryPresenter::setThumbnailer(std::shared_ptr<Thumbnailer> newThumbnailer) {
  if (!newThumbnailer || newThumbnailer == thumbnailer)
    return;
  disconnect(thumbnailer.get(), &Thumbnailer::thumbnailReady, this,
             &DirectoryPresenter::onThumbnailReady);
  thumbnailer = newThumbnailer;
  connect(thumbnailer.get(), &Thumbnailer::thumbnailReady, this,
          &DirectoryPresenter::onThumbnailReady);
}

void DirectoryPresenter::unsetModel() {
  cancelExpandedPathsScan();
  invalidateFolderThumbnailRequests();
  disconnect(model.get(), &DirectoryModel::fileRemoved, this,
             &DirectoryPresenter::onFileRemoved);
  disconnect(model.get(), &DirectoryModel::fileAdded, this,
             &DirectoryPresenter::onFileAdded);
  disconnect(model.get(), &DirectoryModel::fileRenamed, this,
             &DirectoryPresenter::onFileRenamed);
  disconnect(model.get(), &DirectoryModel::fileModified, this,
             &DirectoryPresenter::onFileModified);
  disconnect(model.get(), &DirectoryModel::dirRemoved, this,
             &DirectoryPresenter::onDirRemoved);
  disconnect(model.get(), &DirectoryModel::dirAdded, this,
             &DirectoryPresenter::onDirAdded);
  disconnect(model.get(), &DirectoryModel::dirRenamed, this,
             &DirectoryPresenter::onDirRenamed);
  model = nullptr;
  // also empty view?
}

void DirectoryPresenter::setView(std::shared_ptr<IDirectoryView> _view) {
  if (view)
    return;
  view = _view;
  if (model)
    view->populate(mShowDirs ? model->totalCount() : model->fileCount());
  connect(dynamic_cast<QObject *>(view.get()), SIGNAL(itemActivated(int)), this,
          SLOT(onItemActivated(int)));
  connect(dynamic_cast<QObject *>(view.get()),
          SIGNAL(thumbnailsRequested(QList<int>, int, bool, bool)), this,
          SLOT(generateThumbnails(QList<int>, int, bool, bool)));
  connect(dynamic_cast<QObject *>(view.get()), SIGNAL(draggedOut()), this,
          SLOT(onDraggedOut()));
  connect(dynamic_cast<QObject *>(view.get()), SIGNAL(draggedOver(int)), this,
          SLOT(onDraggedOver(int)));
  connect(dynamic_cast<QObject *>(view.get()), SIGNAL(openSelectedRequested()), this,
          SLOT(onOpenSelectedRequested()));
  connect(dynamic_cast<QObject *>(view.get()),
          SIGNAL(droppedInto(const QMimeData *, QObject *, int, Qt::DropAction)), this,
          SLOT(onDroppedInto(const QMimeData *, QObject *, int, Qt::DropAction)));
  connect(dynamic_cast<QObject *>(view.get()), SIGNAL(backRequested()), this,
          SIGNAL(backRequested()));
  connect(dynamic_cast<QObject *>(view.get()), SIGNAL(forwardRequested()), this,
          SIGNAL(forwardRequested()));
}

void DirectoryPresenter::setModel(std::shared_ptr<DirectoryModel> newModel) {
  if (model)
    unsetModel();
  if (!newModel)
    return;
  model = newModel;
  populateView();

  // filesystem changes
  connect(model.get(), &DirectoryModel::fileRemoved, this,
          &DirectoryPresenter::onFileRemoved);
  connect(model.get(), &DirectoryModel::fileAdded, this,
          &DirectoryPresenter::onFileAdded);
  connect(model.get(), &DirectoryModel::fileRenamed, this,
          &DirectoryPresenter::onFileRenamed);
  connect(model.get(), &DirectoryModel::fileModified, this,
          &DirectoryPresenter::onFileModified);
  connect(model.get(), &DirectoryModel::dirRemoved, this,
          &DirectoryPresenter::onDirRemoved);
  connect(model.get(), &DirectoryModel::dirAdded, this,
          &DirectoryPresenter::onDirAdded);
  connect(model.get(), &DirectoryModel::dirRenamed, this,
          &DirectoryPresenter::onDirRenamed);
}

void DirectoryPresenter::reloadModel() { populateView(); }

void DirectoryPresenter::populateView() {
  if (!model || !view)
    return;
  invalidateFolderThumbnailRequests();
  // Thumbnailer is shared with the other presenter. Clearing its pool here
  // would cancel that view's queued requests.
  view->populate(mShowDirs ? model->totalCount() : model->fileCount());
  view->setDirCount(mShowDirs ? model->dirCount() : 0);
  selectAndFocus(0);
}

void DirectoryPresenter::disconnectView() {
}

//------------------------------------------------------------------------------

void DirectoryPresenter::onFileRemoved(QString filePath, int index) {
  Q_UNUSED(filePath)
  if (!view)
    return;
  view->removeItem(mShowDirs ? index + model->dirCount() : index);
}

void DirectoryPresenter::onFileRenamed(QString fromPath, int indexFrom,
                                       QString toPath, int indexTo) {
  Q_UNUSED(fromPath)
  Q_UNUSED(toPath)
  if (!view)
    return;
  if (mShowDirs) {
    indexFrom += model->dirCount();
    indexTo += model->dirCount();
  }
  auto oldSelection = view->selection();
  view->removeItem(indexFrom);
  view->insertItem(indexTo);
  // re-select if needed
  if (oldSelection.contains(indexFrom)) {
    if (oldSelection.count() == 1) {
      view->select(indexTo);
      view->focusOn(indexTo);
    } else if (oldSelection.count() > 1) {
      view->select(view->selection() << indexTo);
    }
  }
}

void DirectoryPresenter::onFileAdded(QString filePath) {
  if (!view)
    return;
  int index = model->indexOfFile(filePath);
  view->insertItem(mShowDirs ? model->dirCount() + index : index);
}

void DirectoryPresenter::onFileModified(QString filePath) {
  if (!view)
    return;
  int index = model->indexOfFile(filePath);
  view->reloadItem(mShowDirs ? model->dirCount() + index : index);
}

void DirectoryPresenter::onDirRemoved(QString dirPath, int index) {
  Q_UNUSED(dirPath)
  if (!view || !mShowDirs)
    return;
  view->removeItem(index);
  view->setDirCount(model->dirCount());
}

void DirectoryPresenter::onDirRenamed(QString fromPath, int indexFrom,
                                      QString toPath, int indexTo) {
  Q_UNUSED(fromPath)
  Q_UNUSED(toPath)
  if (!view || !mShowDirs)
    return;
  auto oldSelection = view->selection();
  view->removeItem(indexFrom);
  view->insertItem(indexTo);
  // re-select if needed
  if (oldSelection.contains(indexFrom)) {
    if (oldSelection.count() == 1) {
      view->select(indexTo);
      view->focusOn(indexTo);
    } else if (oldSelection.count() > 1) {
      view->select(view->selection() << indexTo);
    }
  }
}

void DirectoryPresenter::onDirAdded(QString dirPath) {
  if (!view || !mShowDirs)
    return;
  int index = model->indexOfDir(dirPath);
  view->insertItem(index);
  view->setDirCount(model->dirCount());
}

bool DirectoryPresenter::showDirs() { return mShowDirs; }

void DirectoryPresenter::setShowDirs(bool mode) {
  if (mode == mShowDirs)
    return;
  mShowDirs = mode;
  populateView();
}

QList<QString> DirectoryPresenter::selectedPaths() const {
  QList<QString> paths;
  if (!view)
    return paths;
  if (mShowDirs) {
    for (auto i : view->selection()) {
      if (i < model->dirCount())
        paths << model->dirPathAt(i);
      else
        paths << model->filePathAt(i - model->dirCount());
    }
  } else {
    for (auto i : view->selection()) {
      paths << model->filePathAt(i);
    }
  }
  return paths;
}

void DirectoryPresenter::generateThumbnails(QList<int> indexes, int size,
                                            bool crop, bool force) {
  if (!view || !model)
    return;
  if (!mShowDirs) {
    for (int i : indexes) {
      if (i < 0 || i >= model->fileCount())
        continue;
      thumbnailer->getThumbnailAsync(
          thumbnailSourceFromEntry(model->fileEntryAt(i)), size, crop, force);
    }
    return;
  }

  const SortingMode folderIconSort = settings->folderIconSortingMode();
  for (int i : indexes) {
    if (i < 0 || i >= model->totalCount())
      continue;

    if (i < model->dirCount()) {
      const QString dirPath = model->dirPathAt(i);

      // The prepared default is published before cover discovery is queued,
      // so directory enumeration can never delay the first visible icon.
      view->setThumbnail(
          i, defaultFolderThumbnail(size, model->dirNameAt(i)));

      folderCoverResolver->resolve(
          FolderCoverRequest{
              dirPath,
              folderIconSort,
              size,
              folderThumbnailGeneration
          });
    } else {
      const int fileIndex = i - model->dirCount();
      if (fileIndex < 0 || fileIndex >= model->fileCount())
        continue;
      thumbnailer->getThumbnailAsync(
          thumbnailSourceFromEntry(model->fileEntryAt(fileIndex)), size, crop,
          force);
    }
  }
}

void DirectoryPresenter::onFolderCoverResolved(FolderCoverResult result)
{
  if (result.request.generation != folderThumbnailGeneration ||
      !view || !model || !mShowDirs)
    return;

  if (result.status == FolderCoverStatus::ReadError) {
    qWarning() << "[FolderCoverResolver]" << result.diagnostic;
    return;
  }
  if (result.status != FolderCoverStatus::CoverFound ||
      result.coverPath.isEmpty())
    return;

  const int directoryIndex =
      directoryIndexForPath(result.request.folderPath);
  if (directoryIndex < 0)
    return;

  const QString coverKey = thumbnailPathKey(result.coverPath);
  QList<PendingFolderThumbnail> &pending = dirThumbnailTasks[coverKey];
  const PendingFolderThumbnail newRequest{
      result.request.folderPath,
      result.request.thumbnailSize,
      result.request.generation
  };
  bool alreadyPending = false;
  for (const PendingFolderThumbnail &existing : std::as_const(pending)) {
    if (thumbnailPathKey(existing.folderPath) ==
            thumbnailPathKey(newRequest.folderPath) &&
        existing.thumbnailSize == newRequest.thumbnailSize &&
        existing.generation == newRequest.generation) {
      alreadyPending = true;
      break;
    }
  }
  if (alreadyPending)
    return;

  pending.append(newRequest);
  thumbnailer->getThumbnailAsync(
      result.coverPath, result.request.thumbnailSize, false, false,
      kFolderCoverThumbnailPriority);
}

void DirectoryPresenter::onThumbnailReady(std::shared_ptr<Thumbnail> thumb,
                                          QString filePath) {
  if (!view || !model)
    return;

  if (!thumb) {
    qWarning() << "[DirectoryPresenter] Thumbnailer returned an empty result for"
               << filePath;
    return;
  }

  const QString coverKey = thumbnailPathKey(filePath);
  auto folderTasks = dirThumbnailTasks.find(coverKey);
  if (folderTasks != dirThumbnailTasks.end()) {
    QList<PendingFolderThumbnail> remaining;
    const std::shared_ptr<QPixmap> innerPixmap = thumb->pixmap();
    for (const PendingFolderThumbnail &task :
         std::as_const(folderTasks.value())) {
      if (task.thumbnailSize != thumb->size()) {
        remaining.append(task);
        continue;
      }
      if (task.generation != folderThumbnailGeneration || !mShowDirs)
        continue;

      const int directoryIndex = directoryIndexForPath(task.folderPath);
      if (directoryIndex < 0)
        continue;

      if (innerPixmap) {
        view->setThumbnail(
            directoryIndex,
            composeFolderThumbnail(
                task.thumbnailSize, model->dirNameAt(directoryIndex),
                *innerPixmap));
      }
    }

    if (remaining.isEmpty())
      dirThumbnailTasks.erase(folderTasks);
    else
      folderTasks.value() = std::move(remaining);
  }

  int index = model->indexOfFile(filePath);
  if (index == -1)
    return;
  view->setThumbnail(mShowDirs ? model->dirCount() + index : index, thumb);
}

void DirectoryPresenter::onItemActivated(int absoluteIndex) {
  if (!model)
    return;

  auto selection = view->selection();
  if (selection.count() > 1 && selection.contains(absoluteIndex)) {
      QString activePath;
      if (mShowDirs && absoluteIndex < model->dirCount())
          activePath = model->dirPathAt(absoluteIndex);
      else
          activePath = model->filePathAt(mShowDirs ? absoluteIndex - model->dirCount() : absoluteIndex);

      // A recursive scan of every selected directory here (as this branch
      // used to do inline) is fine for a couple of images, but freezes the
      // UI thread for as long as a large subtree takes to walk. Do the
      // scan on a background thread instead; onExpandScanFinished() emits
      // the same signal this branch used to emit inline once the
      // (cancellable) scan completes.
      startExpandedPathsScan(ExpandPurpose::ItemActivation, absoluteIndex, activePath);
      return;
  }

  if (!mShowDirs) {
    emit fileActivated(model->filePathAt(absoluteIndex));
    return;
  }
  if (absoluteIndex < model->dirCount())
    emit dirActivated(model->dirPathAt(absoluteIndex));
  else
    emit fileActivated(model->filePathAt(absoluteIndex - model->dirCount()));
}

void DirectoryPresenter::onOpenSelectedRequested() {
  if (!model)
    return;

  // See the comment in onItemActivated(): run the recursive expansion on
  // a background thread instead of blocking here.
  startExpandedPathsScan(ExpandPurpose::OpenSelected, -1, QString());
}

void DirectoryPresenter::requestExpandedSelectedPathsAsync() {
  // Same background-worker path as onItemActivated()/
  // onOpenSelectedRequested() below - see the ExpandPurpose comment in
  // the header. No fallback index/path: BatchConvert either emits the
  // bounded complete result, reports a failure, or emits nothing
  // (see onExpandScanFinished()).
  startExpandedPathsScan(ExpandPurpose::BatchConvert, -1, QString());
}

void DirectoryPresenter::startExpandedPathsScan(ExpandPurpose purpose, int fallbackAbsoluteIndex,
                                                const QString &fallbackActivePath) {
  if (!model)
    return;

  PendingExpandContext ctx;
  ctx.purpose = purpose;
  ctx.fallbackAbsoluteIndex = fallbackAbsoluteIndex;
  ctx.fallbackActivePath = fallbackActivePath;
  ctx.generation = ++expandEpoch;

  if (expandWorker) {
    // A scan is already running - cancel it and remember this request.
    // onExpandScanFinished() launches it once the in-flight worker has
    // actually stopped, so two QDirIterator scans never race against
    // each other over the same presenter/model state.
    expandNextContext = ctx;
    expandRestartPending = true;
    expandWorker->requestStop();
    return;
  }

  // A worker can be deleted before its queued completion is delivered, which
  // clears the QPointer above. This direct launch is the newest request and
  // therefore supersedes any deferred context left by that stale completion.
  expandRestartPending = false;
  expandNextContext = PendingExpandContext{};
  launchExpandScan(ctx);
}

void DirectoryPresenter::launchExpandScan(const PendingExpandContext &ctx) {
  expandContext = ctx;
  expandAccumulatedPaths.clear();

  // Computed here rather than passed in via ctx, so it reflects the same
  // selection the entries below are built from (matters if this launch
  // was deferred - see startExpandedPathsScan() - and the selection
  // changed while an earlier scan was still winding down).
  if (expandContext.purpose == ExpandPurpose::BatchConvert)
    expandContext.batchConvertDefaultOutputDir = firstSelectedDirectoryPath();

  // Classify each selected path as a file or a directory here, on the UI
  // thread, using cheap in-memory DirectoryModel lookups - the worker
  // itself never touches the model, since it lives on a background thread.
  const QList<QString> selected = selectedPaths();
  QList<DirectoryExpandWorker::SelectedEntry> entries;
  entries.reserve(selected.size());
  for (const QString &path : selected) {
    DirectoryExpandWorker::SelectedEntry entry;
    entry.path = path;
    if (model->containsFile(path)) {
      entry.isDirectory = false;
    } else if (model->containsDir(path)) {
      entry.isDirectory = true;
    } else {
      // Neither a known file nor a known directory any more (e.g. removed
      // from under the selection between the click and this scan
      // starting) - drop it, same as the old synchronous scan implicitly did.
      continue;
    }
    entries << entry;
  }

  auto *thread = new QThread();
  auto *worker =
      new DirectoryExpandWorker(std::move(entries), settings->supportedFormatsRegex());
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &DirectoryExpandWorker::run);
  const quint64 launchGeneration = expandContext.generation;
  connect(worker, &DirectoryExpandWorker::pathsReady, this,
          [this, launchGeneration](QList<QString> paths) {
            onExpandBatchReady(launchGeneration, std::move(paths));
          });
  connect(worker, &DirectoryExpandWorker::resultLimitExceeded, this,
          [this, launchGeneration](int resultLimit) {
            onExpandScanFailed(
                {launchGeneration, expandResultLimitErrorMessage(resultLimit)});
          });
  connect(worker, &DirectoryExpandWorker::error, this,
          [this, launchGeneration](const QString &message) {
            onExpandScanFailed({
                launchGeneration,
                tr("Directory expansion failed: %1").arg(message),
            });
  });
  connect(worker, &DirectoryExpandWorker::finished, thread, &QThread::quit);
  connect(worker, &DirectoryExpandWorker::finished, this, [this, launchGeneration]() {
    onExpandScanFinished(launchGeneration);
  });
  connect(worker, &DirectoryExpandWorker::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);

  expandWorker = worker;
  expandThread = thread;
  thread->start();
}

void DirectoryPresenter::cancelExpandedPathsScan() {
  if (expandWorker)
    expandWorker->requestStop();
  expandRestartPending = false;
  expandNextContext = PendingExpandContext{};
  ++expandEpoch;
}

void DirectoryPresenter::onExpandBatchReady(quint64 generation, QList<QString> paths) {
  if (generation != expandEpoch || generation != expandContext.generation ||
      !expandContext.failureMessage.isEmpty())
    return;

  const qsizetype remainingCapacity =
      DirectoryExpandWorker::MAX_RESULT_COUNT - expandAccumulatedPaths.size();
  if (paths.size() > remainingCapacity) {
    onExpandScanFailed({
        generation,
        expandResultLimitErrorMessage(DirectoryExpandWorker::MAX_RESULT_COUNT),
    });
    if (expandWorker)
      expandWorker->requestStop();
    return;
  }

  expandAccumulatedPaths.append(std::move(paths));
}

QString DirectoryPresenter::expandResultLimitErrorMessage(int resultLimit) const {
  return tr("Directory expansion stopped because the selection contains more than "
            "%1 supported files. Narrow the selection and try again.")
      .arg(resultLimit);
}

void DirectoryPresenter::onExpandScanFailed(ExpandFailure failure) {
  if (failure.generation != expandEpoch ||
      failure.generation != expandContext.generation)
    return;

  qWarning() << "[DirectoryPresenter]" << failure.errorMessage;
  expandContext.failureMessage = std::move(failure.errorMessage);
  expandAccumulatedPaths.clear();
}

void DirectoryPresenter::onExpandScanFinished(quint64 generation) {
  // A newer worker may already have been launched if this worker was deleted
  // before its queued completion reached the presenter. Never let the stale
  // completion reset that newer worker's state.
  if (generation != expandContext.generation)
    return;

  PendingExpandContext finishedCtx = std::move(expandContext);
  QList<QString> filePaths = std::move(expandAccumulatedPaths);

  expandWorker = nullptr;
  expandThread = nullptr;
  expandContext = PendingExpandContext{};
  expandAccumulatedPaths.clear();

  if (expandRestartPending) {
    PendingExpandContext nextCtx = expandNextContext;
    expandRestartPending = false;
    expandNextContext = PendingExpandContext{};
    launchExpandScan(nextCtx);
    return; // superseded request - this (possibly partial, cancelled) scan's results are discarded
  }

  if (generation != expandEpoch || !model)
    return;

  if (!finishedCtx.failureMessage.isEmpty()) {
    emit selectionExpansionFailed(std::move(finishedCtx.failureMessage));
    return;
  }

  switch (finishedCtx.purpose) {
    case ExpandPurpose::OpenSelected:
      if (!filePaths.isEmpty()) {
        QString activePath = filePaths.first();
        emit filesActivated(std::move(filePaths), std::move(activePath));
      }
      break;

    case ExpandPurpose::ItemActivation: {
      if (filePaths.count() > 1) {
        QString activePath = filePaths.contains(finishedCtx.fallbackActivePath)
                                 ? std::move(finishedCtx.fallbackActivePath)
                                 : filePaths.first();
        emit filesActivated(std::move(filePaths), std::move(activePath));
        break;
      }
      // 0 or 1 matching files after expansion: fall back to activating
      // the originally-clicked item directly, exactly like the pre-scan
      // synchronous code path used to when selection.count() <= 1.
      int absoluteIndex = finishedCtx.fallbackAbsoluteIndex;
      if (!mShowDirs) {
        emit fileActivated(model->filePathAt(absoluteIndex));
      } else if (absoluteIndex < model->dirCount()) {
        emit dirActivated(model->dirPathAt(absoluteIndex));
      } else {
        emit fileActivated(model->filePathAt(absoluteIndex - model->dirCount()));
      }
      break;
    }

    case ExpandPurpose::BatchConvert:
      // Mirrors the old synchronous helper's implicit behavior: do
      // nothing on an empty expansion instead of opening an empty batch
      // converter (see Core::onBatchConverterPathsReady()).
      if (!filePaths.isEmpty())
        emit expandedSelectedPathsReady(
            std::move(filePaths), std::move(finishedCtx.batchConvertDefaultOutputDir));
      break;
  }
}

QString DirectoryPresenter::firstSelectedDirectoryPath() const {
  if (!model)
    return {};

  for (const QString &path : selectedPaths()) {
    QFileInfo info(path);
    if (info.isDir()) {
      return info.absoluteFilePath();
    }
  }

  return {};
}

void DirectoryPresenter::onDraggedOut() { emit draggedOut(selectedPaths()); }

void DirectoryPresenter::onDraggedOver(int index) {
  if (!model || view->selection().contains(index))
    return;
  if (showDirs() && index < model->dirCount())
    view->setDragHover(index);
}

void DirectoryPresenter::onDroppedInto(const QMimeData *data, QObject *source,
                                       int targetIndex, Qt::DropAction action) {
  if (!data->hasUrls() || model->source() != SOURCE_DIRECTORY)
    return;

  // ignore drops into selected / current folder when we are the source of
  // dropEvent
  if (source && (view->selection().contains(targetIndex) || targetIndex == -1))
    return;
  // ignore drops into a file
  if (showDirs() && targetIndex >= model->dirCount())
    return;

  // convert urls to qstrings
  QStringList pathList;
  QList<QUrl> urlList = data->urls();
  for (int i = 0; i < urlList.size(); ++i)
    pathList.append(urlList.at(i).toLocalFile());

  // get target dir path
  QString destDir;
  if (showDirs() && targetIndex < model->dirCount())
    destDir = model->dirPathAt(targetIndex);
  if (destDir.isEmpty()) // fallback to the current dir
    destDir = model->directoryPath();
  pathList.removeAll(destDir); // remove target dir from source list

  // pass to core
  emit droppedInto(pathList, destDir, action);
}

void DirectoryPresenter::selectAndFocus(QString path) {
  if (!model || !view || path.isEmpty())
    return;
  if (model->containsDir(path) && showDirs()) {
    int dirIndex = model->indexOfDir(path);
    view->select(dirIndex);
    view->focusOn(dirIndex);
  } else if (model->containsFile(path)) {
    int fileIndex = showDirs() ? model->indexOfFile(path) + model->dirCount()
                               : model->indexOfFile(path);
    view->select(fileIndex);
    view->focusOn(fileIndex);
  }
}

void DirectoryPresenter::selectAndFocus(int absoluteIndex) {
  if (!model || !view)
    return;
  view->select(absoluteIndex);
  view->focusOn(absoluteIndex);
}

std::shared_ptr<Thumbnail>
DirectoryPresenter::defaultFolderThumbnail(int size, const QString &dirName)
{
  return std::make_shared<Thumbnail>(
      dirName, tr("Folder"), size, defaultFolderPixmap(size));
}

std::shared_ptr<QPixmap>
DirectoryPresenter::defaultFolderPixmap(int size)
{
  if (size <= 0 || !folderIconRenderer.isValid() ||
      folderIconRenderer.defaultSize().width() <= 0)
    return {};

  const qreal reportedDevicePixelRatio =
      qApp ? qApp->devicePixelRatio() : kDefaultDevicePixelRatio;
  const qreal devicePixelRatio =
      reportedDevicePixelRatio > 0
          ? reportedDevicePixelRatio
          : kDefaultDevicePixelRatio;
  const QColor iconColor =
      settings->colorScheme().thumbnail_folder_icons;
  const DefaultFolderIconKey key{
      size,
      devicePixelRatio,
      iconColor.rgba()
  };
  const auto cachedIcon = defaultFolderIconCache.constFind(key);
  if (cachedIcon != defaultFolderIconCache.cend())
    return cachedIcon.value();

  const int scaleFactor =
      qMax(1, size / folderIconRenderer.defaultSize().width());
  const QSize baseSize =
      folderIconRenderer.defaultSize() * scaleFactor;
  if (baseSize.isEmpty())
    return {};

  auto pixmap = std::make_shared<QPixmap>(baseSize);
  pixmap->setDevicePixelRatio(devicePixelRatio);
  pixmap->fill(Qt::transparent);

  const QRectF logicalRect(
      QPointF(0, 0), QSizeF(baseSize) / devicePixelRatio);
  QRectF renderedRect = logicalRect;
  renderedRect.setSize(
      logicalRect.size() * kFolderIconRenderedScale);
  renderedRect.moveCenter(logicalRect.center());

  {
    QPainter painter(pixmap.get());
    folderIconRenderer.render(&painter, renderedRect);
  }
  ImageLib::recolor(*pixmap, iconColor);

  defaultFolderIconCache.insert(key, pixmap);
  return pixmap;
}

std::shared_ptr<Thumbnail>
DirectoryPresenter::composeFolderThumbnail(int size, const QString &dirName,
                                           const QPixmap &innerThumb) {
  const std::shared_ptr<QPixmap> defaultPixmap =
      defaultFolderPixmap(size);
  if (!defaultPixmap)
    return std::make_shared<Thumbnail>(
        dirName, tr("Folder"), size, std::shared_ptr<QPixmap>{});

  auto pixmap = std::make_shared<QPixmap>(*defaultPixmap);
  const qreal devicePixelRatio =
      pixmap->devicePixelRatioF() > 0
          ? pixmap->devicePixelRatioF()
          : kDefaultDevicePixelRatio;
  const QRectF logicalRect(
      QPointF(0, 0), QSizeF(pixmap->size()) / devicePixelRatio);
  QRectF renderedRect = logicalRect;
  renderedRect.setSize(
      logicalRect.size() * kFolderIconRenderedScale);
  renderedRect.moveCenter(logicalRect.center());

  // Draw the inner thumbnail
  QPainter painter(pixmap.get());
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Approximation for the folder "window" (in logical coordinates)
  QRectF windowRect(
      renderedRect.left() +
          renderedRect.width() * kFolderWindowLeftOffset,
      renderedRect.top() +
          renderedRect.height() * kFolderWindowTopOffset,
      renderedRect.width() * kFolderWindowWidthScale,
      renderedRect.height() * kFolderWindowHeightScale);

  // Maintain aspect ratio of the thumbnail inside the window
  // innerThumb already has its own dpr set, drawPixmap will handle it
  qreal innerDpr = innerThumb.devicePixelRatioF();
  if (innerDpr <= 0)
    innerDpr = kDefaultDevicePixelRatio;

  QSize scaledSize(qRound(innerThumb.width() / innerDpr),
                   qRound(innerThumb.height() / innerDpr));
  const QSize windowSize = windowRect.size().toSize();
  // Keep tiny source images at their native logical size. Scaling them up
  // here would bake the enlargement into the composed folder pixmap, so the
  // no-upscale logic in ThumbnailWidget could no longer recover the original
  // dimensions.
  if (scaledSize.width() > windowSize.width() ||
      scaledSize.height() > windowSize.height()) {
    scaledSize.scale(windowSize, Qt::KeepAspectRatio);
  }

  QRectF targetRect(0, 0, scaledSize.width(), scaledSize.height());
  targetRect.moveCenter(windowRect.center());

  QPainterPath path;
  path.addRoundedRect(targetRect, kFolderThumbnailCornerRadius,
                      kFolderThumbnailCornerRadius);
  painter.setClipPath(path);

  painter.drawPixmap(targetRect, innerThumb, innerThumb.rect());
  painter.end();

  return std::make_shared<Thumbnail>(
      dirName, tr("Folder"), size, std::move(pixmap));
}

QString DirectoryPresenter::thumbnailPathKey(const QString &path)
{
  if (path.isEmpty())
    return {};
  return QDir::cleanPath(
             QDir::fromNativeSeparators(path))
      .toCaseFolded();
}

int DirectoryPresenter::directoryIndexForPath(const QString &path) const
{
  if (!model)
    return -1;

  const int directIndex = model->indexOfDir(path);
  if (directIndex >= 0)
    return directIndex;

  const QString expectedPath = thumbnailPathKey(path);
  for (int index = 0; index < model->dirCount(); ++index) {
    if (thumbnailPathKey(model->dirPathAt(index)) == expectedPath)
      return index;
  }
  return -1;
}

void DirectoryPresenter::invalidateFolderThumbnailRequests()
{
  ++folderThumbnailGeneration;
  dirThumbnailTasks.clear();
}

void DirectoryPresenter::onSettingsChanged() {
  if (!view || !model || !mShowDirs)
    return;
  invalidateFolderThumbnailRequests();
  QList<int> folderIndexes;
  for (int i = 0; i < model->dirCount(); ++i) {
    folderIndexes.append(i);
  }
  generateThumbnails(folderIndexes, settings->folderViewIconSize(), false, true);
}
