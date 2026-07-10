#pragma once

#include <QString>
#include <QSize>
#include <QUrl>
#include <QMimeDatabase>
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <cmath>
#include <cstring>
#include "utils/stuff.h"
#ifdef USE_EXIV2

#include <exiv2/exiv2.hpp>
#include <iostream>
#include <iomanip>
#include <cassert>

#endif

#include <QImageReader>

enum DocumentType { NONE, STATIC, ANIMATED };

class DocumentInfo {
public:
    DocumentInfo(QString path);
    ~DocumentInfo();
    
    QString directoryPath() const;
    QString filePath() const;
    QString fileName() const;
    QString baseName() const;
    qint64 fileSize() const;
    DocumentType type() const;
    QMimeType mimeType() const;

    // file extension (guessed from mime-type)
    QString format() const;
    int exifOrientation() const;

    QDateTime lastModified() const;
    void refresh();
    void loadExifTags();
    QMap<QString, QString> getExifTags();
    void loadGenerationInfo();
    QList<QPair<QString, QString>> getGenerationInfo();

private:
    QFileInfo fileInfo;
    DocumentType mDocumentType;
    int mOrientation;
    QString mFormat;
    bool exifLoaded;
    bool generationInfoLoaded;

    // guesses file type from its contents
    // and sets extension
    void detectFormat();
    void loadExifOrientation();
    bool detectAPNG();
    bool detectAnimatedWebP();
    bool detectAnimatedJxl();
    bool detectAnimatedAvif();
    QMap<QString, QString> exifTags;
    // Order-preserving (unlike QMap, which is always sorted by key): the
    // display order for this data is meaningful (Checkpoint, CLIP, VAE,
    // Sampler, Scheduler, Seed, CFG, Denoise, Steps, LoRA) and isn't
    // alphabetical, so insertion order must be kept intact through to the UI.
    QList<QPair<QString, QString>> generationInfo;
    QMimeType mMimeType;
};
