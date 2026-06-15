#include "bookmarkswidget.h"
#include "settings.h"
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QFileInfo>

BookmarksWidget::BookmarksWidget(QWidget *parent) : QWidget(parent), highlightedPath("") {
    setAcceptDrops(true);
    setContentsMargins(0,0,0,0);
    layout.setContentsMargins(0,0,0,0);
    layout.setSpacing(0);
    setLayout(&layout);
    connect(settings, &Settings::settingsChanged, this, &BookmarksWidget::readSettings);
    readSettings();
}

BookmarksWidget::~BookmarksWidget() {
}

void BookmarksWidget::readSettings() {
    QStringList _paths = settings->bookmarks();
    for(const auto &path : std::as_const(_paths))
        addBookmark(path);
    if(_paths.empty())
        addBookmark(QDir::homePath());
}

void BookmarksWidget::saveBookmarks() {
    settings->setBookmarks(paths);
}

void BookmarksWidget::addBookmark(QString dirPath) {
    if(paths.contains(dirPath))
        return;
    paths.push_back(dirPath);
    QUrl url(dirPath);
    BookmarksItem *item = new BookmarksItem(url.fileName(), dirPath);
    layout.addWidget(item);
    item->show();
    connect(item, &BookmarksItem::clicked, this, &BookmarksWidget::bookmarkClicked);
    connect(item, &BookmarksItem::removeClicked, this, &BookmarksWidget::removeBookmark);
    connect(item, &BookmarksItem::droppedIn, this, &BookmarksWidget::droppedIn);
    saveBookmarks();
}

void BookmarksWidget::removeBookmark(QString dirPath) {
    for(int i = 0; i < layout.count(); i++) {
        auto w = dynamic_cast<BookmarksItem*>(layout.itemAt(i)->widget());
        if(w && w->path() == dirPath) {
            if(highlightedPath == dirPath)
                highlightedPath = "";
            layout.removeWidget(w);
            disconnect(w, &BookmarksItem::clicked, this, &BookmarksWidget::bookmarkClicked);
            disconnect(w, &BookmarksItem::removeClicked, this, &BookmarksWidget::removeBookmark);
            disconnect(w, &BookmarksItem::droppedIn, this, &BookmarksWidget::droppedIn);
            w->deleteLater();
            paths.removeAll(dirPath);
            saveBookmarks();
            break;
        }
    }
}

void BookmarksWidget::onPathChanged(QString path) {
    if(highlightedPath == path)
        return;
    if(paths.contains(highlightedPath)) {
        int currentIndex = paths.indexOf(highlightedPath);
        auto w = dynamic_cast<BookmarksItem*>(layout.itemAt(currentIndex)->widget());
        w->setHighlighted(false);
        highlightedPath = "";
    }
    if(paths.contains(path)) {
        int newIndex = paths.indexOf(path);
        auto w = dynamic_cast<BookmarksItem*>(layout.itemAt(newIndex)->widget());
        w->setHighlighted(true);
        highlightedPath = path;
    }
}

void BookmarksWidget::dropEvent(QDropEvent *event) {
    if(event->mimeData()->hasFormat("application/x-qimgv-bookmark")) {
        QString path = QString::fromUtf8(event->mimeData()->data("application/x-qimgv-bookmark"));
        int oldIndex = paths.indexOf(path);
        if(oldIndex == -1) return;

        int targetIndex = -1;
        for(int i = 0; i < layout.count(); i++) {
            auto w = layout.itemAt(i)->widget();
            if(w && event->pos().y() < w->geometry().center().y()) {
                targetIndex = i;
                break;
            }
        }
        if(targetIndex == -1) targetIndex = layout.count() - 1;

        if(oldIndex != targetIndex) {
            paths.move(oldIndex, targetIndex);
            // Re-ordering widgets in QVBoxLayout
            QLayoutItem *item = layout.takeAt(oldIndex);
            layout.insertItem(targetIndex, item);
            saveBookmarks();
        }
        event->acceptProposedAction();
    } else if(event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        bool accepted = false;
        for(const auto &url : urls) {
            QString localPath = url.toLocalFile();
            if(!localPath.isEmpty() && QFileInfo(localPath).isDir()) {
                addBookmark(localPath);
                accepted = true;
            }
        }
        if(accepted) {
            event->acceptProposedAction();
        }
    }
}

void BookmarksWidget::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls() || event->mimeData()->hasFormat("application/x-qimgv-bookmark")) {
        event->acceptProposedAction();
    }
}

void BookmarksWidget::dragMoveEvent(QDragMoveEvent *event) {
    if(event->mimeData()->hasUrls() || event->mimeData()->hasFormat("application/x-qimgv-bookmark")) {
        event->acceptProposedAction();
    }
}
