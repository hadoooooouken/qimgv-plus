#pragma once

#include <QString>
#include <QDebug>
#include <QPixmap>
#include <memory>
#include "utils/imagelib.h"
#include "utils/stuff.h"
#include "sourcecontainers/documentinfo.h"

class Image {
public:
    Image(QString);
    Image(std::unique_ptr<DocumentInfo>);
    virtual ~Image() = 0;
    virtual std::unique_ptr<QPixmap> getPixmap() = 0;
    virtual std::shared_ptr<const QImage> getImage() = 0;
    virtual std::shared_ptr<const QImage> getDisplayImage() = 0;
    DocumentType type() const;
    QString filePath() const;
    virtual int height() = 0;
    virtual int width() = 0;
    virtual QSize size() = 0;
    bool isLoaded() const;
    QString fileName() const;
    QString baseName() const;
    QString format() const;
    bool isEdited() const;
    qint64 fileSize() const;
    QDateTime lastModified() const;
    QMap<QString, QString> getExifTags();
    virtual int frameCount() const { return 1; }

protected:
    virtual void load() = 0;
    std::unique_ptr<DocumentInfo> mDocInfo;
    bool mLoaded, mEdited;
    QString mPath;
    QSize resolution;
};
