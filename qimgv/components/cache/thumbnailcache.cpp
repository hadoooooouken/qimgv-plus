#include "thumbnailcache.h"
#include <QBuffer>
#include <QThread>

ThumbnailCache::ThumbnailCache() {
    cacheDirPath = settings->thumbnailCacheDir();
}

QSqlDatabase ThumbnailCache::getDatabaseConnection() {
    QString connectionName = QString("thumbnail_db_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            return db;
        }
    }
    
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(cacheDirPath + "thumbnails.db");
    if (db.open()) {
        QSqlQuery query(db);
        query.exec("PRAGMA journal_mode=WAL;");
        query.exec("PRAGMA synchronous=NORMAL;");
        query.exec("CREATE TABLE IF NOT EXISTS thumbnails ("
                   "id TEXT PRIMARY KEY, "
                   "last_modified TEXT, "
                   "original_width INTEGER, "
                   "original_height INTEGER, "
                   "label TEXT, "
                   "data BLOB);");
    }
    return db;
}

QString ThumbnailCache::thumbnailPath(QString id) {
    Q_UNUSED(id)
    return cacheDirPath + "thumbnails.db";
}

bool ThumbnailCache::exists(QString id) {
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return false;
    
    QSqlQuery query(db);
    query.prepare("SELECT 1 FROM thumbnails WHERE id = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return true;
    }
    return false;
}

void ThumbnailCache::saveThumbnail(const QImage *image, QString id) {
    if (!image) return;
    
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return;
    
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image->save(&buffer, "JXL", 85);
    
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO thumbnails (id, last_modified, original_width, original_height, label, data) "
                  "VALUES (:id, :last_modified, :original_width, :original_height, :label, :data)");
    query.bindValue(":id", id);
    query.bindValue(":last_modified", image->text("lastModified"));
    query.bindValue(":original_width", image->text("originalWidth").toInt());
    query.bindValue(":original_height", image->text("originalHeight").toInt());
    query.bindValue(":label", image->text("label"));
    query.bindValue(":data", ba);
    
    if (!query.exec()) {
        qWarning() << "Failed to save thumbnail to DB:" << query.lastError().text();
    }
}

std::unique_ptr<QImage> ThumbnailCache::readThumbnail(QString id) {
    QSqlDatabase db = getDatabaseConnection();
    if (!db.isOpen()) return nullptr;
    
    QSqlQuery query(db);
    query.prepare("SELECT last_modified, original_width, original_height, label, data FROM thumbnails WHERE id = :id");
    query.bindValue(":id", id);
    
    if (query.exec() && query.next()) {
        QString lastModified = query.value(0).toString();
        int originalWidth = query.value(1).toInt();
        int originalHeight = query.value(2).toInt();
        QString label = query.value(3).toString();
        QByteArray ba = query.value(4).toByteArray();
        
        auto thumb = std::make_unique<QImage>();
        if (thumb->loadFromData(ba)) {
            thumb->setText("lastModified", lastModified);
            thumb->setText("originalWidth", QString::number(originalWidth));
            thumb->setText("originalHeight", QString::number(originalHeight));
            thumb->setText("label", label);
            return thumb;
        }
    }
    return nullptr;
}

void ThumbnailCache::clear() {
    QSqlDatabase db = getDatabaseConnection();
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.exec("DELETE FROM thumbnails;");
        query.exec("VACUUM;");
    }
}
