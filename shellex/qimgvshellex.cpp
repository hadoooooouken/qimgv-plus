// shellex/qimgvshellex.cpp
#include "qimgvshellex.h"
#include "djvu.h"
#include <memory>
#include <shlwapi.h>

// Qt Headers
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include <limits>
#include <new>
#include <mutex>
#include <string_view>
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
constexpr int MAX_DDS_MIP_LEVEL_COUNT = 32;
constexpr ULONG STREAM_COPY_CHUNK_SIZE = 64 * 1024;
constexpr qint64 MAX_IN_MEMORY_FALLBACK_BYTES = 1024 * 1024;
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
  { L".cbz",  "cbz",  false, L"qimgvplus.AssocFile.cbz" },

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

  // DjVu
  { L".djvu", nullptr, false, L"qimgvplus.AssocFile.djvu" },
  { L".djv",  nullptr, false, L"qimgvplus.AssocFile.djvu" },

  // TIFF
  { L".tif",  "tiff", false, L"qimgvplus.AssocFile.tif" },
  { L".tiff", "tiff", false, L"qimgvplus.AssocFile.tiff" },

  // JPEG 2000
  { L".jp2",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".j2k",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".jpf",  nullptr, false, L"qimgvplus.AssocFile.jp2" },
  { L".jpx",  "jp2",  false, L"qimgvplus.AssocFile.jp2" },
  { L".jpc",  "j2k",  false, L"qimgvplus.AssocFile.jp2" },

  // HTJ2K / JPH
  { L".jph",  nullptr, false, L"qimgvplus.AssocFile.jph" },

  // SVG is rendered through the validated SVG-specific path below.
  { L".svg",  nullptr, false, L"qimgvplus.AssocFile.svg" }
};

static wchar_t asciiLower(wchar_t character) noexcept {
  if (character >= L'A' && character <= L'Z')
    return character + (L'a' - L'A');
  return character;
}

static std::wstring_view
fileExtension(std::wstring_view fileName) noexcept {
  if (fileName.empty())
    return {};

  const size_t separatorPosition = fileName.find_last_of(L"\\/");
  const size_t extensionPosition = fileName.find_last_of(L'.');
  if (extensionPosition == std::wstring_view::npos ||
      extensionPosition + 1 == fileName.size() ||
      (separatorPosition != std::wstring_view::npos &&
       extensionPosition < separatorPosition))
    return {};

  return fileName.substr(extensionPosition);
}

static const ExtensionInfo *
findExtensionInfo(std::wstring_view fileName) noexcept {
  const std::wstring_view extension = fileExtension(fileName);
  if (extension.empty())
    return nullptr;

  for (const auto &info : g_extensionTable) {
    const std::wstring_view supportedExtension(info.dotExt);
    if (extension.size() == supportedExtension.size() &&
        std::equal(extension.cbegin(), extension.cend(),
                   supportedExtension.cbegin(),
                   [](wchar_t left, wchar_t right) {
                     return asciiLower(left) == asciiLower(right);
                   }))
      return &info;
  }
  return nullptr;
}

struct QtLibraryRequirements {
  bool needsSvg;
  bool needsPdf;
  bool needsJpeg;
  bool needsZlib;
};

static QtLibraryRequirements
qtLibraryRequirements(const ExtensionInfo &extensionInfo) noexcept {
  const wchar_t *extension = extensionInfo.dotExt;
  const bool isTiff = _wcsicmp(extension, L".tif") == 0 ||
                      _wcsicmp(extension, L".tiff") == 0;
  const bool isLayeredArchive = _wcsicmp(extension, L".kra") == 0 ||
                                _wcsicmp(extension, L".ora") == 0;
  const bool isCbz = _wcsicmp(extension, L".cbz") == 0;
  return {
      .needsSvg = _wcsicmp(extension, L".svg") == 0,
      .needsPdf = _wcsicmp(extension, L".ai") == 0,
      // CBZ may contain TIFF pages, whose plugin uses the root-level JPEG DLL.
      .needsJpeg = extensionInfo.isRaw || isTiff || isCbz,
      // PNG/TIFF pages inside CBZ may load plugins that depend on zlib1.dll.
      .needsZlib = isTiff || isLayeredArchive || isCbz,
  };
}

constexpr QtLibraryRequirements UNNAMED_SVG_LIBRARY_REQUIREMENTS{
    .needsSvg = true,
    .needsPdf = false,
    .needsJpeg = false,
    .needsZlib = false,
};

using SvgReferenceGraph = QHash<QString, QSet<QString>>;

static bool isSvgExtension(const QString &ext) { return ext == u"svg"; }
static bool isDdsExtension(const QString &ext) { return ext == u"dds"; }
static bool isDjvuExtension(const QString &ext) {
  return ext == u"djvu" || ext == u"djv";
}

struct ThumbnailReaderOptions {
  QByteArray format;
  QString extension;
  UINT requestedEdge;
};

static bool configureThumbnailReader(
    QImageReader &reader, const ThumbnailReaderOptions &options) {
  if (options.format.isEmpty())
    return false;

  reader.setFormat(options.format);
  reader.setAutoDetectImageFormat(false);
  reader.setAutoTransform(true);

  // DDS exposes mip levels as images and cannot scale while decoding. Select a
  // bounded mip level without enumerating frames for animated/multipage files.
  if (isDdsExtension(options.extension)) {
    const int imageCount = reader.imageCount();
    if (imageCount <= 0 || imageCount > MAX_DDS_MIP_LEVEL_COUNT)
      return false;

    int bestIndex = 0;
    int bestDifference = std::numeric_limits<int>::max();
    bool foundValidMip = false;
    for (int imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
      if (!reader.jumpToImage(imageIndex))
        return false;

      const QSize mipSize = reader.size();
      if (!mipSize.isValid())
        continue;

      const int mipEdge = std::max(mipSize.width(), mipSize.height());
      const int difference =
          std::abs(mipEdge - static_cast<int>(options.requestedEdge));
      if (difference < bestDifference) {
        bestDifference = difference;
        bestIndex = imageIndex;
        foundValidMip = true;
      }
    }

    if (!foundValidMip || !reader.jumpToImage(bestIndex))
      return false;
  }

  QSize size = reader.size();
  if (size.isValid()) {
    if (size.width() > MAX_THUMBNAIL_DIMENSION ||
        size.height() > MAX_THUMBNAIL_DIMENSION)
      return false;
    size.scale(options.requestedEdge, options.requestedEdge,
               Qt::KeepAspectRatio);
    reader.setScaledSize(size);
  }

  return true;
}

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

static HRESULT readBoundedStream(IStream *stream, qint64 maximumBytes,
                                 QByteArray &data) {
  if (!stream)
    return E_POINTER;
  if (maximumBytes < 0)
    return E_INVALIDARG;

  data.clear();

  LARGE_INTEGER zero = {};
  ULARGE_INTEGER savedPosition = {};
  HRESULT result = stream->Seek(zero, STREAM_SEEK_CUR, &savedPosition);
  if (FAILED(result) ||
      savedPosition.QuadPart >
          static_cast<ULONGLONG>(std::numeric_limits<LONGLONG>::max()))
    return FAILED(result) ? result : STG_E_SEEKERROR;

  result = stream->Seek(zero, STREAM_SEEK_SET, nullptr);
  if (SUCCEEDED(result)) {
    char chunk[STREAM_COPY_CHUNK_SIZE];
    while (true) {
      ULONG bytesRead = 0;
      result = stream->Read(chunk, STREAM_COPY_CHUNK_SIZE, &bytesRead);
      if (FAILED(result))
        break;
      if (bytesRead > STREAM_COPY_CHUNK_SIZE) {
        result = STG_E_READFAULT;
        break;
      }
      if (data.size() >
          maximumBytes - static_cast<qint64>(bytesRead)) {
        result = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        break;
      }

      data.append(chunk, static_cast<qsizetype>(bytesRead));
      if (result == S_FALSE || bytesRead == 0) {
        result = S_OK;
        break;
      }
    }
  }

  LARGE_INTEGER restorePosition = {};
  restorePosition.QuadPart = static_cast<LONGLONG>(savedPosition.QuadPart);
  const HRESULT restoreResult =
      stream->Seek(restorePosition, STREAM_SEEK_SET, nullptr);
  if (FAILED(result))
    return result;
  return restoreResult;
}

static bool readSvgStream(IStream *stream, QByteArray &svg) {
  return SUCCEEDED(readBoundedStream(stream, MAX_SVG_SOURCE_BYTES, svg));
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
    previewReader.setAutoDetectImageFormat(false);
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

static bool extractLibRawEmbeddedPreview(LibRaw &raw, UINT requestedEdge,
                                         QImage &outImg) {
  outImg = QImage();
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

static bool tryLibRawEmbeddedPreview(const std::wstring &filePath,
                                     UINT requestedEdge,
                                     QImage &outImg) {
  if (filePath.empty())
    return false;

  LibRaw raw;
  if (raw.open_file(filePath.c_str()) != LIBRAW_SUCCESS)
    return false;
  return extractLibRawEmbeddedPreview(raw, requestedEdge, outImg);
}

static bool tryLibRawEmbeddedPreview(const QByteArray &rawBuffer,
                                     UINT requestedEdge,
                                     QImage &outImg) {
  if (rawBuffer.isEmpty() ||
      rawBuffer.size() > MAX_THUMBNAIL_FILE_SIZE)
    return false;

  LibRaw raw;
  if (raw.open_buffer(rawBuffer.constData(),
                      static_cast<size_t>(rawBuffer.size())) !=
      LIBRAW_SUCCESS)
    return false;
  return extractLibRawEmbeddedPreview(raw, requestedEdge, outImg);
}



static QImage renderDjvuThumbnail(const QByteArray &data, UINT requestedEdge) {
  if (data.isEmpty() || requestedEdge == 0)
    return QImage();

  static std::once_flag initFlag;
  std::call_once(initFlag, [] { djvu_init(); });

  djvu_ctx *ctx = djvu_ctx_new(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  if (!ctx)
    return QImage();

  djvu_doc *doc = djvu_doc_open(
      ctx, reinterpret_cast<const uint8_t *>(data.constData()),
      static_cast<size_t>(data.size()));
  if (!doc) {
    djvu_ctx_free(ctx);
    return QImage();
  }

  QImage result;
  if (djvu_doc_page_count(doc) > 0) {
    djvu_render_info nativeInfo{};
    if (djvu_page_render_info(doc, 0, 1, &nativeInfo) == 0 &&
        nativeInfo.width > 0 && nativeInfo.height > 0) {
      const int longestEdge = std::max(nativeInfo.width, nativeInfo.height);
      const int decodeEdge = static_cast<int>(requestedEdge) * 2;
      const int subsample =
          std::max(1, (longestEdge + decodeEdge - 1) / decodeEdge);

      djvu_render_info renderInfo{};
      if (djvu_page_render_info(doc, 0, subsample, &renderInfo) == 0 &&
          renderInfo.width > 0 && renderInfo.height > 0) {
        QImage::Format format = QImage::Format_Invalid;
        if (renderInfo.format == DJVU_FORMAT_RGB24)
          format = QImage::Format_RGB888;
        else if (renderInfo.format == DJVU_FORMAT_GRAY8)
          format = QImage::Format_Grayscale8;

        if (format != QImage::Format_Invalid) {
          QImage decoded(renderInfo.width, renderInfo.height, format);
          if (!decoded.isNull() &&
              djvu_page_render_into(doc, 0, subsample, decoded.bits(),
                                    decoded.bytesPerLine()) == 0) {
            result = std::move(decoded);
          }
        }
      }
    }
  }

  djvu_doc_close(doc);
  djvu_ctx_free(ctx);

  if (!result.isNull() &&
      (result.width() > static_cast<int>(requestedEdge) ||
       result.height() > static_cast<int>(requestedEdge))) {
    result = result.scaled(static_cast<int>(requestedEdge),
                           static_cast<int>(requestedEdge),
                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }
  return result;
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

static long dllReferenceCount() noexcept {
  return InterlockedCompareExchange(&g_cDllRef, 0, 0);
}




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
}

QImgvThumbnailProvider::~QImgvThumbnailProvider() {
  IStream *stream = std::exchange(m_pStream, nullptr);
  if (stream)
    stream->Release();
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
    InterlockedDecrement(&g_cDllRef);
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

// Keeps Qt and the root-level dependencies used by image plugins loaded for
// the complete thumbnail request without changing DllHost's global DLL search
// path.
class ScopedQtLibraries {
public:
  ScopedQtLibraries(const wchar_t *dllDir,
                    const QtLibraryRequirements &requirements)
      : m_error(ERROR_SUCCESS),
        m_hCore(loadFromDirectory(dllDir, L"Qt6Core.dll", m_error)),
        m_hGui(loadFromDirectory(dllDir, L"Qt6Gui.dll", m_error)),
        m_hSvg(requirements.needsSvg
                   ? loadFromDirectory(dllDir, L"Qt6Svg.dll", m_error)
                   : nullptr),
        m_hPdf(requirements.needsPdf
                   ? loadFromDirectory(dllDir, L"Qt6Pdf.dll", m_error)
                   : nullptr),
        m_hJpeg(requirements.needsJpeg
                    ? loadFromDirectory(dllDir, L"jpeg62.dll", m_error)
                    : nullptr),
        m_hZlib(requirements.needsZlib
                    ? loadFromDirectory(dllDir, L"zlib1.dll", m_error)
                    : nullptr),
        m_ready(m_hCore && m_hGui &&
                 (!requirements.needsSvg || m_hSvg) &&
                 (!requirements.needsPdf || m_hPdf) &&
                 (!requirements.needsJpeg || m_hJpeg) &&
                 (!requirements.needsZlib || m_hZlib)) {}

  ScopedQtLibraries(const ScopedQtLibraries &) = delete;
  ScopedQtLibraries &operator=(const ScopedQtLibraries &) = delete;
  ScopedQtLibraries(ScopedQtLibraries &&) = delete;
  ScopedQtLibraries &operator=(ScopedQtLibraries &&) = delete;

  ~ScopedQtLibraries() {
    release(m_hPdf);
    release(m_hSvg);
    release(m_hGui);
    release(m_hCore);
    release(m_hJpeg);
    release(m_hZlib);
  }

  bool isReady() const noexcept { return m_ready; }
  DWORD errorCode() const noexcept {
    return m_error == ERROR_SUCCESS ? ERROR_MOD_NOT_FOUND : m_error;
  }

private:
  static void recordFirstError(DWORD &firstError, DWORD error) noexcept {
    if (firstError == ERROR_SUCCESS) {
      firstError =
          error == ERROR_SUCCESS ? ERROR_MOD_NOT_FOUND : error;
    }
  }

  static HMODULE loadFromDirectory(const wchar_t *dllDir,
                                   const wchar_t *fileName,
                                   DWORD &firstError) noexcept {
    if (!dllDir || *dllDir == L'\0' || !fileName || *fileName == L'\0') {
      recordFirstError(firstError, ERROR_INVALID_PARAMETER);
      return nullptr;
    }

    wchar_t path[MAX_PATH] = L"";
    if (!PathCombineW(path, dllDir, fileName)) {
      recordFirstError(firstError, ERROR_INSUFFICIENT_BUFFER);
      return nullptr;
    }

    constexpr DWORD safeSearchFlags =
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    HMODULE module = LoadLibraryExW(path, nullptr, safeSearchFlags);
    if (!module)
      recordFirstError(firstError, GetLastError());
    return module;
  }

  static void release(HMODULE module) noexcept {
    if (module)
      FreeLibrary(module);
  }

  DWORD m_error;
  HMODULE m_hCore;
  HMODULE m_hGui;
  HMODULE m_hSvg;
  HMODULE m_hPdf;
  HMODULE m_hJpeg;
  HMODULE m_hZlib;
  bool m_ready;
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
    const ExtensionInfo *extensionInfo = nullptr;
    bool probeUnnamedStreamAsSvg = false;
    if (m_pStream) {
      STATSTG sizeInfo{};
      const HRESULT sizeResult =
          m_pStream->Stat(&sizeInfo, STATFLAG_NONAME);
      if (SUCCEEDED(sizeResult) &&
          sizeInfo.cbSize.QuadPart >
              static_cast<ULONGLONG>(MAX_THUMBNAIL_FILE_SIZE))
        return E_FAIL;

      // A stream name is optional and is not guaranteed to be a file path.
      // Use it when available, but retain only the bounded SVG path for a
      // genuinely unnamed stream.
      STATSTG nameInfo{};
      const HRESULT nameResult =
          m_pStream->Stat(&nameInfo, STATFLAG_DEFAULT);
      if (SUCCEEDED(nameResult)) {
        if (FAILED(sizeResult) &&
            nameInfo.cbSize.QuadPart >
                static_cast<ULONGLONG>(MAX_THUMBNAIL_FILE_SIZE)) {
          if (nameInfo.pwcsName)
            CoTaskMemFree(nameInfo.pwcsName);
          return E_FAIL;
        }

        if (nameInfo.pwcsName && *nameInfo.pwcsName != L'\0') {
          const std::wstring_view streamName(nameInfo.pwcsName);
          extensionInfo = findExtensionInfo(streamName);
          CoTaskMemFree(nameInfo.pwcsName);
        } else {
          if (nameInfo.pwcsName)
            CoTaskMemFree(nameInfo.pwcsName);
          probeUnnamedStreamAsSvg = true;
        }
      } else {
        probeUnnamedStreamAsSvg = true;
      }
    } else {
      extensionInfo = findExtensionInfo(m_szFilePath);
    }

    // Explicit unsupported extensions must never reach a decoder. An unnamed
    // stream may only enter the capped and validated SVG-specific path below.
    if (!extensionInfo && !probeUnnamedStreamAsSvg)
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    // Retrieve absolute DLL directory path and set up DLL search environment
    HMODULE hModule = nullptr;
    wchar_t dllDir[MAX_PATH] = L"";
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&DllGetClassObject),
                            &hModule))
      return HRESULT_FROM_WIN32(GetLastError());

    const DWORD modulePathLength = GetModuleFileNameW(hModule, dllDir, MAX_PATH);
    if (modulePathLength == 0)
      return HRESULT_FROM_WIN32(GetLastError());
    if (modulePathLength >= MAX_PATH || !PathRemoveFileSpecW(dllDir))
      return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

    const QtLibraryRequirements libraryRequirements =
        extensionInfo ? qtLibraryRequirements(*extensionInfo)
                      : UNNAMED_SVG_LIBRARY_REQUIREMENTS;
    ScopedQtLibraries qtLibraries(dllDir, libraryRequirements);
    if (!qtLibraries.isReady())
      return HRESULT_FROM_WIN32(qtLibraries.errorCode());

    // QImageReader only needs Qt's plugin search path. Creating a
    // process-global QCoreApplication in an unloadable COM DLL leaves a
    // dangling qApp when the provider module is recycled by Explorer.
    if (dllDir[0] != L'\0')
      QCoreApplication::addLibraryPath(QString::fromWCharArray(dllDir));

    const QString ext = extensionInfo
                            ? QString::fromWCharArray(extensionInfo->dotExt + 1)
                            : QString();
    if (!m_szFilePath.empty()) {
      QFileInfo fileInfo(QString::fromStdWString(m_szFilePath));
      if (fileInfo.size() > MAX_THUMBNAIL_FILE_SIZE)
        return E_FAIL;
    }

    const bool knownSvg = extensionInfo && isSvgExtension(ext);
    if (knownSvg || probeUnnamedStreamAsSvg) {
      QByteArray svg;
      const bool readSucceeded =
          m_pStream ? readSvgStream(m_pStream, svg)
                    : readSvgFile(QString::fromStdWString(m_szFilePath), svg);
      HBITMAP bitmap = readSucceeded ? renderSvgThumbnail(svg, cx) : nullptr;
      if (bitmap) {
        *phbmp = bitmap;
        *pdwAlpha = WTSAT_ARGB;
        return S_OK;
      }
      return knownSvg ? E_FAIL
                      : HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    if (!extensionInfo)
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    if (extensionInfo->isRaw) {
      QImage rawImg;
      bool previewExtracted = false;
      if (m_pStream) {
        QByteArray rawBuffer;
        const HRESULT readResult = readBoundedStream(
            m_pStream, MAX_THUMBNAIL_FILE_SIZE, rawBuffer);
        if (FAILED(readResult))
          return readResult;
        previewExtracted =
            tryLibRawEmbeddedPreview(rawBuffer, cx, rawImg);
      } else {
        previewExtracted =
            tryLibRawEmbeddedPreview(m_szFilePath, cx, rawImg);
      }

      if (!previewExtracted || rawImg.isNull())
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

    if (isDjvuExtension(ext)) {
      QByteArray djvuData;
      if (m_pStream) {
        const HRESULT readResult =
            readBoundedStream(m_pStream, MAX_THUMBNAIL_FILE_SIZE, djvuData);
        if (FAILED(readResult))
          return readResult;
      } else {
        QFile djvuFile(QString::fromStdWString(m_szFilePath));
        if (!djvuFile.open(QIODevice::ReadOnly))
          return E_FAIL;
        djvuData = djvuFile.readAll();
        if (djvuData.isEmpty() ||
            djvuData.size() > MAX_THUMBNAIL_FILE_SIZE)
          return E_FAIL;
      }

      QImage djvuImage = renderDjvuThumbnail(djvuData, cx);
      if (djvuImage.isNull())
        return E_FAIL;

      if (djvuImage.format() != QImage::Format_ARGB32_Premultiplied)
        djvuImage =
            djvuImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);

      BITMAPV5HEADER bi;
      ZeroMemory(&bi, sizeof(bi));
      bi.bV5Size = sizeof(bi);
      bi.bV5Width = djvuImage.width();
      bi.bV5Height = -djvuImage.height();
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
        if (hbmp)
          DeleteObject(hbmp);
        return E_FAIL;
      }

      memcpy(pBits, djvuImage.constBits(), djvuImage.sizeInBytes());
      *phbmp = hbmp;
      *pdwAlpha = WTSAT_RGB;
      return S_OK;
    }

  // Fallback / standard path: set up QImageReader
  std::unique_ptr<QStreamDevice> streamDevice;
  QImageReader reader;
  QByteArray format;

  format = extensionInfo->qtFormat ? QByteArray(extensionInfo->qtFormat)
                                   : ext.toUtf8();
  const ThumbnailReaderOptions readerOptions{format, ext, cx};

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

  if (!configureThumbnailReader(reader, readerOptions))
    return E_FAIL;

  QImage img;
  if (!reader.read(&img)) {
    if (m_pStream) {
      // Fallback for stream: try memory buffer if small, otherwise temporary file
      if (seekable && streamSize > 0 &&
          streamSize <= MAX_IN_MEMORY_FALLBACK_BYTES) {
        QByteArray buffer;
        buffer.resize(streamSize);
        LARGE_INTEGER zero = {0};
        if (FAILED(m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr)))
          return E_FAIL;
        ULONG bytesRead = 0;
        HRESULT hr =
            m_pStream->Read(buffer.data(), (ULONG)streamSize, &bytesRead);
        if (SUCCEEDED(hr) && bytesRead == streamSize) {
          QBuffer memBuffer(&buffer);
          memBuffer.open(QIODevice::ReadOnly);
          QImageReader memReader(&memBuffer);
          if (!configureThumbnailReader(memReader, readerOptions))
            return E_FAIL;
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
          if (FAILED(m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr)))
            return E_FAIL;
          char chunk[STREAM_COPY_CHUNK_SIZE];
          qint64 totalCopied = 0;
          bool writeFailed = false;

          while (true) {
            ULONG bytesRead = 0;
            const HRESULT readResult =
                m_pStream->Read(chunk, STREAM_COPY_CHUNK_SIZE, &bytesRead);
            if (FAILED(readResult) ||
                bytesRead > STREAM_COPY_CHUNK_SIZE) {
              writeFailed = true;
              break;
            }
            if (bytesRead == 0)
              break;
            if (totalCopied >
                MAX_THUMBNAIL_FILE_SIZE - static_cast<qint64>(bytesRead)) {
              writeFailed = true;
              break;
            }
            totalCopied += static_cast<qint64>(bytesRead);
            if (tempFile.write(chunk, static_cast<qint64>(bytesRead)) !=
                static_cast<qint64>(bytesRead)) {
              writeFailed = true;
              break;
            }
            if (readResult == S_FALSE)
              break;
          }

          if (writeFailed) {
            tempFile.close();
            tempFile.remove();
            return E_FAIL;
          }

          if (!tempFile.flush())
            return E_FAIL;

          QImageReader fileReader(tempFile.fileName());
          if (!configureThumbnailReader(fileReader, readerOptions))
            return E_FAIL;
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

QImgvThumbnailProviderClassFactory::~QImgvThumbnailProviderClassFactory() =
    default;

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
    InterlockedDecrement(&g_cDllRef);
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
      []() -> HRESULT { return dllReferenceCount() == 0 ? S_OK : S_FALSE; });
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
      hr = CreateRegistryKeyAndValue(HKEY_CURRENT_USER, extKey, nullptr,
                                     CLSID_QImgvThumbnailProvider_Str);
      if (FAILED(hr))
        return hr;
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
          hr = CreateRegistryKeyAndValue(HKEY_CURRENT_USER, progIdKey, nullptr,
                                         CLSID_QImgvThumbnailProvider_Str);
          if (FAILED(hr))
            return hr;
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
