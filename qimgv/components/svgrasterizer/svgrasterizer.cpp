#include "svgrasterizer.h"

#include <QDebug>
#include <QFile>
#include <QMetaObject>
#include <QPainter>
#include <QSvgRenderer>

#include <utility>

// The parsed document, owned by the tasks of one generation. The renderer
// is created on a worker thread and detached from it (moveToThread(nullptr)),
// so that whichever thread drops the last reference may destroy it.
struct SvgDocument {
  std::unique_ptr<QSvgRenderer> renderer;
};

namespace {
using namespace Qt::StringLiterals;

// One task at a time: requests replace each other, and the document is
// never used by two tasks at once.
constexpr int kWorkerThreads = 1;
const QString kSvgSuffix = u".svg"_s;
} // namespace

//------------------------------------------------------------------------------
SvgRasterizer::SvgRasterizer(QObject *parent) : QObject(parent) {
  mPool.setMaxThreadCount(kWorkerThreads);
  mPool.setObjectName(u"SvgRasterizer"_s);
}

SvgRasterizer::~SvgRasterizer() {
  // Queued tasks never start; the running one posts its result to this
  // object, which must outlive it. Posted results die with the object.
  mPool.clear();
  mPool.waitForDone();
}

//------------------------------------------------------------------------------
bool SvgRasterizer::isSvgFile(const QString &path) {
  return path.endsWith(kSvgSuffix, Qt::CaseInsensitive);
}

bool SvgRasterizer::matchesImage(QSize documentSize, QSize imageSize) {
  if (documentSize.isEmpty() || imageSize.isEmpty())
    return false;
  return documentSize.scaled(imageSize, Qt::KeepAspectRatio) == imageSize;
}

bool SvgRasterizer::isValidRequest(const SvgRasterRequest &request,
                                   QSize imageSize) {
  const QSize target = request.targetSize;
  if (request.sourceRect.isEmpty() || target.isEmpty())
    return false;
  if (!QRect(QPoint(0, 0), imageSize).contains(request.sourceRect))
    return false;
  return target.width() <= kMaxTargetDimension &&
         target.height() <= kMaxTargetDimension &&
         qint64(target.width()) * target.height() <= kMaxTargetPixels;
}

//------------------------------------------------------------------------------
void SvgRasterizer::open(const QString &path, QSize imageSize) {
  close();
  mState = State::Opening;
  mImageSize = imageSize;
  mDocument = std::make_shared<SvgDocument>();
  mQueuedOpen = OpenTask{TaskIdentity{mGeneration, kNoRequest}, path};
  dispatch();
}

void SvgRasterizer::close() {
  // A new generation: results of running tasks no longer match.
  ++mGeneration;
  mState = State::Closed;
  mImageSize = QSize();
  mDocumentSize = QSize();
  mDocument.reset();
  mQueuedOpen.reset();
  mQueuedRaster.reset();
  mWantedRequestId = kNoRequest;
}

bool SvgRasterizer::isOpen() const { return mState != State::Closed; }

bool SvgRasterizer::isReady() const { return mState == State::Ready; }

QSize SvgRasterizer::documentSize() const { return mDocumentSize; }

//------------------------------------------------------------------------------
quint64 SvgRasterizer::request(const SvgRasterRequest &request) {
  if (mState != State::Ready) {
    qWarning() << "SvgRasterizer::request: no document is ready";
    return kNoRequest;
  }
  if (!isValidRequest(request, mImageSize)) {
    qWarning() << "SvgRasterizer::request: invalid request"
               << request.sourceRect << request.targetSize << "for"
               << mImageSize;
    return kNoRequest;
  }
  const quint64 id = ++mLastRequestId;
  mWantedRequestId = id;
  mQueuedRaster = RasterTask{TaskIdentity{mGeneration, id}, request};
  dispatch();
  return id;
}

void SvgRasterizer::cancelRequests() {
  mQueuedRaster.reset();
  mWantedRequestId = kNoRequest;
}

bool SvgRasterizer::isBusy() const { return mRunning.has_value(); }

//------------------------------------------------------------------------------
void SvgRasterizer::dispatch() {
  if (mRunning)
    return;
  if (mQueuedOpen) {
    OpenTask task = std::move(*mQueuedOpen);
    mQueuedOpen.reset();
    startOpen(std::move(task));
  } else if (mQueuedRaster && mState == State::Ready) {
    RasterTask task = std::move(*mQueuedRaster);
    mQueuedRaster.reset();
    startRaster(std::move(task));
  }
}

void SvgRasterizer::startOpen(OpenTask task) {
  mRunning = task.identity;
  mPool.start([this, task = std::move(task), document = mDocument]() {
    const OpenResult result = parse(*document, task.path);
    QMetaObject::invokeMethod(
        this,
        [this, identity = task.identity, result]() {
          onOpenFinished(identity, result);
        },
        Qt::QueuedConnection);
  });
}

void SvgRasterizer::startRaster(RasterTask task) {
  mRunning = task.identity;
  mPool.start([this, task = std::move(task), document = mDocument,
               imageSize = mImageSize]() {
    const RasterResult result = rasterize(*document, imageSize, task.request);
    QMetaObject::invokeMethod(
        this,
        [this, identity = task.identity, request = task.request, result]() {
          onRasterFinished(identity, request, result);
        },
        Qt::QueuedConnection);
  });
}

bool SvgRasterizer::finishTask(TaskIdentity identity) {
  if (!mRunning || *mRunning != identity) {
    qWarning() << "SvgRasterizer: completion of an unknown task (generation"
               << identity.generation << "request" << identity.requestId
               << ")";
    return false;
  }
  mRunning.reset();
  return true;
}

void SvgRasterizer::onOpenFinished(TaskIdentity identity,
                                   const OpenResult &result) {
  if (!finishTask(identity))
    return;
  if (identity.generation == mGeneration) {
    if (result.error.isEmpty()) {
      mState = State::Ready;
      mDocumentSize = result.documentSize;
      emit documentReady(mDocumentSize);
    } else {
      // Keeps the generation: nothing of this document can run any more.
      mState = State::Closed;
      mDocument.reset();
      mQueuedRaster.reset();
      emit documentFailed(result.error);
    }
  }
  dispatch();
}

void SvgRasterizer::onRasterFinished(TaskIdentity identity,
                                     const SvgRasterRequest &request,
                                     const RasterResult &result) {
  if (!finishTask(identity))
    return;
  if (identity.generation == mGeneration &&
      identity.requestId == mWantedRequestId) {
    mWantedRequestId = kNoRequest;
    if (result.error.isEmpty())
      emit rasterized(SvgRaster{identity.requestId, request, result.image});
    else
      emit rasterFailed(identity.requestId, result.error);
  }
  dispatch();
}

//------------------------------------------------------------------------------
// Worker
//------------------------------------------------------------------------------
SvgRasterizer::OpenResult SvgRasterizer::parse(SvgDocument &document,
                                               const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {.error = u"Cannot open %1: %2"_s.arg(path, file.errorString())};
  auto renderer = std::make_unique<QSvgRenderer>();
  // A static document: SMIL animations are played by AnimationPlayer.
  renderer->setAnimationEnabled(false);
  if (!renderer->load(file.readAll()) || !renderer->isValid())
    return {.error = u"Cannot parse the SVG document %1"_s.arg(path)};
  const QSize documentSize = renderer->defaultSize();
  if (documentSize.isEmpty())
    return {.error = u"The SVG document %1 has no size"_s.arg(path)};
  renderer->moveToThread(nullptr);
  document.renderer = std::move(renderer);
  return {.documentSize = documentSize};
}

// Draws the whole document into a rectangle of imageSize scaled by the
// target / source ratio, shifted so that sourceRect lands on the raster: the
// same mapping as the Qt SVG image plugin's decode at imageSize.
SvgRasterizer::RasterResult
SvgRasterizer::rasterize(SvgDocument &document, QSize imageSize,
                         const SvgRasterRequest &request) {
  if (!document.renderer)
    return {.error = u"The SVG document is not loaded"_s};
  QImage image(request.targetSize, QImage::Format_ARGB32_Premultiplied);
  if (image.isNull()) {
    return {.error = u"Cannot allocate a %1x%2 SVG raster"_s.arg(
                request.targetSize.width())
                         .arg(request.targetSize.height())};
  }
  image.fill(Qt::transparent);
  const QRect &source = request.sourceRect;
  const qreal scaleX = qreal(request.targetSize.width()) / source.width();
  const qreal scaleY = qreal(request.targetSize.height()) / source.height();
  {
    QPainter painter(&image);
    if (!painter.isActive())
      return {.error = u"Cannot paint the SVG raster"_s};
    painter.translate(-source.x() * scaleX, -source.y() * scaleY);
    document.renderer->render(
        &painter, QRectF(0.0, 0.0, imageSize.width() * scaleX,
                         imageSize.height() * scaleY));
  }
  return {.image = std::move(image)};
}
