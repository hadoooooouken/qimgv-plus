#ifndef FILESYSTEMMODELCUSTOM_H
#define FILESYSTEMMODELCUSTOM_H

#include <QApplication>
#include <QFileSystemModel>
#include <QPainter>
#include <QHash>
#include <QString>
#include "utils/iconfontmanager.h"

class FileSystemModelCustom : public QFileSystemModel
{
public:
    FileSystemModelCustom(QObject *parent = nullptr);
    ~FileSystemModelCustom() override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    void refreshPath(const QString &path = QString());

protected:
    Qt::ItemFlags flags(const QModelIndex &index) const override;
private:
    static constexpr int kFolderIconSizePx = 16;

    void updateFolderIcon();

    QPixmap folderIcon;
    mutable QHash<QString, bool> hasSubfoldersCache;
};

#endif // FILESYSTEMMODELCUSTOM_H
