#include "documentinfo.h"
#include "settings.h"
#include "components/comfymetadata/comfyksamplerparser.h"

#include <utility>

DocumentInfo::DocumentInfo(QString path)
    : mDocumentType(DocumentType::NONE),
      mOrientation(0),
      mFormat(""),
      exifLoaded(false),
      generationInfoLoaded(false)
{
    fileInfo.setFile(path);
    if(!fileInfo.isFile()) {
        qWarning() << "FileInfo: cannot open: " << path;
        return;
    }
    detectFormat();
}

DocumentInfo::~DocumentInfo() {
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

QString DocumentInfo::directoryPath() const {
    return fileInfo.absolutePath();
}

QString DocumentInfo::filePath() const {
    return fileInfo.absoluteFilePath();
}

QString DocumentInfo::fileName() const {
    return fileInfo.fileName();
}

QString DocumentInfo::baseName() const {
    return fileInfo.baseName();
}

// bytes
qint64 DocumentInfo::fileSize() const {
    return fileInfo.size();
}

DocumentType DocumentInfo::type() const {
    return mDocumentType;
}

QMimeType DocumentInfo::mimeType() const {
    return mMimeType;
}

QString DocumentInfo::format() const {
    return mFormat;
}

QDateTime DocumentInfo::lastModified() const {
    return fileInfo.lastModified();
}

// For cases like orientation / even mimetype change we just reload
// Image from scratch, so don`t bother handling it here
void DocumentInfo::refresh() {
    fileInfo.refresh();
}

int DocumentInfo::exifOrientation() const {
    return mOrientation;
}

// ##############################################################
// ####################### PRIVATE METHODS ######################
// ##############################################################
void DocumentInfo::detectFormat() {
    if(mDocumentType != DocumentType::NONE)
        return;
    static QMimeDatabase mimeDb;
    // Fast check by extension first to bail out on videos
    QMimeType fastMime = mimeDb.mimeTypeForFile(fileInfo.filePath(), QMimeDatabase::MatchExtension);
    if(fastMime.name().startsWith(QLatin1String("video/"))) {
        mDocumentType = DocumentType::NONE;
        return;
    }

    mMimeType = mimeDb.mimeTypeForFile(fileInfo.filePath(), QMimeDatabase::MatchContent);
    auto mimeName = mMimeType.name().toUtf8();
    auto suffix = fileInfo.suffix().toLower().toUtf8();
    if(mimeName == "image/jpeg") {
        mFormat = "jpg";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/png") {
        if(QImageReader::supportedImageFormats().contains("apng") && detectAPNG()) {
            mFormat = "apng";
            mDocumentType = DocumentType::ANIMATED;
        } else {
            mFormat = "png";
            mDocumentType = DocumentType::STATIC;
        }
    } else if(mimeName == "image/gif") {
        mFormat = "gif";
        mDocumentType = DocumentType::ANIMATED;
    } else if(mimeName == "image/webp" || (mimeName == "audio/x-riff" && suffix == "webp")) {
        mFormat = "webp";
        mDocumentType = detectAnimatedWebP() ? DocumentType::ANIMATED : DocumentType::STATIC;
    } else if(mimeName == "image/jxl") {
        mFormat = "jxl";
        mDocumentType = detectAnimatedJxl() ? DocumentType::ANIMATED : DocumentType::STATIC;
        if(mDocumentType == DocumentType::ANIMATED && !settings->jxlAnimation()) {
            mDocumentType = DocumentType::NONE;
            qInfo() << "animated jxl is off; skipping file";
        }
    } else if(mimeName == "image/avif") {
        mFormat = "avif";
        mDocumentType = detectAnimatedAvif() ? DocumentType::ANIMATED : DocumentType::STATIC;
    } else if(mimeName == "image/heif" || mimeName == "image/heic" || suffix == "heif" || suffix == "heic") {
        mFormat = "heif";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/bmp") {
        mFormat = "bmp";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/svg+xml" || suffix == "svg") {
        mFormat = "svg";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/vnd.radiance" || suffix == "hdr") {
        mFormat = "hdr";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/x-exr" || suffix == "exr") {
        mFormat = "exr";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "application/pdf" || suffix == "pdf" || suffix == "ai" || mimeName == "application/illustrator") {
        mFormat = "pdf";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/x-tga" || mimeName == "image/x-targa" || suffix == "tga") {
        mFormat = "tga";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "application/x-krita" || suffix == "kra") {
        mFormat = "kra";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/openraster" || suffix == "ora") {
        mFormat = "ora";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/vnd.ms-photo" || mimeName == "image/jxr" || suffix == "jxr" || suffix == "hdp" || suffix == "wdp") {
        mFormat = "jxr";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/x-qoi" || suffix == "qoi") {
        mFormat = "qoi";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/x-dds" || mimeName == "image/dds" || mimeName == "image/vnd-ms.dds" || suffix == "dds") {
        mFormat = "dds";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/jp2" || mimeName == "image/jpx" || mimeName == "image/jpm" || suffix == "jp2" || suffix == "j2k" || suffix == "jpf" || suffix == "jpx" || suffix == "jpc") {
        mFormat = "jp2";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName == "image/jph" || suffix == "jph") {
        mFormat = "jph";
        mDocumentType = DocumentType::STATIC;
    } else if(mimeName.startsWith("image/x-") || suffix == "arw" || suffix == "cr2" || suffix == "nef" || suffix == "dng" || suffix == "raf") {
        // RAW formats
        mFormat = "raw";
        mDocumentType = DocumentType::STATIC;
    } else if(QImageReader::supportedMimeTypes().contains(mimeName)) {
        mFormat = mMimeType.preferredSuffix();
        mDocumentType = DocumentType::STATIC;
    } else {
        // unknown file type; skip
        mDocumentType = DocumentType::NONE;
    }
    if(mDocumentType != DocumentType::NONE)
        loadExifOrientation();
}

inline
// dumb apng detector
bool DocumentInfo::detectAPNG() {
    QFile f(fileInfo.filePath());
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        const int len = 120;
        QByteArray qbuf("\0", len);
        if (in.readRawData(qbuf.data(), len) > 0) {
            return qbuf.contains("acTL");
        }
    }
    return false;
}

bool DocumentInfo::detectAnimatedWebP() {
    QFile f(fileInfo.filePath());
    bool result = false;
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        in.skipRawData(12);
        char *buf = static_cast<char*>(malloc(5));
        buf[4] = '\0';
        in.readRawData(buf, 4);
        if(strcmp(buf, "VP8X") == 0) {
            in.skipRawData(4);
            char flags;
            in.readRawData(&flags, 1);
            if(flags & (1 << 1)) {
                result = true;
            }
        }
        free(buf);
    }
    return result;
}

bool DocumentInfo::detectAnimatedJxl() {
    QImageReader r(fileInfo.filePath(), "jxl");
    return r.supportsAnimation();
}

bool DocumentInfo::detectAnimatedAvif() {
    QFile f(fileInfo.filePath());
    bool result = false;
    if(f.open(QFile::ReadOnly)) {
        QDataStream in(&f);
        in.skipRawData(4); // skip box size
        char *buf = static_cast<char*>(malloc(9));
        buf[8] = '\0';
        in.readRawData(buf, 8);
        if(strcmp(buf, "ftypavis") == 0) {
            result = true;
        }
        free(buf);
    }
    return result;
}

void DocumentInfo::loadExifTags() {
    if (exifLoaded)
        return;

    // Formats supported by the application but either completely unknown 
    // to Exiv2 or having zero metadata support. Bypassing them avoids 
    // first-chance exceptions during debugging sessions.
    static const QStringList unsupportedByExiv2 = {
        "gif", "bmp", "svg", "hdr", "exr", "pdf", 
        "tga", "kra", "ora", "jxr", "qoi", "dds",
        "ai"
    };

    // Skip Exiv2 processing for unsupported image and document types
    if (unsupportedByExiv2.contains(mFormat)) {
        exifLoaded = true;
        return;
    }

    exifLoaded = true;
    exifTags.clear();

#ifdef USE_EXIV2
    try {
        QFile exifFile(fileInfo.filePath());
        if (!exifFile.open(QFile::ReadOnly)) {
            qWarning() << "DocumentInfo: failed to open file for EXIF read:"
                       << fileInfo.filePath() << exifFile.errorString();
            return;
        }

        const qint64 fileSize = exifFile.size();
        if (fileSize <= 0) {
            qWarning() << "DocumentInfo: cannot read EXIF from an empty file:"
                       << fileInfo.filePath();
            return;
        }

        const uchar *mappedFile = exifFile.map(0, fileSize);
        if (!mappedFile) {
            qWarning() << "DocumentInfo: failed to map file for EXIF read:"
                       << fileInfo.filePath() << exifFile.errorString();
            return;
        }

        // MemIo keeps a non-owning view, so exifFile must outlive the Exiv2 image.
        Exiv2::BasicIo::UniquePtr exifIo = std::make_unique<Exiv2::MemIo>(
            reinterpret_cast<const Exiv2::byte*>(mappedFile),
            static_cast<size_t>(fileSize));
        std::unique_ptr<Exiv2::Image> image = Exiv2::ImageFactory::open(std::move(exifIo));
        if (!image) {
            qWarning() << "DocumentInfo: Exiv2 could not identify the file:"
                       << fileInfo.filePath();
            return;
        }

        image->readMetadata();
        Exiv2::ExifData &exifData = image->exifData();
        if (exifData.empty())
            return;

        Exiv2::ExifKey make("Exif.Image.Make");
        Exiv2::ExifKey model("Exif.Image.Model");
        Exiv2::ExifKey dateTime("Exif.Image.DateTime");
        Exiv2::ExifKey exposureTime("Exif.Photo.ExposureTime");
        Exiv2::ExifKey fnumber("Exif.Photo.FNumber");
        Exiv2::ExifKey isoSpeedRatings("Exif.Photo.ISOSpeedRatings");
        Exiv2::ExifKey flash("Exif.Photo.Flash");
        Exiv2::ExifKey focalLength("Exif.Photo.FocalLength");
        Exiv2::ExifKey userComment("Exif.Photo.UserComment");

        Exiv2::ExifData::const_iterator it;

        it = exifData.findKey(make);
        if (it != exifData.end())
            exifTags.insert(QObject::tr("Make"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(model);
        if (it != exifData.end())
            exifTags.insert(QObject::tr("Model"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(dateTime);
        if (it != exifData.end())
            exifTags.insert(QObject::tr("Date/Time"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(exposureTime);
        if (it != exifData.end()) {
            Exiv2::Rational r = it->toRational();
            if (r.first < r.second) {
                qreal exp = round(static_cast<qreal>(r.second) / r.first);
                exifTags.insert(QObject::tr("ExposureTime"), "1/" + QString::number(exp) + QObject::tr(" sec"));
            } else {
                qreal exp = round(static_cast<qreal>(r.first) / r.second);
                exifTags.insert(QObject::tr("ExposureTime"), QString::number(exp) + QObject::tr(" sec"));
            }
        }

        it = exifData.findKey(fnumber);
        if (it != exifData.end()) {
            Exiv2::Rational r = it->toRational();
            qreal fn = static_cast<qreal>(r.first) / r.second;
            exifTags.insert(QObject::tr("F Number"), "f/" + QString::number(fn, 'g', 3));
        }

        it = exifData.findKey(isoSpeedRatings);
        if (it != exifData.end())
            exifTags.insert(QObject::tr("ISO Speed ratings"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(flash);
        if (it != exifData.end())
            exifTags.insert(QObject::tr("Flash"), QString::fromStdString(it->value().toString()));

        it = exifData.findKey(focalLength);
        if (it != exifData.end()) {
            Exiv2::Rational r = it->toRational();
            qreal fn = static_cast<qreal>(r.first) / r.second;
            exifTags.insert(QObject::tr("Focal Length"), QString::number(fn, 'g', 3) + QObject::tr(" mm"));
        }

        it = exifData.findKey(userComment);
        if (it != exifData.end()) {
            auto comment = QString::fromStdString(it->value().toString());
            if (comment.startsWith(QLatin1String("charset=")))
                comment.remove(0, comment.indexOf(QLatin1Char(' ')) + 1);
            exifTags.insert(QObject::tr("UserComment"), comment);
        }
    }
#if !EXIV2_TEST_VERSION(0, 28, 0)
    catch (Exiv2::BasicError<wchar_t>& e) {
        qWarning() << "Caught Exiv2::BasicError exception:\n" << e.what() << "\n";
        return;
    }
#endif
    catch (Exiv2::Error& e) {
        qWarning() << "Caught Exiv2 exception:\n" << e.what() << "\n";
        return;
    }
#endif  // USE_EXIV2
}

QMap<QString, QString> DocumentInfo::getExifTags() {
    if(!exifLoaded)
        loadExifTags();
    return exifTags;
}

void DocumentInfo::loadGenerationInfo() {
    if (generationInfoLoaded)
        return;
    generationInfoLoaded = true;
    generationInfo.clear();

    // ComfyUI writes its generation graph into PNG tEXt/zTXt chunks; other
    // formats never carry this data, so skip the parse attempt entirely.
    if (mFormat != "png" && mFormat != "apng")
        return;

    auto result = ComfyKSamplerParser::parseFromPng(filePath());
    if (!result)
        return; // no ComfyUI metadata in this file - not an application error

    const KSamplerInfo &info = *result;

    // Fixed display order: Checkpoint, CLIP, VAE, Sampler, Scheduler, Seed,
    // CFG, Denoise, Steps, LoRA. QList<QPair<>> (unlike QMap) preserves this
    // insertion order all the way through to the UI.
    if (!info.modelName.isEmpty())
        generationInfo.append({ QObject::tr("Checkpoint"), info.modelName });
    if (!info.clipName.isEmpty())
        generationInfo.append({ QObject::tr("CLIP"), info.clipName });
    if (!info.vaeName.isEmpty())
        generationInfo.append({ QObject::tr("VAE"), info.vaeName });
    if (!info.samplerName.isEmpty())
        generationInfo.append({ QObject::tr("Sampler"), info.samplerName });
    if (!info.scheduler.isEmpty())
        generationInfo.append({ QObject::tr("Scheduler"), info.scheduler });
    generationInfo.append({ QObject::tr("Seed"), QString::number(info.seed) });
    generationInfo.append({ QObject::tr("CFG"), QString::number(info.cfg, 'g', 4) });
    generationInfo.append({ QObject::tr("Denoise"), QString::number(info.denoise, 'g', 4) });
    generationInfo.append({ QObject::tr("Steps"), QString::number(info.steps) });
    if (!info.loraNames.isEmpty())
        generationInfo.append({ QObject::tr("LoRA"), info.loraNames.join(QStringLiteral(", ")) });
    if (!info.positivePrompt.isEmpty())
        generationInfo.append({ QObject::tr("Prompt"), info.positivePrompt });
}

QList<QPair<QString, QString>> DocumentInfo::getGenerationInfo() {
    if (!generationInfoLoaded)
        loadGenerationInfo();
    return generationInfo;
}

void DocumentInfo::loadExifOrientation() {
    if(mDocumentType == DocumentType::NONE)
        return;

    QImageReader reader;
    reader.setFileName(filePath());

    if(!mFormat.isEmpty())
        reader.setFormat(mFormat.toLatin1());

    if(reader.canRead())
        mOrientation = static_cast<int>(reader.transformation());
}
