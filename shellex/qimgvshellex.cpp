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
#include <QIODevice>
#include <QImage>
#include <QImageReader>
#include <QTemporaryFile>
#include <libraw/libraw.h>

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

static bool isRawExtension(const QString &ext) {
  return ext == QLatin1String("arw") || ext == QLatin1String("cr2") ||
         ext == QLatin1String("cr3") || ext == QLatin1String("nef") ||
         ext == QLatin1String("dng") || ext == QLatin1String("rw2") ||
         ext == QLatin1String("pef") || ext == QLatin1String("raf") ||
         ext == QLatin1String("orf") || ext == QLatin1String("raw");
}

static QImage
createQImageFromLibRawImage(const libraw_processed_image_t *processedImage) {
  if (!processedImage || !processedImage->data ||
      processedImage->data_size == 0)
    return QImage();

  if (processedImage->type == LIBRAW_IMAGE_JPEG) {
    return QImage::fromData(
        reinterpret_cast<const uchar *>(processedImage->data),
        processedImage->data_size, "JPEG");
  }

  if (processedImage->type != LIBRAW_IMAGE_BITMAP || processedImage->bits != 8)
    return QImage();

  QImage::Format format = QImage::Format_Invalid;
  if (processedImage->colors == 3) {
    format = QImage::Format_RGB888;
  } else if (processedImage->colors == 1) {
    format = QImage::Format_Grayscale8;
  } else {
    return QImage();
  }

  const int bytesPerLine = processedImage->width * processedImage->colors;
  QImage img(reinterpret_cast<const uchar *>(processedImage->data),
             processedImage->width, processedImage->height, bytesPerLine,
             format);
  return img.copy();
}

static bool tryLibRawThumbnail(const QByteArray &rawBuffer, UINT cx,
                               QImage &outImg) {
  if (rawBuffer.isEmpty())
    return false;

  LibRaw raw;
  if (raw.open_buffer(rawBuffer.constData(), rawBuffer.size()) !=
      LIBRAW_SUCCESS)
    return false;

  // 1) Try embedded JPEG preview first
  if (raw.unpack_thumb() == LIBRAW_SUCCESS) {
    std::unique_ptr<libraw_processed_image_t,
                    decltype(&LibRaw::dcraw_clear_mem)>
        thumb(raw.dcraw_make_mem_thumb(), LibRaw::dcraw_clear_mem);
    if (thumb) {
      QImage img = createQImageFromLibRawImage(thumb.get());
      if (!img.isNull() && img.width() >= (int)cx) {
        outImg = std::move(img);
        raw.recycle();
        return true;
      }
    }
  }

  // 2) Half-size raw render for better quality than most embedded previews
  raw.imgdata.params.half_size = 1;
  raw.imgdata.params.use_camera_wb = 1;
  raw.imgdata.params.output_color = 1;

  if (raw.unpack() == LIBRAW_SUCCESS && raw.dcraw_process() == LIBRAW_SUCCESS) {
    std::unique_ptr<libraw_processed_image_t,
                    decltype(&LibRaw::dcraw_clear_mem)>
        processed(raw.dcraw_make_mem_image(), LibRaw::dcraw_clear_mem);
    if (processed) {
      QImage img = createQImageFromLibRawImage(processed.get());
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

static bool tryLibRawThumbnailFromStream(IStream *stream, UINT cx,
                                         QImage &outImg) {
  if (!stream)
    return false;

  QStreamDevice device(stream);
  if (!device.seek(0))
    return false;

  QByteArray rawBuffer = device.readAll();
  if (rawBuffer.isEmpty())
    return false;

  return tryLibRawThumbnail(rawBuffer, cx, outImg);
}

static bool tryLibRawThumbnailFromFile(const QString &filePath, UINT cx,
                                       QImage &outImg) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
    return false;

  QByteArray rawBuffer = file.readAll();
  if (rawBuffer.isEmpty())
    return false;

  return tryLibRawThumbnail(rawBuffer, cx, outImg);
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

// Supported extensions that we want to register directly
const wchar_t *g_extensions[] = {
    L".arw", L".cr2", L".cr3", L".nef", L".dng", L".rw2", L".pef", L".raf",
    L".orf", L".raw",                                // RAW formats
    L".kra", L".ora",                                // Krita / OpenRaster
    L".webp", L".jxl", L".avif", L".heic", L".heif", // Modern web/mobile
                                                     // formats
    L".exr", L".hdr",                                // High dynamic range
    L".tga", L".jxr", L".hdp", L".wdp",              // Other image formats
    L".qoi", L".dds",                                // QOI and DDS
    L".psd", L".psb",                                // Photoshop
    L".ai", L".pdf",                                 // Adobe Illustrator / PDF
    L".tif", L".tiff",                               // TIFF
    L".svg", L".svgz",                               // Scalable Vector Graphics
    L".jp2", L".j2k", L".jpf", L".jpx", L".jpc",     // JPEG 2000
    L".jph"                                          // HTJ2K / JPH
};

// ProgIDs to hook into
const wchar_t *g_progIds[] = {
    L"qimgvplus.AssocFile.raw",  L"qimgvplus.AssocFile.kra",
    L"qimgvplus.AssocFile.webp", L"qimgvplus.AssocFile.jxl",
    L"qimgvplus.AssocFile.avif", L"qimgvplus.AssocFile.heic",
    L"qimgvplus.AssocFile.heif", L"qimgvplus.AssocFile.exr",
    L"qimgvplus.AssocFile.hdr",  L"qimgvplus.AssocFile.tga",
    L"qimgvplus.AssocFile.ora",  L"qimgvplus.AssocFile.jxr",
    L"qimgvplus.AssocFile.psd",  L"qimgvplus.AssocFile.psb",
    L"qimgvplus.AssocFile.ai",   L"qimgvplus.AssocFile.pdf",
    L"qimgvplus.AssocFile.tif",  L"qimgvplus.AssocFile.tiff",
    L"qimgvplus.AssocFile.qoi",  L"qimgvplus.AssocFile.dds",
    L"qimgvplus.AssocFile.jp2",  L"qimgvplus.AssocFile.jph"};


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

HRESULT DeleteRegistryKey(HKEY hKeyParent, LPCWSTR pszSubKey) {
  LSTATUS status = RegDeleteTreeW(hKeyParent, pszSubKey);
  return HRESULT_FROM_WIN32(status);
}

// QImgvThumbnailProvider Implementation
QImgvThumbnailProvider::QImgvThumbnailProvider()
    : m_cRef(1), m_pStream(nullptr) {
  InterlockedIncrement(&g_cDllRef);
  m_szFilePath[0] = L'\0';
}

QImgvThumbnailProvider::~QImgvThumbnailProvider() {
  InterlockedDecrement(&g_cDllRef);
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
}

// IUnknown Methods
IFACEMETHODIMP QImgvThumbnailProvider::QueryInterface(REFIID riid, void **ppv) {
  static const QITAB qit[] = {
      QITABENT(QImgvThumbnailProvider, IThumbnailProvider),
      QITABENT(QImgvThumbnailProvider, IInitializeWithFile),
      QITABENT(QImgvThumbnailProvider, IInitializeWithStream),
      {0},
  };
  return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) QImgvThumbnailProvider::AddRef() {
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) QImgvThumbnailProvider::Release() {
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

// IInitializeWithFile Methods
IFACEMETHODIMP QImgvThumbnailProvider::Initialize(LPCWSTR pszFilePath,
                                                  DWORD grfMode) {
  if (!pszFilePath)
    return E_INVALIDARG;
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
  wcscpy_s(m_szFilePath, MAX_PATH, pszFilePath);
  return S_OK;
}

// IInitializeWithStream Methods
IFACEMETHODIMP QImgvThumbnailProvider::Initialize(IStream *pStream,
                                                  DWORD grfMode) {
  if (!pStream)
    return E_INVALIDARG;
  if (m_pStream) {
    m_pStream->Release();
    m_pStream = nullptr;
  }
  m_pStream = pStream;
  m_pStream->AddRef();
  m_szFilePath[0] = L'\0';
  return S_OK;
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
      swprintf_s(corePath, MAX_PATH, L"%s\\Qt%dCore.dll", dllDir,
                 QT_VERSION_MAJOR);
      swprintf_s(guiPath, MAX_PATH, L"%s\\Qt%dGui.dll", dllDir,
                 QT_VERSION_MAJOR);

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
                                                    WTS_ALPHATYPE *pdwAlpha) {
  if (!phbmp || !pdwAlpha)
    return E_INVALIDARG;
  *phbmp = nullptr;
  *pdwAlpha = WTSAT_UNKNOWN;

  if (m_szFilePath[0] == L'\0' && !m_pStream) {
    return E_FAIL;
  }

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

  // Initialize headless QCoreApplication inside DLL if not already running
  if (!QCoreApplication::instance()) {
    int argc = 1;
    static char *argv[] = {(char *)"qimgvshellex.dll"};
    new QCoreApplication(argc, argv);
  }

  // Initialize library search paths for Qt plugins (imageformats) relative to
  // the DLL
  static bool libPathInitialized = false;
  if (!libPathInitialized) {
    if (dllDir[0] != L'\0') {
      QString qPath = QString::fromWCharArray(dllDir);
      QCoreApplication::addLibraryPath(qPath);
    }
    libPathInitialized = true;
  }

  std::unique_ptr<QStreamDevice> streamDevice;
  QImageReader reader;
  QByteArray format;

  // Determine stream size and seek capability
  qint64 streamSize = 0;
  bool seekable = false;
  LARGE_INTEGER savedPos = {0};
  if (m_pStream) {
    STATSTG statstg;
    if (SUCCEEDED(m_pStream->Stat(&statstg, STATFLAG_DEFAULT))) {
      if (statstg.pwcsName) {
        QString path = QString::fromWCharArray(statstg.pwcsName);
        QFileInfo fi(path);
        QString ext = fi.suffix().toLower();

        // Map extension to Qt image format name
        if (ext == "arw" || ext == "cr2" || ext == "cr3" || ext == "nef" ||
            ext == "dng" || ext == "rw2" || ext == "pef" || ext == "raf" ||
            ext == "orf" || ext == "raw") {
          format = "raw";
        } else if (ext == "kra") {
          format = "kra";
        } else if (ext == "ora") {
          format = "ora";
        } else if (ext == "psd" || ext == "psb") {
          format = "psd";
        } else if (ext == "svg" || ext == "svgz") {
          format = "svg";
        } else if (ext == "heic" || ext == "heif") {
          format = "heif";
        } else if (ext == "ai" || ext == "pdf") {
          format = "pdf";
        } else if (ext == "tif" || ext == "tiff") {
          format = "tiff";
        } else {
          format = ext.toUtf8();
        }

        CoTaskMemFree(statstg.pwcsName);
      }
    }

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
    QString filePath = QString::fromWCharArray(m_szFilePath);
    reader.setFileName(filePath);
  }

  if (!format.isEmpty()) {
    reader.setFormat(format);
  }

  QString rawExt;
  if (m_pStream) {
    STATSTG statstg;
    if (SUCCEEDED(m_pStream->Stat(&statstg, STATFLAG_DEFAULT)) &&
        statstg.pwcsName) {
      rawExt = QFileInfo(QString::fromWCharArray(statstg.pwcsName))
                   .suffix()
                   .toLower();
      CoTaskMemFree(statstg.pwcsName);
    }
  } else {
    rawExt =
        QFileInfo(QString::fromWCharArray(m_szFilePath)).suffix().toLower();
  }

  if (isRawExtension(rawExt)) {
    QImage rawImg;
    bool rawOk = false;
    if (m_pStream) {
      rawOk = tryLibRawThumbnailFromStream(m_pStream, cx, rawImg);
    } else {
      rawOk = tryLibRawThumbnailFromFile(QString::fromWCharArray(m_szFilePath),
                                         cx, rawImg);
    }

    if (rawOk && !rawImg.isNull()) {
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
    }
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
    size.scale(cx, cx, Qt::KeepAspectRatio);
    reader.setScaledSize(size);
  }

  QImage img;
  if (!reader.read(&img)) {
    if (m_pStream) {
      // Fallback for stream: try memory buffer if small, otherwise temporary
      // file
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
            sz.scale(cx, cx, Qt::KeepAspectRatio);
            memReader.setScaledSize(sz);
          }
          if (memReader.read(&img)) {
            // success
          }
        }
      } else {
        QTemporaryFile tempFile;
        tempFile.setFileTemplate(QDir::tempPath() + "/qimgv_XXXXXX");
        if (tempFile.open()) {
          // Rewind stream and copy data
          LARGE_INTEGER zero = {0};
          m_pStream->Seek(zero, STREAM_SEEK_SET, nullptr);
          char chunk[65536];
          ULONG read;
          while (SUCCEEDED(m_pStream->Read(chunk, sizeof(chunk), &read)) &&
                 read > 0) {
            tempFile.write(chunk, read);
          }
          tempFile.flush();

          QImageReader fileReader(tempFile.fileName());
          if (!format.isEmpty())
            fileReader.setFormat(format);
          fileReader.setAutoTransform(true);
          QSize sz = fileReader.size();
          if (sz.isValid()) {
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
                                                                  void **ppv) {
  static const QITAB qit[] = {
      QITABENT(QImgvThumbnailProviderClassFactory, IClassFactory),
      {0},
  };
  return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) QImgvThumbnailProviderClassFactory::AddRef() {
  return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) QImgvThumbnailProviderClassFactory::Release() {
  ULONG cRef = InterlockedDecrement(&m_cRef);
  if (cRef == 0) {
    delete this;
  }
  return cRef;
}

// IClassFactory Methods
IFACEMETHODIMP
QImgvThumbnailProviderClassFactory::CreateInstance(IUnknown *pUnkOuter,
                                                   REFIID riid, void **ppv) {
  if (pUnkOuter != nullptr)
    return CLASS_E_NOAGGREGATION;

  QImgvThumbnailProvider *pProvider =
      new (std::nothrow) QImgvThumbnailProvider();
  if (!pProvider)
    return E_OUTOFMEMORY;

  HRESULT hr = pProvider->QueryInterface(riid, ppv);
  pProvider->Release();
  return hr;
}

IFACEMETHODIMP QImgvThumbnailProviderClassFactory::LockServer(BOOL fLock) {
  if (fLock) {
    InterlockedIncrement(&g_cDllRef);
  } else {
    InterlockedDecrement(&g_cDllRef);
  }
  return S_OK;
}

// COM DLL Exported functions
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv) {
  if (ppv == nullptr)
    return E_INVALIDARG;
  *ppv = nullptr;

  if (rclsid == CLSID_QImgvThumbnailProvider) {
    QImgvThumbnailProviderClassFactory *pFactory =
        new (std::nothrow) QImgvThumbnailProviderClassFactory();
    if (!pFactory)
      return E_OUTOFMEMORY;

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
  }
  return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow() { return (g_cDllRef == 0) ? S_OK : S_FALSE; }

STDAPI DllRegisterServer() {
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

  HRESULT hr = CreateRegistryKeyAndValue(HKEY_LOCAL_MACHINE, clsidKey, nullptr,
                                         L"qimgv-plus Thumbnail Provider");
  if (FAILED(hr))
    return hr;

  wchar_t inprocKey[MAX_PATH];
  swprintf_s(inprocKey, MAX_PATH, L"%s\\InprocServer32", clsidKey);
  hr = CreateRegistryKeyAndValue(HKEY_LOCAL_MACHINE, inprocKey, nullptr,
                                 dllPath);
  if (FAILED(hr))
    return hr;

  hr = CreateRegistryKeyAndValue(HKEY_LOCAL_MACHINE, inprocKey,
                                 L"ThreadingModel", L"Apartment");
  if (FAILED(hr))
    return hr;

  // Register for direct Extensions
  for (const auto &ext : g_extensions) {
    wchar_t extKey[MAX_PATH];
    swprintf_s(extKey, MAX_PATH,
               L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
               L"234630154E96}",
               ext);
    CreateRegistryKeyAndValue(HKEY_LOCAL_MACHINE, extKey, nullptr,
                              CLSID_QImgvThumbnailProvider_Str);
  }

  // Register for ProgIDs
  for (const auto &progId : g_progIds) {
    wchar_t progIdKey[MAX_PATH];
    swprintf_s(progIdKey, MAX_PATH,
               L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
               L"234630154E96}",
               progId);
    CreateRegistryKeyAndValue(HKEY_LOCAL_MACHINE, progIdKey, nullptr,
                              CLSID_QImgvThumbnailProvider_Str);
  }

  // Notify shell about changes
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return S_OK;
}

STDAPI DllUnregisterServer() {
  wchar_t clsidKey[MAX_PATH];
  swprintf_s(clsidKey, MAX_PATH, L"Software\\Classes\\CLSID\\%s",
             CLSID_QImgvThumbnailProvider_Str);
  DeleteRegistryKey(HKEY_LOCAL_MACHINE, clsidKey);

  // Unregister for direct Extensions
  for (const auto &ext : g_extensions) {
    wchar_t extKey[MAX_PATH];
    swprintf_s(extKey, MAX_PATH,
               L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
               L"234630154E96}",
               ext);
    DeleteRegistryKey(HKEY_LOCAL_MACHINE, extKey);
  }

  // Unregister for ProgIDs
  for (const auto &progId : g_progIds) {
    wchar_t progIdKey[MAX_PATH];
    swprintf_s(progIdKey, MAX_PATH,
               L"Software\\Classes\\%s\\ShellEx\\{E357FCCD-A995-4576-B01F-"
               L"234630154E96}",
               progId);
    DeleteRegistryKey(HKEY_LOCAL_MACHINE, progIdKey);
  }

  // Notify shell about changes
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return S_OK;
}