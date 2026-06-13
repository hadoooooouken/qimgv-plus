#pragma once

#include <QObject>
#include <memory>
#include <QDir>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "settings.h"
#include "sourcecontainers/thumbnail.h"

class ThumbnailCache : public QObject
{
    Q_OBJECT
public:
    explicit ThumbnailCache();

    void saveThumbnail(const QImage *image, QString id);
    std::unique_ptr<QImage> readThumbnail(QString id);
    QString thumbnailPath(QString id);
    bool exists(QString id);
    void clear();

signals:

public slots:

private:
    QSqlDatabase getDatabaseConnection();
    QString cacheDirPath;
};
