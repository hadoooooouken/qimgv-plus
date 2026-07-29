#include "directorypresenter.h"
#include <QPainter>
#include <QPainterPath>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QRegularExpression>
#include <QCollator>
#include <QDebug>
#include "settings.h"

namespace {

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
    : QObject(parent), mShowDirs(false) {
  // Own instance by default so DirectoryPresenter keeps working
  // standalone; Core replaces it with a shared one via setThumbnailer()
  // so the thumbnail panel and folder view dedupe requests against each
  // other instead of each decoding the same image independently.
  thumbnailer = std::make_shared<Thumbnailer>();
  connect(thumbnailer.get(), &Thumbnailer::thumbnailReady, this,
          &DirectoryPresenter::onThumbnailReady);
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
  dirThumbnailTasks.clear();
  thumbnailer->clearTasks();
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
  dirThumbnailTasks.clear();
  thumbnailer->clearTasks();
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
  for (int i : indexes) {
    if (i < model->dirCount()) {
      QString dirPath = model->dirPathAt(i);
      QStringList filters;
      for (auto &format : QImageReader::supportedImageFormats())
        filters << "*." + QString::fromLatin1(format);
      filters << "*.jfif" << "*.tga" << "*.webp";

      SortingMode folderIconSort = settings->folderIconSortingMode();
      // Single linear pass instead of entryInfoList()+sort - avoids
      // materializing and sorting the whole directory just to read
      // first(). See findFolderCoverImage() for details.
      QString latestImage = findFolderCoverImage(dirPath, filters, folderIconSort);
      if (!latestImage.isEmpty()) {
        latestImage = QDir::fromNativeSeparators(latestImage);
        dirThumbnailTasks.insert(latestImage, i);
        thumbnailer->getThumbnailAsync(latestImage, size, false, false);
      }

      // show default folder icon while loading
      QSvgRenderer svgRenderer;
      svgRenderer.load(
          QString(":/res/icons/common/other/folder32-scalable.svg"));
      int factor = size / svgRenderer.defaultSize().width();
      QSize baseSize = svgRenderer.defaultSize() * factor;
      qreal dpr = qApp->devicePixelRatio();
      QPixmap *pixmap = new QPixmap(baseSize);
      pixmap->setDevicePixelRatio(dpr);
      pixmap->fill(Qt::transparent);
      
      QRectF logicalRect(QPointF(0, 0), QSizeF(baseSize) / dpr);
      QRectF renderedRect = logicalRect;
      renderedRect.setSize(logicalRect.size() * 0.90f);
      renderedRect.moveCenter(logicalRect.center());

      QPainter pixPainter(pixmap);
      svgRenderer.render(&pixPainter, renderedRect);
      pixPainter.end();

      ImageLib::recolor(*pixmap, settings->colorScheme().thumbnail_folder_icons);

      std::shared_ptr<Thumbnail> thumb(
          new Thumbnail(model->dirNameAt(i), tr("Folder"), size,
                        std::shared_ptr<QPixmap>(pixmap)));
      view->setThumbnail(i, thumb);
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

// Finds the folder-cover candidate with a single linear pass, tracking only
// the current best-so-far entry per the active sort mode. Replaces the old
// dir.entryInfoList(filters, QDir::Files, sortFlags) + list.first() pattern,
// which had to materialize and fully sort every matching file in the folder
// just to read one entry. This runs on every call to generateThumbnails(),
// which fires on every folder-view scroll tick (ThumbnailView::
// loadVisibleThumbnails()) - so the sort cost was being paid repeatedly on
// the same folders as they scrolled in and out of the preload zone.
//
// Note: for SORT_TIME*/SORT_SIZE* this still stat()s every file, since that
// IO is inherent to "newest/oldest/largest/smallest" semantics - only the
// O(n log n) comparison + full-list allocation is removed. If folders can
// live on slow/network drives, consider also moving this call off the GUI
// thread (e.g. QtConcurrent::run, mirroring ThumbnailerRunnable).
QString DirectoryPresenter::findFolderCoverImage(const QString &dirPath,
                                                  const QStringList &filters,
                                                  SortingMode mode) const {
  QDirIterator it(dirPath, filters, QDir::Files);
  QString bestPath;
  QDateTime bestTime;
  qint64 bestSize = -1;
  QCollator collator;
  collator.setCaseSensitivity(Qt::CaseInsensitive);

  while (it.hasNext()) {
    it.next();
    QFileInfo info = it.fileInfo();
    switch (mode) {
      case SORT_NAME:
      case SORT_NAME_DESC: {
        bool better = bestPath.isEmpty();
        if (!better) {
          bool less = collator.compare(info.fileName(), QFileInfo(bestPath).fileName()) < 0;
          better = (mode == SORT_NAME) ? less : !less;
        }
        if (better)
          bestPath = info.absoluteFilePath();
        break;
      }
      case SORT_SIZE:
      case SORT_SIZE_DESC: {
        qint64 sz = info.size();
        bool better = bestSize < 0 || (mode == SORT_SIZE ? sz < bestSize : sz > bestSize);
        if (better) {
          bestSize = sz;
          bestPath = info.absoluteFilePath();
        }
        break;
      }
      case SORT_TIME:
      case SORT_TIME_DESC:
      default: {
        QDateTime t = info.lastModified();
        bool better = !bestTime.isValid() || (mode == SORT_TIME ? t < bestTime : t > bestTime);
        if (better) {
          bestTime = t;
          bestPath = info.absoluteFilePath();
        }
        break;
      }
    }
  }
  return bestPath;
}

void DirectoryPresenter::onThumbnailReady(std::shared_ptr<Thumbnail> thumb,
                                          QString filePath) {
  if (!view || !model)
    return;

  // folder thumbnail?
  QString normalizedPath = QDir::fromNativeSeparators(filePath);
  if (dirThumbnailTasks.contains(normalizedPath)) {
    QList<int> indices = dirThumbnailTasks.values(normalizedPath);
    for (int i : std::as_const(indices)) {
      if (thumb->pixmap()) {
        auto folderThumb = composeFolderThumbnail(
            thumb->size(), model->dirNameAt(i), *thumb->pixmap());
        view->setThumbnail(i, folderThumb);
      }
    }
    dirThumbnailTasks.remove(normalizedPath);
    return;
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
DirectoryPresenter::composeFolderThumbnail(int size, const QString &dirName,
                                           const QPixmap &innerThumb) {
  QSvgRenderer svgRenderer(
      QString(":/res/icons/common/other/folder32-scalable.svg"));
  if (!svgRenderer.isValid() || svgRenderer.defaultSize().width() <= 0)
    return std::shared_ptr<Thumbnail>(
        new Thumbnail(dirName, tr("Folder"), size, nullptr));

  int factor = size / svgRenderer.defaultSize().width();
  QSize baseSize = svgRenderer.defaultSize() * factor;

  if (baseSize.isEmpty())
    return std::shared_ptr<Thumbnail>(
        new Thumbnail(dirName, tr("Folder"), size, nullptr));

  qreal dpr = qApp->devicePixelRatio();
  QPixmap *pixmap = new QPixmap(baseSize);
  pixmap->setDevicePixelRatio(dpr);
  pixmap->fill(Qt::transparent);

  QRectF logicalRect(QPointF(0, 0), QSizeF(baseSize) / dpr);
  QRectF renderedRect = logicalRect;
  renderedRect.setSize(logicalRect.size() * 0.90f);
  renderedRect.moveCenter(logicalRect.center());

  {
    QPainter painter(pixmap);
    svgRenderer.render(&painter, renderedRect);
  } // painter scope ends here, so it's safe to recolor

  ImageLib::recolor(*pixmap, settings->colorScheme().thumbnail_folder_icons);

  // Draw the inner thumbnail
  QPainter painter(pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Approximation for the folder "window" (in logical coordinates)
  QRectF windowRect(renderedRect.left() + renderedRect.width() * 0.025,
                    renderedRect.top() + renderedRect.height() * 0.17,
                    renderedRect.width() * 0.95, renderedRect.height() * 0.80);

  // Maintain aspect ratio of the thumbnail inside the window
  // innerThumb already has its own dpr set, drawPixmap will handle it
  qreal innerDpr = innerThumb.devicePixelRatioF();
  if (innerDpr <= 0)
    innerDpr = 1.0;

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
  path.addRoundedRect(targetRect, 4.0, 4.0);
  painter.setClipPath(path);

  painter.drawPixmap(targetRect, innerThumb, innerThumb.rect());
  painter.end();

  return std::shared_ptr<Thumbnail>(
      new Thumbnail(dirName, tr("Folder"), size, std::shared_ptr<QPixmap>(pixmap)));
}

void DirectoryPresenter::onSettingsChanged() {
  if (!view || !model || !mShowDirs)
    return;
  QList<int> folderIndexes;
  for (int i = 0; i < model->dirCount(); ++i) {
    folderIndexes.append(i);
  }
  generateThumbnails(folderIndexes, settings->folderViewIconSize(), false, true);
}
