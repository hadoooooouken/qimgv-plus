#pragma once

#include <QImage>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QThreadPool>
#include <memory>
#include <optional>

struct SvgDocument;

// Part of an SVG document to rasterize: sourceRect of the shown image (the
// document drawn at the image size given to SvgRasterizer::open(), in image
// pixels) rendered into an image of targetSize pixels.
struct SvgRasterRequest {
  QRect sourceRect;
  QSize targetSize;

  friend bool operator==(const SvgRasterRequest &,
                         const SvgRasterRequest &) = default;
};

// A finished SvgRasterRequest: an ARGB32_Premultiplied image of
// request.targetSize.
struct SvgRaster {
  quint64 requestId = 0;
  SvgRasterRequest request;
  QImage image;
};

// Renders parts of one SVG document at any scale with QSvgRenderer on a
// worker thread, without any UI. The Qt Quick viewer shows the result over
// the raster decoded by the Loader, so that SVG documents stay sharp at any
// zoom without parsing or painting them on the GUI thread (the widget
// viewer's QGraphicsSvgItem does both).
//
// open() reads and parses the document on the worker and reports
// documentReady() or documentFailed(). request() then rasterizes a part of
// it; at most one task runs at a time and a newer request replaces one that
// has not started yet. Only the result of the latest request of the current
// document is reported (rasterized() or rasterFailed()); cancelRequests(),
// close() and open() drop the results of earlier requests. A task that has
// already started cannot be interrupted: it finishes in the background and
// its result is discarded.
//
// The worker is a private single-thread pool, not the global one. The
// destructor waits for the running task.
//
// GUI thread only; the parsed document is only touched by the worker tasks.
class SvgRasterizer : public QObject {
  Q_OBJECT

public:
  // Returned by request() when nothing was requested.
  static constexpr quint64 kNoRequest = 0;
  // Largest raster side and area. The viewer requests the visible part of
  // the image at the displayed size, which stays far below these.
  static constexpr int kMaxTargetDimension = 16384;
  static constexpr qint64 kMaxTargetPixels = 128'000'000;

  explicit SvgRasterizer(QObject *parent = nullptr);
  ~SvgRasterizer() override;

  // The file is an SVG document this component renders.
  [[nodiscard]] static bool isSvgFile(const QString &path);
  // The document (default size documentSize) is the source of an image of
  // imageSize: the same size, or that size scaled down with its aspect
  // ratio (the Loader limits the side of decoded images). An image whose
  // sides were swapped by an edit does not match.
  [[nodiscard]] static bool matchesImage(QSize documentSize, QSize imageSize);
  // request lies within an image of imageSize and its raster within the
  // size limits.
  [[nodiscard]] static bool isValidRequest(const SvgRasterRequest &request,
                                           QSize imageSize);

  // Opens the SVG document at path, drawn at imageSize (the size of the
  // image decoded from it); drops the previous document.
  void open(const QString &path, QSize imageSize);
  // Drops the document and the results of its requests.
  void close();
  // A document is being opened or is open.
  [[nodiscard]] bool isOpen() const;
  // The document is parsed; request() is possible.
  [[nodiscard]] bool isReady() const;
  // QSvgRenderer::defaultSize() of the open document; invalid until ready.
  [[nodiscard]] QSize documentSize() const;

  // Rasterizes request; returns its id, or kNoRequest (with a warning) when
  // no document is ready or the request is not valid.
  quint64 request(const SvgRasterRequest &request);
  // Drops the queued request and the result of the running one.
  void cancelRequests();
  // A worker task is still running (also one whose result is discarded).
  [[nodiscard]] bool isBusy() const;

signals:
  void documentReady(QSize documentSize);
  void documentFailed(const QString &message);
  void rasterized(const SvgRaster &raster);
  void rasterFailed(quint64 requestId, const QString &message);

private:
  enum class State { Closed, Opening, Ready };

  // Identity of a task: the document generation it belongs to and, for a
  // raster task, its request id (kNoRequest for the open task).
  struct TaskIdentity {
    quint64 generation = 0;
    quint64 requestId = kNoRequest;

    friend bool operator==(const TaskIdentity &,
                           const TaskIdentity &) = default;
  };
  struct OpenTask {
    TaskIdentity identity;
    QString path;
  };
  struct RasterTask {
    TaskIdentity identity;
    SvgRasterRequest request;
  };
  struct OpenResult {
    QSize documentSize;
    QString error;
  };
  struct RasterResult {
    QImage image;
    QString error;
  };

  // Starts the next queued task when none is running.
  void dispatch();
  void startOpen(OpenTask task);
  void startRaster(RasterTask task);
  // Completion callbacks, posted to the GUI thread by the tasks.
  void onOpenFinished(TaskIdentity identity, const OpenResult &result);
  void onRasterFinished(TaskIdentity identity, const SvgRasterRequest &request,
                        const RasterResult &result);
  // Validates and clears the running task; false for an unexpected one.
  bool finishTask(TaskIdentity identity);

  // Runs on the worker.
  static OpenResult parse(SvgDocument &document, const QString &path);
  static RasterResult rasterize(SvgDocument &document, QSize imageSize,
                                const SvgRasterRequest &request);

  State mState = State::Closed;
  quint64 mGeneration = 0;
  quint64 mLastRequestId = kNoRequest;
  // The request whose result is reported; kNoRequest after a cancel.
  quint64 mWantedRequestId = kNoRequest;
  QSize mImageSize;
  QSize mDocumentSize;
  // Shared with the tasks of the current document; never dereferenced on
  // the GUI thread.
  std::shared_ptr<SvgDocument> mDocument;
  // The task physically running on the worker, until its completion
  // callback arrives.
  std::optional<TaskIdentity> mRunning;
  std::optional<OpenTask> mQueuedOpen;
  std::optional<RasterTask> mQueuedRaster;
  // Declared last: destroyed (and waited for) first.
  QThreadPool mPool;
};
