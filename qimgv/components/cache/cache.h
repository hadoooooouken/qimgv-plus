#pragma once

#include <QMap>
#include <QMutex>
#include <functional>
#include <memory>
#include "sourcecontainers/image.h"

class Cache {
public:
    explicit Cache();
    bool contains(QString path) const;
    void remove(QString path);
    void clear();

    bool insert(std::shared_ptr<Image> img);
    void trimTo(QStringList list);

    std::shared_ptr<Image> get(QString path);
    const QList<QString> keys() const;

private:
    using RemovalPredicate = std::function<bool(const QString &)>;

    void removeMatching(const RemovalPredicate &shouldRemove);

    QMap<QString, std::shared_ptr<Image>> items;
    // Guards the map.
    mutable QMutex mutex;
};
