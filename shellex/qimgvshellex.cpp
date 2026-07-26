// shellex/qimgvshellex.cpp
#include "qimgvshellex.h"
#include <memory>
#include <shlwapi.h>

// Qt Headers
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIODevice>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryFile>
#include <QVector>
#include <QXmlStreamReader>
#include <QtSvg/QSvgRenderer>
#include <libraw/libraw.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <new>
#include <utility>

namespace {

template <typename Callable, typename FailureCleanup>
HRESULT invokeComBoundary(Callable &&callable,
                          FailureCleanup &&failureCleanup) noexcept {
  try {
    return std::forward<Callable>(callable)();
  } catch (const std::bad_alloc &) {
    std::forward<FailureCleanup>(failureCleanup)();
    return E_OUTOFMEMORY;
  } catch (...) {
    std::forward<FailureCleanup>(failureCleanup)();
    return E_UNEXPECTED;
  }
}

template <typename Callable>
HRESULT invokeComBoundary(Callable &&callable) noexcept {
  return invokeComBoundary(std::forward<Callable>(callable), []() noexcept {});
}

struct ComReleaser {
  template <typename ComObject>
  void operator()(ComObject *object) const noexcept {
    if (object)
      object->Release();
  }
};

} // namespace

const qint64 MAX_THUMBNAIL_FILE_SIZE = 256 * 1024 * 1024; // 256 MB
const int MAX_THUMBNAIL_DIMENSION = 16384; // 16384 pixels
constexpr UINT MAX_REQUESTED_THUMBNAIL_EDGE = 1024;
constexpr qint64 MAX_RAW_PREVIEW_SOURCE_BYTES = 32 * 1024 * 1024;
constexpr qint64 MAX_RAW_PREVIEW_BITMAP_BYTES = 64 * 1024 * 1024;
constexpr qint64 MAX_SVG_SOURCE_BYTES = 8 * 1024 * 1024;
constexpr UINT MAX_SVG_THUMBNAIL_EDGE = 1024;
constexpr qint64 MAX_SVG_THUMBNAIL_BYTES =
    qint64(MAX_SVG_THUMBNAIL_EDGE) * MAX_SVG_THUMBNAIL_EDGE * 4;
constexpr int MAX_SVG_ELEMENT_COUNT = 10000;
constexpr int MAX_SVG_ATTRIBUTE_COUNT = 50000;
constexpr int MAX_SVG_XML_DEPTH = 64;
constexpr int MAX_SVG_REFERENCE_NODE_COUNT = 4096;
constexpr int MAX_SVG_REFERENCE_EDGE_COUNT = 16384;
constexpr int MAX_SVG_REFERENCE_DEPTH = 64;
constexpr double MAX_SVG_COORDINATE = 1000000.0;
constexpr double MAX_SVG_ASPECT_RATIO = 10000.0;

class QStreamDevice : public QIODevice {
public:
  QStreamDevice(IStream *stream, QObject *parent = nullptr)
      : QIODevice(parent), m_stream(stream) {
    open(QIODevice::ReadOnly);
  }

  bool isSequential() const override {
    return false; // Random-access (seekable) device
  }

  qint64 size() const override {
    STATSTG statstg;
    if (SUCCEEDED(m_stream->Stat(&statstg, STATFLAG_NONAME))) {
      return (qint64)statstg.cbSize.QuadPart;
    }
    return 0;
  }

  qint64 pos() const override {
    LARGE_INTEGER move;
    move.QuadPart = 0;
    ULARGE_INTEGER pos;
    if (SUCCEEDED(m_stream->Seek(move, STREAM_SEEK_CUR, &pos))) {
      return (qint64)pos.QuadPart;
    }
    return 0;
  }

  bool seek(qint64 pos) override {
    LARGE_INTEGER move;
    move.QuadPart = pos;
    ULARGE_INTEGER newPos;
    if (SUCCEEDED(m_stream->Seek(move, STREAM_SEEK_SET, &newPos))) {
      QIODevice::seek(pos); // Sync QIODevice's internal state
      return true;
    }
    return false;
  }

protected:
  qint64 readData(char *data, qint64 maxlen) override {
    ULONG read = 0;
    HRESULT hr = m_stream->Read(data, (ULONG)maxlen, &read);
    return SUCCEEDED(hr) ? read : -1;
  }
  qint64 writeData(const char *, qint64) override { return -1; }

private:
  IStream *m_stream;
};

struct ExtensionInfo {
  const wchar_t *dotExt;
  const char *qtFormat;
  bool isRaw;
  const wchar_t *progId;
};

static const ExtensionInfo g_extensionTable[] = {
  // RAW formats
  { L".arw",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".cr2",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".cr3",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".nef",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".dng",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".rw2",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".pef",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".raf",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".orf",  "raw",  true,  L"qimgvplus.AssocFile.raw" },
  { L".raw",  "raw",  true,  L"qimgvplus.AssocFile.raw" },

  // Krita / OpenRaster
  { L".kra",  "kra",  false, L"qimgvplus.AssocFile.kra" },
  { L".ora",  "ora",  false, L"qimgvplus.AssocFile.ora" },

  // Modern web/mobile formats
  { L".webp", nullptr, false, L"qimgvplus.AssocFile.webp" },
  { L".jxl",  nullptr, false, L"qimgvplus.AssocFile.jxl" },
  { L".avif", nullptr, false, L"qimgvplus.AssocFile.avif" },
  { L".heic", "heif",  false, L"qimgvplus.AssocFile.heic" },
  { L".heif", "heif",  false, L"qimgvplus.AssocFile.heif" },

  // High dynamic range
  { L".exr",  nullptr, false, L"qimgvplus.AssocFile.exr" },
  { L".hdr",  nullptr, false, L"qimgvplus.AssocFile.hdr" },

  // Other image formats
  { L".tga",  nullptr, false, L"qimgvplus.AssocFile.tga" },
  { L".jxr",  nullptr, false, L"qimgvplus.AssocFile.jxr" },
  { L".hdp",  nullptr, false, L"qimgvplus.AssocFile.jxr" },
  { L".wdp",  nullptr, false, L"qimgvplus.AssocFile.jxr" },

  // QOI and DDS
  { L".qoi",  nullptr, false, L"qimgvplus.AssocFile.qoi" },
  { L".dds",  nullptr, false, L"qimgvplus.AssocFile.dds" },

  // Photoshop
  { L".psd",  "psd",  false, L"qimgvplus.AssocFile.psd" },
  { L".psb",  "psd",  false, L"qimgvplus.AssocFile.psb" },

  // Adobe Illustrator
  { L".ai",   "pdf",  false, L"qimgvplus.AssocFile.ai" },

  // TIFF
  { L".tif",  "tiff", false, L"qimgvplus.AssocFile.tif" },
  { L".tiff", "tiff", false, L"qimgvplus.AssocFile.tiff" },

  // JPEG 2000
  { L".jp2",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".j2k",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".jpf",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".jpx",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".jpc",  nullptr, false, L"qimgvplus.AssocFile.jp2" },

  // HTJ2K / JPH
  { L".jph",  nullptr, false, L"qimgvplus.AssocFile.jph" },

  // SVG is rendered through the validated SVG-specific path below.
  { L".svg",  nullptr, false, L"qimgvplus.AssocFile.svg" }
};

using SvgReferenceGraph = QHash<QString, QSet<QString>>;

static bool isSvgExtension(const QString &ext) { return ext == u"svg"; }

static bool parseSafeSvgNumber(const QString &value, double &number) {
  bool ok = false;
  const double parsed = value.trimmed().toDouble(&ok);
  if (!ok || !std::isfinite(parsed) ||
      std::abs(parsed) > MAX_SVG_COORDINATE)
    return false;
  number = parsed;
  return true;
}

static bool parseSafeSvgLength(const QString &value, double &number) {
  QString text = value.trimmed();
  static const QStringList supportedUnits = {
      QStringLiteral("px"), QStringLiteral("pt"), QStringLiteral("pc"),
      QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("in"),
      QStringLiteral("em"), QStringLiteral("ex"), QStringLiteral("q"),
      QStringLiteral("%")};
  for (const QString &unit : supportedUnits) {
    if (text.endsWith(unit, Qt::CaseInsensitive)) {
      text.chop(unit.size());
      break;
    }
  }
  return parseSafeSvgNumber(text, number);
}

static bool hasSafeSvgAspectRatio(double width, double height) {
  if (width <= 0.0 || height <= 0.0)
    return false;
  const double ratio = width / height;
  return std::isfinite(ratio) && ratio >= 1.0 / MAX_SVG_ASPECT_RATIO &&
         ratio <= MAX_SVG_ASPECT_RATIO;
}

static bool containsSvgWhitespace(const QString &value) {
  return std::any_of(value.cbegin(), value.cend(),
                     [](const QChar character) { return character.isSpace(); });
}

static bool addSvgReference(const QString &reference, QSet<QString> &references) {
  const QString id = reference.trimmed();
  if (id.isEmpty() || containsSvgWhitespace(id))
    return false;
  references.insert(id);
  return true;
}

static bool collectSvgUrlReferences(const QString &value,
                                    QSet<QString> &references) {
  constexpr QChar closingParenthesis = u')';
  constexpr QChar idPrefix = u'#';
  constexpr QStringView urlPrefix = u"url(";

  int searchOffset = 0;
  while (true) {
    const int urlOffset = value.indexOf(urlPrefix, searchOffset,
                                        Qt::CaseInsensitive);
    if (urlOffset < 0)
      return true;

    const int referenceStart = urlOffset + urlPrefix.size();
    const int referenceEnd = value.indexOf(closingParenthesis, referenceStart);
    if (referenceEnd < 0)
      return false;

    QString reference =
        value.mid(referenceStart, referenceEnd - referenceStart).trimmed();
    if (reference.size() >= 2 &&
        ((reference.front() == u'\'' && reference.back() == u'\'') ||
         (reference.front() == u'"' && reference.back() == u'"'))) {
      reference = reference.sliced(1, reference.size() - 2).trimmed();
    }
    if (!reference.startsWith(idPrefix) ||
        !addSvgReference(reference.sliced(1), references))
      return false;

    searchOffset = referenceEnd + 1;
  }
}

static bool validateSvgReferenceGraph(const SvgReferenceGraph &graph) {
  enum class VisitState { NotVisited, Visiting, Visited };

  QHash<QString, VisitState> states;
  std::function<bool(const QString &, int)> visit =
      [&](const QString &id, int depth) {
        if (depth > MAX_SVG_REFERENCE_DEPTH)
          return false;

        const auto node = graph.constFind(id);
        if (node == graph.cend())
          return false;

        const VisitState state = states.value(id, VisitState::NotVisited);
        if (state == VisitState::Visiting)
          return false;
        if (state == VisitState::Visited)
          return true;

        states.insert(id, VisitState::Visiting);
        for (const QString &reference : *node) {
          if (!visit(reference, depth + 1))
            return false;
        }
        states.insert(id, VisitState::Visited);
        return true;
      };

  for (auto node = graph.cbegin(); node != graph.cend(); ++node) {
    if (!visit(node.key(), 0))
      return false;
  }
  return true;
}

static bool validateSvgViewBox(const QString &value) {
  const QStringList values =
      value.split(QRegularExpression(QStringLiteral("[\\s,]+")),
                  Qt::SkipEmptyParts);
  if (values.size() != 4)
    return false;

  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  return parseSafeSvgNumber(values[0], x) && parseSafeSvgNumber(values[1], y) &&
         parseSafeSvgNumber(values[2], width) &&
         parseSafeSvgNumber(values[3], height) &&
         hasSafeSvgAspectRatio(width, height);
}

static bool validateSvgDocument(const QByteArray &svg) {
  if (svg.isEmpty() || svg.size() > MAX_SVG_SOURCE_BYTES)
    return false;

  QXmlStreamReader xml(svg);
  SvgReferenceGraph graph;
  QVector<QString> activeIds;
  QVector<QString> elementIds;
  bool sawRoot = false;
  bool sawRootEnd = false;
  bool sawDoctype = false;
  int depth = 0;
  int elementCount = 0;
  int attributeCount = 0;
  int referenceEdgeCount = 0;
  int styleDepth = 0;
  QString styleText;
  QSet<QString> styleReferences;
  double rootWidth = 0.0;
  double rootHeight = 0.0;
  bool hasRootWidth = false;
  bool hasRootHeight = false;

  while (!xml.atEnd()) {
    const QXmlStreamReader::TokenType token = xml.readNext();
    if (token == QXmlStreamReader::DTD) {
      if (sawDoctype || sawRoot ||
          xml.dtdName().compare(u"svg", Qt::CaseInsensitive) != 0 ||
          !xml.entityDeclarations().isEmpty() ||
          !xml.notationDeclarations().isEmpty())
        return false;
      sawDoctype = true;
      continue;
    }
    if (token == QXmlStreamReader::EntityReference)
      return false;
    if (token == QXmlStreamReader::ProcessingInstruction &&
        xml.processingInstructionTarget().compare(
            u"xml-stylesheet", Qt::CaseInsensitive) == 0)
      return false;

    if (token == QXmlStreamReader::StartElement) {
      ++depth;
      if (depth > MAX_SVG_XML_DEPTH || ++elementCount > MAX_SVG_ELEMENT_COUNT)
        return false;

      const QString elementName = xml.name().toString().toLower();
      if (!sawRoot) {
        if (elementName != u"svg")
          return false;
        sawRoot = true;
      }

      if (elementName == u"script" || elementName == u"foreignobject" ||
          elementName == u"image" || elementName == u"animate" ||
          elementName == u"animatemotion" || elementName == u"animatecolor" ||
          elementName == u"animatetransform" || elementName == u"set" ||
          elementName == u"audio" || elementName == u"video" ||
          elementName == u"iframe" || elementName == u"object" ||
          elementName == u"embed")
        return false;

      if (elementName == u"style") {
        if (styleDepth != 0)
          return false;
        styleDepth = depth;
        styleText.clear();
      }

      const QXmlStreamAttributes attributes = xml.attributes();
      attributeCount += attributes.size();
      if (attributeCount > MAX_SVG_ATTRIBUTE_COUNT)
        return false;

      QString elementId;
      for (const QXmlStreamAttribute &attribute : attributes) {
        if (attribute.name() == u"id") {
          elementId = attribute.value().toString().trimmed();
          break;
        }
      }

      if (!elementId.isEmpty()) {
        if (containsSvgWhitespace(elementId) || graph.contains(elementId) ||
            graph.size() >= MAX_SVG_REFERENCE_NODE_COUNT)
          return false;
        graph.insert(elementId, {});
        activeIds.append(elementId);
      }
      elementIds.append(elementId);

      QSet<QString> references;
      for (const QXmlStreamAttribute &attribute : attributes) {
        const QString name = attribute.name().toString().toLower();
        const QString value = attribute.value().toString().trimmed();
        if (name.startsWith(u"on"))
          return false;

        if (name == u"href") {
          if (!value.startsWith(u'#') ||
              !addSvgReference(value.sliced(1), references))
            return false;
        }
        if (!collectSvgUrlReferences(value, references))
          return false;

        if (name == u"viewbox" && !validateSvgViewBox(value))
          return false;
        if (name == u"width" || name == u"height") {
          double dimension = 0.0;
          if (!parseSafeSvgLength(value, dimension))
            return false;
          if (depth == 1 && name == u"width") {
            if (dimension <= 0.0)
              return false;
            rootWidth = dimension;
            hasRootWidth = true;
          } else if (depth == 1 && name == u"height") {
            if (dimension <= 0.0)
              return false;
            rootHeight = dimension;
            hasRootHeight = true;
          }
        }
      }

      for (const QString &ownerId : activeIds) {
        QSet<QString> &ownerReferences = graph[ownerId];
        for (const QString &reference : references) {
          if (!ownerReferences.contains(reference)) {
            if (++referenceEdgeCount > MAX_SVG_REFERENCE_EDGE_COUNT)
              return false;
            ownerReferences.insert(reference);
          }
        }
      }
    } else if (token == QXmlStreamReader::Characters && styleDepth != 0) {
      styleText.append(xml.text());
    } else if (token == QXmlStreamReader::EndElement) {
      if (depth == styleDepth) {
        if (styleText.contains(u"@import", Qt::CaseInsensitive) ||
            !collectSvgUrlReferences(styleText, styleReferences))
          return false;
        styleDepth = 0;
        styleText.clear();
      }
      if (depth == 1)
        sawRootEnd = true;
      if (elementIds.isEmpty())
        return false;
      if (!elementIds.takeLast().isEmpty())
        activeIds.removeLast();
      if (--depth < 0)
        return false;
    }
  }

  for (const QString &reference : styleReferences) {
    if (!graph.contains(reference))
      return false;
  }

  return !xml.hasError() && sawRoot && sawRootEnd && depth == 0 &&
         styleDepth == 0 && elementIds.isEmpty() && activeIds.isEmpty() &&
         (!hasRootWidth || !hasRootHeight ||
          hasSafeSvgAspectRatio(rootWidth, rootHeight)) &&
         validateSvgReferenceGraph(graph);
}

static bool readSvgFile(const QString &path, QByteArray &svg) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() < 0 ||
      file.size() > MAX_SVG_SOURCE_BYTES)
    return false;
  svg = file.read(MAX_SVG_SOURCE_BYTES + 1);
  return svg.size() <= MAX_SVG_SOURCE_BYTES;
}

static bool readSvgStream(IStream *stream, QByteArray &svg) {
  if (!stream)
    return false;

  LARGE_INTEGER streamStart = {};
  if (FAILED(stream->Seek(streamStart, STREAM_SEEK_SET, nullptr)))
    return false;

  constexpr ULONG streamChunkSize = 64 * 1024;
  char chunk[streamChunkSize];
  bool complete = true;
  while (complete) {
    ULONG bytesRead = 0;
    const HRESULT result = stream->Read(chunk, streamChunkSize, &bytesRead);
    if (FAILED(result) ||
        svg.size() > MAX_SVG_SOURCE_BYTES - static_cast<qint64>(bytesRead)) {
      complete = false;
      break;
    }
    svg.append(chunk, static_cast<qsizetype>(bytesRead));
    if (bytesRead == 0)
      break;
  }

  return SUCCEEDED(stream->Seek(streamStart, STREAM_SEEK_SET, nullptr)) &&
         complete;
}

static HBITMAP renderSvgThumbnail(const QByteArray &svg, UINT requestedEdge) {
  if (!validateSvgDocument(svg) || requestedEdge == 0)
    return nullptr;

  const UINT edge = std::min(requestedEdge, MAX_SVG_THUMBNAIL_EDGE);
  QSvgRenderer renderer;
  if (!renderer.load(svg) || !renderer.isValid())
    return nullptr;

  QSizeF sourceSize(renderer.defaultSize());
  if (sourceSize.isEmpty())
    sourceSize = renderer.viewBoxF().size();
  if (sourceSize.isEmpty() ||
      !hasSafeSvgAspectRatio(sourceSize.width(), sourceSize.height()))
    return nullptr;

  const double scale = static_cast<double>(edge) /
                       std::max(sourceSize.width(), sourceSize.height());
  const QSize thumbnailSize(
      std::max(1, qRound(sourceSize.width() * scale)),
      std::max(1, qRound(sourceSize.height() * scale)));
  QImage image(thumbnailSize, QImage::Format_ARGB32_Premultiplied);
  if (image.isNull() || image.sizeInBytes() > MAX_SVG_THUMBNAIL_BYTES)
    return nullptr;
  image.fill(Qt::transparent);

  renderer.setAspectRatioMode(Qt::KeepAspectRatio);
  QPainter painter(&image);
  if (!painter.isActive())
    return nullptr;
  renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(image.size())));
  painter.end();

  BITMAPV5HEADER header = {};
  header.bV5Size = sizeof(header);
  header.bV5Width = image.width();
  header.bV5Height = -image.height();
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  void *bits = nullptr;
  HDC desktopDc = GetDC(nullptr);
  if (!desktopDc)
    return nullptr;
  HBITMAP bitmap = CreateDIBSection(desktopDc,
                                    reinterpret_cast<BITMAPINFO *>(&header),
                                    DIB_RGB_COLORS, &bits, nullptr, 0);
  ReleaseDC(nullptr, desktopDc);
  if (!bitmap || !bits) {
    if (bitmap)
      DeleteObject(bitmap);
    return nullptr;
  }

  memcpy(bits, image.constBits(), static_cast<size_t>(image.sizeInBytes()));
  return bitmap;
}

static bool isRawExtension(const QString &ext) {
  for (const auto &info : g_extensionTable) {
    if (info.isRaw) {
      if (ext == QString::fromWCharArray(info.dotExt + 1)) {
        return true;
      }
    }
  }
  return false;
}

static QImage
createQImageFromLibRawImage(const libraw_processed_image_t *processedImage,
                            UINT requestedEdge) {
  if (!processedImage || !processedImage->data ||
      processedImage->data_size == 0)
    return QImage();

  if (processedImage->width > MAX_THUMBNAIL_DIMENSION ||
      processedImage->height > MAX_THUMBNAIL_DIMENSION)
    return QImage();

  if (processedImage->type == LIBRAW_IMAGE_JPEG) {
    if (qint64(processedImage->data_size) > MAX_RAW_PREVIEW_SOURCE_BYTES)
      return QImage();

    QByteArray previewData = QByteArray::fromRawData(
        reinterpret_cast<const char *>(processedImage->data),
        qsizetype(processedImage->data_size));
    QBuffer previewBuffer(&previewData);
    if (!previewBuffer.open(QIODevice::ReadOnly))
      return QImage();

    QImageReader previewReader(&previewBuffer, "JPEG");
    QSize previewSize = previewReader.size();
    if (!previewSize.isValid() ||
        previewSize.width() > MAX_THUMBNAIL_DIMENSION ||
        previewSize.height() > MAX_THUMBNAIL_DIMENSION)
      return QImage();

    previewSize.scale(int(requestedEdge), int(requestedEdge),
                      Qt::KeepAspectRatio);
    previewReader.setScaledSize(previewSize);

    QImage image;
    if (!previewReader.read(&image))
      return QImage();
    return image;
  }

  if (processedImage->type != LIBRAW_IMAGE_BITMAP || processedImage->bits != 8)
    return QImage();

  if (qint64(processedImage->data_size) > MAX_RAW_PREVIEW_BITMAP_BYTES)
    return QImage();

  QImage::Format format = QImage::Format_Invalid;
  if (processedImage->colors == 3) {
    format = QImage::Format_RGB888;
  } else if (processedImage->colors == 1) {
    format = QImage::Format_Grayscale8;
  } else {
    return QImage();
  }

  qint64 need = qint64(processedImage->width) * processedImage->colors;
  if (processedImage->width <= 0 || processedImage->height <= 0 ||
      need <= 0 || need * processedImage->height > qint64(processedImage->data_size))
    return QImage();

  const int bytesPerLine = int(need);
  QImage img(reinterpret_cast<const uchar *>(processedImage->data),
             processedImage->width, processedImage->height, bytesPerLine,
             format);
  if (img.width() > int(requestedEdge) ||
      img.height() > int(requestedEdge)) {
    return img.scaled(int(requestedEdge), int(requestedEdge),
                      Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  return img.copy();
}

static bool tryLibRawEmbeddedPreview(const std::wstring &filePath,
                                     UINT requestedEdge,
                                     QImage &outImg) {
  if (filePath.empty())
    return false;

  LibRaw raw;
  if (raw.open_file(filePath.c_str()) != LIBRAW_SUCCESS)
    return false;

  if (qint64(raw.imgdata.thumbnail.tlength) >
      MAX_RAW_PREVIEW_SOURCE_BYTES) {
    raw.recycle();
    return false;
  }

  if (raw.unpack_thumb() == LIBRAW_SUCCESS) {
    std::unique_ptr<libraw_processed_image_t,
                    decltype(&LibRaw::dcraw_clear_mem)>
        thumb(raw.dcraw_make_mem_thumb(), LibRaw::dcraw_clear_mem);
    if (thumb) {
      QImage img =
          createQImageFromLibRawImage(thumb.get(), requestedEdge);
      if (!img.isNull()) {
        outImg = std::move(img);
        raw.recycle();
        return true;
      }
    }
  }

  raw.recycle();
  return false;
}



// {978A692C-CD23-4A59-8664-98F1E1B9200B}
const CLSID CLSID_QImgvThumbnailProvider = {
    0x978a692c,
    0xcd23,
    0x4a59,
    {0x86, 0x64, 0x98, 0xf1, 0xe1, 0xb9, 0x20, 0x0b}};
const wchar_t *CLSID_QImgvThumbnailProvider_Str =
    L"{978A692C-CD23-4A59-8664-98F1E1B9200B}";

long g_cDllRef = 0;




// Helper functions for registry manipulation
HRESULT CreateRegistryKeyAndValue(HKEY hKeyParent, LPCWSTR pszSubKey,
                                  LPCWSTR pszValueName, LPCWSTR pszValue) {
  HKEY hKey = nullptr;
  LSTATUS status = RegCreateKeyExW(hKeyParent, pszSubKey, 0, nullptr,
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                   &hKey, nullptr);
  if (status != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(status);

  if (pszValue) {
    status =
        RegSetValueExW(hKey, pszValueName, 0, REG_SZ, (const BYTE *)pszValue,
                       (DWORD)(wcslen(pszValue) + 1) * sizeof(wchar_t));
  }
  RegCloseKey(hKey);
  return HRESULT_FROM_WIN32(status);
}

HRESULT CreateRegistryKeyAndDwordValue(HKEY hKeyParent, LPCWSTR pszSubKey,
                                       LPCWSTR pszValueName, DWORD dwValue) {
  HKEY hKey = nullptr;
  LSTATUS status = RegCreateKeyExW(hKeyParent, pszSubKey, 0, nullptr,
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr,
                                   &hKey, nullptr);
  if (status != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(status);

  status = RegSetValueExW(hKey, pszValueName, 0, REG_DWORD,
                          (const BYTE *)&dwValue, sizeof(dwValue));
  RegCloseKey(hKey);
  return HRESULT_FROM_WIN32(status);
}

HRESULT DeleteRegistryKey(HKEY hKeyParent, LPCWSTR pszSubKey) {
  LSTATUS status = RegDeleteTreeW(hKeyParent, pszSubKey);
  return HRESULT_FROM_WIN32(status);
}

// QImgvThumbnailProvider Implementation
QImgvThumbnailProvider::QImgvThumbnailProvider()
    : m_cRef(1), m_pStream(nullptr) {
  InterlockedIncrement(&g_cDllRef);
  m_szFilePath.clear();
}

QImgvThumbnailProvider::~QImgvThumbnailProvider() {
  InterlockedDecrement(&g_cDllRef);
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
}

// IUnknown Methods
IFACEMETHODIMP QImgvThumbnailProvider::QueryInterface(REFIID riid,
                                                      void **ppv) noexcept {
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;

  return invokeComBoundary(
      [&]() -> HRESULT {
        static const QITAB qit[] = {
            QITABENT(QImgvThumbnailProvider, IThumbnailProvider),
            QITABENT(QImgvThumbnailProvider, IInitializeWithFile),
            QITABENT(QImgvThumbnailProvider, IInitializeWithStream),
            {0},
        };
        return QISearch(this, qit, riid, ppv);
      },
      [&]() noexcept { *ppv = nullptr; });
}

IFACEMETHODIMP_(ULONG) QImgvThumbnailProvider::AddRef() noexcept {
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) QImgvThumbnailProvider::Release() noexcept {
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

// IInitializeWithFile Methods
IFACEMETHODIMP QImgvThumbnailProvider::Initialize(LPCWSTR pszFilePath,
                                                  DWORD grfMode) noexcept {
  return invokeComBoundary([&]() -> HRESULT {
    if (!pszFilePath || *pszFilePath == L'\0')
      return E_INVALIDARG;

    std::wstring filePath(pszFilePath);
    IStream *previousStream = std::exchange(m_pStream, nullptr);
    m_szFilePath.swap(filePath);
    if (previousStream)
      previousStream->Release();

    return S_OK;
  });
}

// IInitializeWithStream Methods
IFACEMETHODIMP QImgvThumbnailProvider::Initialize(IStream *pStream,
                                                  DWORD grfMode) noexcept {
  return invokeComBoundary([&]() -> HRESULT {
    if (!pStream)
      return E_INVALIDARG;

    pStream->AddRef();
    IStream *previousStream = std::exchange(m_pStream, pStream);
    m_szFilePath.clear();
    if (previousStream)
      previousStream->Release();

    return S_OK;
  });
}

// RAII class to manage DLL loading and search path restoration
class DllEnvironmentManager {
public:
  DllEnvironmentManager(const wchar_t *dllDir) {
    m_restored = FALSE;
    m_hCore = nullptr;
    m_hGui = nullptr;

    if (dllDir && dllDir[0] != L'\0') {
      m_restored = SetDllDirectoryW(dllDir);

      wchar_t corePath[MAX_PATH];
      wchar_t guiPath[MAX_PATH];
      swprintf_s(corePath, MAX_PATH, L"%s\\Qt6Core.dll", dllDir);
      swprintf_s(guiPath, MAX_PATH, L"%s\\Qt6Gui.dll", dllDir);

      m_hCore = LoadLibraryW(corePath);
      m_hGui = LoadLibraryW(guiPath);
    }
  }
  ~DllEnvironmentManager() {
    if (m_hGui)
      FreeLibrary(m_hGui);
    if (m_hCore)
      FreeLibrary(m_hCore);
    if (m_restored) {
      SetDllDirectoryW(nullptr);
    }
  }

private:
  BOOL m_restored;
  HMODULE m_hCore;
  HMODULE m_hGui;
};

// IThumbnailProvider Methods
IFACEMETHODIMP QImgvThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp,
                                                    WTS_ALPHATYPE *pdwAlpha)
    noexcept {
  if (!phbmp || !pdwAlpha)
    return E_POINTER;
  *phbmp = nullptr;
  *pdwAlpha = WTSAT_UNKNOWN;
  if (cx == 0)
    return E_INVALIDARG;

  cx = std::min(cx, MAX_REQUESTED_THUMBNAIL_EDGE);

  if (m_szFilePath.empty() && !m_pStream) {
    return E_FAIL;
  }

  return invokeComBoundary([&]() -> HRESULT {
    // Retrieve absolute DLL directory path and set up DLL search environment
    HMODULE hModule = nullptr;
    wchar_t dllDir[MAX_PATH] = L"";
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&DllGetClassObject, &hModule)) {
      GetModuleFileNameW(hModule, dllDir, MAX_PATH);
      PathRemoveFileSpecW(dllDir);
    }
    DllEnvironmentManager envManager(dllDir);

    // Initialize Qt and its image format plugin path once across all thumbnail
    // provider threads.
    static std::once_flag qtInitialization;
    std::call_once(qtInitialization, [dllDir] {
      static int argc = 1;
      static char appName[] = "qimgvshellex.dll";
      static char *argv[] = {appName, nullptr};
      if (!QCoreApplication::instance()) {
        new QCoreApplication(argc, argv);
      }

      if (dllDir[0] != L'\0') {
        QString qPath = QString::fromWCharArray(dllDir);
        QCoreApplication::addLibraryPath(qPath);
      }
    });

    QString ext;
    if (m_pStream) {
      STATSTG statstg;
      if (SUCCEEDED(m_pStream->Stat(&statstg, STATFLAG_DEFAULT))) {
        if ((qint64)statstg.cbSize.QuadPart > MAX_THUMBNAIL_FILE_SIZE) {
          if (statstg.pwcsName) {
            CoTaskMemFree(statstg.pwcsName);
          }
          return E_FAIL;
        }
        if (statstg.pwcsName) {
          ext = QFileInfo(QString::fromWCharArray(statstg.pwcsName)).suffix().toLower();
          CoTaskMemFree(statstg.pwcsName);
        }
      }
    } else {
      QFileInfo fileInfo(QString::fromStdWString(m_szFilePath));
      if (fileInfo.size() > MAX_THUMBNAIL_FILE_SIZE) {
        return E_FAIL;
      }
      ext = fileInfo.suffix().toLower();
    }

    const bool knownSvg = isSvgExtension(ext);
    if (knownSvg || (ext.isEmpty() && m_pStream)) {
      QByteArray svg;
      const bool readSucceeded =
          m_pStream ? readSvgStream(m_pStream, svg)
                    : readSvgFile(QString::fromStdWString(m_szFilePath), svg);
      HBITMAP bitmap = readSucceeded ? renderSvgThumbnail(svg, cx) : nullptr;
      if (!bitmap && knownSvg)
        return E_FAIL;
      if (bitmap) {
        *phbmp = bitmap;
        *pdwAlpha = WTSAT_ARGB;
        return S_OK;
      }
    }

    // Avoid content-based selection of the installed RAW image plugin when a
    // stream does not expose a file name.
    if (ext.isEmpty())
      return E_FAIL;

  if (isRawExtension(ext)) {
    if (m_szFilePath.empty())
      return E_FAIL;

    QImage rawImg;
    if (!tryLibRawEmbeddedPreview(m_szFilePath, cx, rawImg) || rawImg.isNull())
      return E_FAIL;

    if (rawImg.width() > (int)cx || rawImg.height() > (int)cx) {
      rawImg = rawImg.scaled(cx, cx, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    if (rawImg.format() != QImage::Format_ARGB32_Premultiplied) {
      rawImg = rawImg.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    BITMAPV5HEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = rawImg.width();
    bi.bV5Height = -rawImg.height(); // Top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void *pBits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hbmp = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
                                    &pBits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (hbmp && pBits) {
      memcpy(pBits, rawImg.constBits(), rawImg.sizeInBytes());
      *phbmp = hbmp;
      *pdwAlpha = WTSAT_ARGB;
      return S_OK;
    }

    if (hbmp)
      DeleteObject(hbmp);
    return E_FAIL;
  }

  // Fallback / standard path: set up QImageReader
  std::unique_ptr<QStreamDevice> streamDevice;
  QImageReader reader;
  QByteArray format;

  if (!ext.isEmpty()) {
    const ExtensionInfo *found = nullptr;
    for (const auto &info : g_extensionTable) {
      if (ext == QString::fromWCharArray(info.dotExt + 1)) {
        found = &info;
        break;
      }
    }

    if (found) {
      if (found->qtFormat) {
        format = found->qtFormat;
      } else {
        format = ext.toUtf8();
      }
    } else {
      format = ext.toUtf8();
    }
  }

  qint64 streamSize = 0;
  bool seekable = false;
  LARGE_INTEGER savedPos = {0};

  if (m_pStream) {
    // Determine stream size using end seek
    LARGE_INTEGER zero = {0};
    if (SUCCEEDED(m_pStream->Seek(zero, STREAM_SEEK_CUR,
                                  (ULARGE_INTEGER *)&savedPos))) {
      ULARGE_INTEGER endPos;
      if (SUCCEEDED(m_pStream->Seek(zero, STREAM_SEEK_END, &endPos))) {
        streamSize = endPos.QuadPart;
        seekable = true;
        // Restore original position
        m_pStream->Seek(savedPos, STREAM_SEEK_SET, nullptr);
      }
    }

    streamDevice = std::make_unique<QStreamDevice>(m_pStream);
    reader.setDevice(streamDevice.get());
  } else {
    QString filePath = QString::fromStdWString(m_szFilePath);
    reader.setFileName(filePath);
  }

  if (!format.isEmpty()) {
    reader.setFormat(format);
  }

  reader.setAutoTransform(true); // Rotate according to Exif orientation tags

  // If the format has multiple images (e.g. ICO), choose the one closest to
  // requested size cx
  int imageCount = reader.imageCount();
  if (imageCount > 1) {
    int bestIndex = 0;
    int bestDiff = 999999;
    for (int i = 0; i < imageCount; ++i) {
      if (reader.jumpToImage(i)) {
        QSize frameSize = reader.size();
        if (frameSize.isValid()) {
          int diff = qAbs(frameSize.width() - (int)cx);
          if (diff < bestDiff) {
            bestDiff = diff;
            bestIndex = i;
          }
        }
      }
    }
    reader.jumpToImage(bestIndex);
  }

  // Scale size during read if supported by reader for major performance gains
  QSize size = reader.size();
  if (size.isValid()) {
    if (size.width() > MAX_THUMBNAIL_DIMENSION || size.height() > MAX_THUMBNAIL_DIMENSION) {
      return E_FAIL;
    }
    size.scale(cx, cx, Qt::KeepAspectRatio);
    reader.setScaledSize(size);
  }

  QImage img;
  if (!reader.read(&img)) {
    if (m_pStream) {
      // Fallback for stream: try memory buffer if small, otherwise temporary file
      if (seekable && streamSize > 0 &&
          streamSize <= 1024 * 1024) { // up to 1 MB in memory
        QByteArray buffer;
        buffer.resize(streamSize);
        LARGE_INTEGER zero = {0};
        m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
        ULONG bytesRead = 0;
        HRESULT hr =
            m_pStream->Read(buffer.data(), (ULONG)streamSize, &bytesRead);
        if (SUCCEEDED(hr) && bytesRead == streamSize) {
          QBuffer memBuffer(&buffer);
          memBuffer.open(QIODevice::ReadOnly);
          QImageReader memReader(&memBuffer);
          if (!format.isEmpty())
            memReader.setFormat(format);
          memReader.setAutoTransform(true);
          QSize sz = memReader.size();
          if (sz.isValid()) {
            if (sz.width() > MAX_THUMBNAIL_DIMENSION || sz.height() > MAX_THUMBNAIL_DIMENSION) {
              return E_FAIL;
            }
            sz.scale(cx, cx, Qt::KeepAspectRatio);
            memReader.setScaledSize(sz);
          }
          if (memReader.read(&img)) {
            // success
          }
        }
      } else {
        if (seekable && streamSize > MAX_THUMBNAIL_FILE_SIZE) {
          return E_FAIL;
        }

        QTemporaryFile tempFile;
        tempFile.setFileTemplate(QDir::tempPath() + "/qimgv_XXXXXX");
        if (tempFile.open()) {
          // Rewind stream and copy data
          LARGE_INTEGER zero = {0};
          m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
          char chunk[65536];
          ULONG read;
          qint64 totalCopied = 0;
          bool writeFailed = false;

          while (SUCCEEDED(m_pStream->Read(chunk, sizeof(chunk), &read)) &&
                 read > 0) {
            totalCopied += read;
            if (totalCopied > MAX_THUMBNAIL_FILE_SIZE) {
              writeFailed = true;
              break;
            }
            if (tempFile.write(chunk, read) != (qint64)read) {
              writeFailed = true;
              break;
            }
          }

          if (writeFailed) {
            tempFile.close();
            tempFile.remove();
            return E_FAIL;
          }

          tempFile.flush();

          QImageReader fileReader(tempFile.fileName());
          if (!format.isEmpty())
            fileReader.setFormat(format);
          fileReader.setAutoTransform(true);
          QSize sz = fileReader.size();
          if (sz.isValid()) {
            if (sz.width() > MAX_THUMBNAIL_DIMENSION || sz.height() > MAX_THUMBNAIL_DIMENSION) {
              return E_FAIL;
            }
            sz.scale(cx, cx, Qt::KeepAspectRatio);
            fileReader.setScaledSize(sz);
          }
          if (fileReader.read(&img)) {
            // success
          }
          // QTemporaryFile automatically removes itself on destruction
        }
      }
    }

    if (img.isNull()) {
      return E_FAIL;
    }
  }

  if (img.width() > MAX_THUMBNAIL_DIMENSION || img.height() > MAX_THUMBNAIL_DIMENSION) {
    return E_FAIL;
  }

  // Downscale manually if the reader didn't scale it
  if (img.width() > (int)cx || img.height() > (int)cx) {
    img = img.scaled(cx, cx, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  // Convert to ARGB32 for alpha channel support
  if (img.format() != QImage::Format_ARGB32_Premultiplied) {
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  }

  // Convert QImage to HBITMAP (standard top-down 32-bit DIB section)
  BITMAPV5HEADER bi;
  ZeroMemory(&bi, sizeof(bi));
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = img.width();
  bi.bV5Height = -img.height(); // Top-down
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  void *pBits = nullptr;
  HDC hdc = GetDC(nullptr);
  HBITMAP hbmp = CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
                                  &pBits, nullptr, 0);
  ReleaseDC(nullptr, hdc);

  if (!hbmp || !pBits) {
    return E_FAIL;
  }

    memcpy(pBits, img.constBits(), img.sizeInBytes());
    *phbmp = hbmp;
    *pdwAlpha = WTSAT_ARGB; // Enable alpha transparency channel
    return S_OK;
  },
                           [&]() noexcept {
                             if (*phbmp)
                               DeleteObject(*phbmp);
                             *phbmp = nullptr;
                             *pdwAlpha = WTSAT_UNKNOWN;
                           });
}

// QImgvThumbnailProviderClassFactory Implementation
QImgvThumbnailProviderClassFactory::QImgvThumbnailProviderClassFactory()
    : m_cRef(1) {
  InterlockedIncrement(&g_cDllRef);
}

QImgvThumbnailProviderClassFactory::~QImgvThumbnailProviderClassFactory() {
  InterlockedDecrement(&g_cDllRef);
}

// IUnknown Methods
IFACEMETHODIMP QImgvThumbnailProviderClassFactory::QueryInterface(REFIID riid,
                                                                  void **ppv)
    noexcept {
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;

  return invokeComBoundary(
      [&]() -> HRESULT {
        static const QITAB qit[] = {
            QITABENT(QImgvThumbnailProviderClassFactory, IClassFactory),
            {0},
        };
        return QISearch(this, qit, riid, ppv);
      },
      [&]() noexcept { *ppv = nullptr; });
}

IFACEMETHODIMP_(ULONG)
QImgvThumbnailProviderClassFactory::AddRef() noexcept {
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG)
QImgvThumbnailProviderClassFactory::Release() noexcept {
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

// IClassFactory Methods
IFACEMETHODIMP
QImgvThumbnailProviderClassFactory::CreateInstance(IUnknown *pUnkOuter,
                                                   REFIID riid,
                                                   void **ppv) noexcept {
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;
  if (pUnkOuter != nullptr)
    return CLASS_E_NOAGGREGATION;

  return invokeComBoundary(
      [&]() -> HRESULT {
        std::unique_ptr<QImgvThumbnailProvider, ComReleaser> provider(
            new QImgvThumbnailProvider());
        return provider->QueryInterface(riid, ppv);
      },
      [&]() noexcept { *ppv = nullptr; });
}

IFACEMETHODIMP
QImgvThumbnailProviderClassFactory::LockServer(BOOL fLock) noexcept {
  return invokeComBoundary([&]() -> HRESULT {
    if (fLock) {
      InterlockedIncrement(&g_cDllRef);
    } else {
      InterlockedDecrement(&g_cDllRef);
    }
    return S_OK;
  });
}

// COM DLL Exported functions
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
  if (ppv == nullptr)
    return E_POINTER;
  *ppv = nullptr;

  return invokeComBoundary(
      [&]() -> HRESULT {
        if (rclsid == CLSID_QImgvThumbnailProvider) {
          std::unique_ptr<QImgvThumbnailProviderClassFactory, ComReleaser>
              factory(new QImgvThumbnailProviderClassFactory());
          return factory->QueryInterface(riid, ppv);
        }
        return CLASS_E_CLASSNOTAVAILABLE;
      },
      [&]() noexcept { *ppv = nullptr; });
}

STDAPI DllCanUnloadNow() {
  return invokeComBoundary(
      []() -> HRESULT { return (g_cDllRef == 0) ? S_OK : S_FALSE; });
}

STDAPI DllRegisterServer() {
  return invokeComBoundary([]() -> HRESULT {
    HMODULE hModule = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&DllGetClassObject, &hModule)) {
      return HRESULT_FROM_WIN32(GetLastError());
    }

    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(hModule, dllPath, MAX_PATH);

    // Register CLSID
    wchar_t clsidKey[MAX_PATH];
    swprintf_s(clsidKey, MAX_PATH, L"Software\\Classes\\CLSID\\%s",
               CLSID_QImgvThumbnailProvider_Str);

    HRESULT hr = CreateRegistryKeyAndValue(
        HKEY_CURRENT_USER, clsidKey, nullptr,
        L"qimgv-plus Thumbnail Provider");
    if (FAILED(hr))
      return hr;

    // Prefer surrogate/process isolation (DisableProcessIsolation = 0)
    hr = CreateRegistryKeyAndDwordValue(HKEY_CURRENT_USER, clsidKey,
                                        L"DisableProcessIsolation", 0);
    if (FAILED(hr))
      return hr;

    wchar_t inprocKey[MAX_PATH];
    swprintf_s(inprocKey, MAX_PATH, L"%s\\InprocServer32", clsidKey);
    hr = CreateRegistryKeyAndValue(HKEY_CURRENT_USER, inprocKey, nullptr,
                                   dllPath);
    if (FAILED(hr))
      return hr;

    hr = CreateRegistryKeyAndValue(HKEY_CURRENT_USER, inprocKey,
                                   L"ThreadingModel", L"Apartment");
    if (FAILED(hr))
      return hr;

    // Register for direct Extensions
    for (const auto &info : g_extensionTable) {
      wchar_t extKey[MAX_PATH];
      swprintf_s(extKey, MAX_PATH,
                 L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
                 L"234630154E96}",
                 info.dotExt);
      CreateRegistryKeyAndValue(HKEY_CURRENT_USER, extKey, nullptr,
                                CLSID_QImgvThumbnailProvider_Str);
    }

    // Register for ProgIDs
    for (size_t i = 0;
         i < sizeof(g_extensionTable) / sizeof(g_extensionTable[0]); ++i) {
      const auto &info = g_extensionTable[i];
      if (info.progId) {
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j) {
          if (g_extensionTable[j].progId &&
              wcscmp(g_extensionTable[j].progId, info.progId) == 0) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          wchar_t progIdKey[MAX_PATH];
          swprintf_s(
              progIdKey, MAX_PATH,
              L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
              L"234630154E96}",
              info.progId);
          CreateRegistryKeyAndValue(HKEY_CURRENT_USER, progIdKey, nullptr,
                                    CLSID_QImgvThumbnailProvider_Str);
        }
      }
    }

    // Notify shell about changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
  });
}

STDAPI DllUnregisterServer() {
  return invokeComBoundary([]() -> HRESULT {
    wchar_t clsidKey[MAX_PATH];
    swprintf_s(clsidKey, MAX_PATH, L"Software\\Classes\\CLSID\\%s",
               CLSID_QImgvThumbnailProvider_Str);
    DeleteRegistryKey(HKEY_CURRENT_USER, clsidKey);

    // Unregister for direct Extensions
    for (const auto &info : g_extensionTable) {
      wchar_t extKey[MAX_PATH];
      swprintf_s(extKey, MAX_PATH,
                 L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
                 L"234630154E96}",
                 info.dotExt);
      DeleteRegistryKey(HKEY_CURRENT_USER, extKey);
    }

    // Unregister for ProgIDs
    for (size_t i = 0;
         i < sizeof(g_extensionTable) / sizeof(g_extensionTable[0]); ++i) {
      const auto &info = g_extensionTable[i];
      if (info.progId) {
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j) {
          if (g_extensionTable[j].progId &&
              wcscmp(g_extensionTable[j].progId, info.progId) == 0) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          wchar_t progIdKey[MAX_PATH];
          swprintf_s(
              progIdKey, MAX_PATH,
              L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
              L"234630154E96}",
              info.progId);
          DeleteRegistryKey(HKEY_CURRENT_USER, progIdKey);
        }
      }
    }

    // Notify shell about changes
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
  });
}
