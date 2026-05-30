#include "directorypresenter.h"
#include <QPainter>

DirectoryPresenter::DirectoryPresenter(QObject *parent)
    : QObject(parent), mShowDirs(false) {
  connect(&thumbnailer, &Thumbnailer::thumbnailReady, this,
          &DirectoryPresenter::onThumbnailReady);
}

void DirectoryPresenter::unsetModel() {
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
  connect(dynamic_cast<QObject *>(view.get()),
          SIGNAL(droppedInto(const QMimeData *, QObject *, int)), this,
          SLOT(onDroppedInto(const QMimeData *, QObject *, int)));
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
  view->populate(mShowDirs ? model->totalCount() : model->fileCount());
  view->setDirCount(mShowDirs ? model->dirCount() : 0);
  selectAndFocus(0);
}

void DirectoryPresenter::disconnectView() {
  // todo
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
  thumbnailer.clearTasks();
  if (!mShowDirs) {
    for (int i : indexes)
      thumbnailer.getThumbnailAsync(model->filePathAt(i), size, crop, force);
    return;
  }
  for (int i : indexes) {
    if (i < model->dirCount()) {
      QString dirPath = model->dirPathAt(i);
      QDir dir(dirPath);
      QStringList filters;
      for (auto &format : QImageReader::supportedImageFormats())
        filters << "*." + QString::fromLatin1(format);
      filters << "*.jfif" << "*.tga" << "*.webp";

      QDir::SortFlags sortFlags = QDir::Time;
      SortingMode folderIconSort = settings->folderIconSortingMode();
      switch (folderIconSort) {
        case SORT_NAME:
          sortFlags = QDir::Name | QDir::IgnoreCase | QDir::LocaleAware;
          break;
        case SORT_NAME_DESC:
          sortFlags = QDir::Name | QDir::IgnoreCase | QDir::LocaleAware | QDir::Reversed;
          break;
        case SORT_SIZE:
          sortFlags = QDir::Size | QDir::Reversed; // smallest first
          break;
        case SORT_SIZE_DESC:
          sortFlags = QDir::Size; // largest first
          break;
        case SORT_TIME:
          sortFlags = QDir::Time | QDir::Reversed; // oldest first
          break;
        case SORT_TIME_DESC:
        default:
          sortFlags = QDir::Time; // newest first
          break;
      }

      QFileInfoList list = dir.entryInfoList(filters, QDir::Files, sortFlags);
      if (!list.isEmpty()) {
        QString latestImage = QDir::fromNativeSeparators(list.first().absoluteFilePath());
        dirThumbnailTasks.insert(latestImage, i);
        thumbnailer.getThumbnailAsync(latestImage, size, false, false);
      }

      // show default folder icon while loading
      QSvgRenderer svgRenderer;
      svgRenderer.load(
          QString(":/res/icons/common/other/folder32-scalable.svg"));
      int factor = (size * 0.90f) / svgRenderer.defaultSize().width();
      QPixmap *pixmap = new QPixmap(svgRenderer.defaultSize() * factor);
      pixmap->fill(Qt::transparent);
      QPainter pixPainter(pixmap);
      svgRenderer.render(&pixPainter);
      pixPainter.end();

      ImageLib::recolor(*pixmap, settings->colorScheme().icons);

      std::shared_ptr<Thumbnail> thumb(
          new Thumbnail(model->dirNameAt(i), tr("Folder"), size,
                        std::shared_ptr<QPixmap>(pixmap)));
      view->setThumbnail(i, thumb);
    } else {
      QString path = model->filePathAt(i - model->dirCount());
      thumbnailer.getThumbnailAsync(path, size, crop, force);
    }
  }
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
    dirThumbnailTasks.remove(filePath);
  }

  int index = model->indexOfFile(filePath);
  if (index == -1)
    return;
  view->setThumbnail(mShowDirs ? model->dirCount() + index : index, thumb);
}

void DirectoryPresenter::onItemActivated(int absoluteIndex) {
  if (!model)
    return;
  if (!mShowDirs) {
    emit fileActivated(model->filePathAt(absoluteIndex));
    return;
  }
  if (absoluteIndex < model->dirCount())
    emit dirActivated(model->dirPathAt(absoluteIndex));
  else
    emit fileActivated(model->filePathAt(absoluteIndex - model->dirCount()));
}

void DirectoryPresenter::onDraggedOut() { emit draggedOut(selectedPaths()); }

void DirectoryPresenter::onDraggedOver(int index) {
  if (!model || view->selection().contains(index))
    return;
  if (showDirs() && index < model->dirCount())
    view->setDragHover(index);
}

void DirectoryPresenter::onDroppedInto(const QMimeData *data, QObject *source,
                                       int targetIndex) {
  if (!data->hasUrls() || model->source() != SOURCE_DIRECTORY)
    return;

  // ignore drops into selected / current folder when we are the source of
  // dropEvent
  if (source && (view->selection().contains(targetIndex) || targetIndex == -1))
    return;
  // ignore drops into a file
  // todo: drop into a current dir when target is a file
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
  emit droppedInto(pathList, destDir);
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

  int factor = (size * 0.90f) / svgRenderer.defaultSize().width();
  QSize baseSize = svgRenderer.defaultSize() * factor;

  if (baseSize.isEmpty())
    return std::shared_ptr<Thumbnail>(
        new Thumbnail(dirName, tr("Folder"), size, nullptr));

  qreal dpr = qApp->devicePixelRatio();
  QPixmap *pixmap = new QPixmap(baseSize);
  pixmap->setDevicePixelRatio(dpr);
  pixmap->fill(Qt::transparent);

  QRectF logicalRect(QPointF(0, 0), QSizeF(baseSize) / dpr);

  {
    QPainter painter(pixmap);
    svgRenderer.render(&painter, logicalRect);
  } // painter scope ends here, so it's safe to recolor

  ImageLib::recolor(*pixmap, settings->colorScheme().icons);

  // Draw the inner thumbnail
  QPainter painter(pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // Approximation for the folder "window" (in logical coordinates)
  QRectF windowRect(logicalRect.width() * 0.025, logicalRect.height() * 0.17,
                    logicalRect.width() * 0.95, logicalRect.height() * 0.80);

  // Maintain aspect ratio of the thumbnail inside the window
  // innerThumb already has its own dpr set, drawPixmap will handle it
  QSize scaledSize = innerThumb.size() / innerThumb.devicePixelRatio();
  scaledSize.scale(windowRect.size().toSize(), Qt::KeepAspectRatio);

  QRectF targetRect(0, 0, scaledSize.width(), scaledSize.height());
  targetRect.moveCenter(windowRect.center());

  painter.drawPixmap(targetRect, innerThumb, innerThumb.rect());
  painter.end();

  return std::shared_ptr<Thumbnail>(
      new Thumbnail(dirName, tr("Folder"), size, std::shared_ptr<QPixmap>(pixmap)));
}
