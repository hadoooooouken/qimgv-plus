#include "bookmarksitem.h"
#include <QDrag>
#include <QApplication>
#include "bookmarkswidget.h"
#include "gui/uimetrics.h"
#include <QFileInfo>

namespace {

constexpr int kFolderIconSizePx = UiMetrics::kCompactIconSizePx;
constexpr int kRemoveIconVerticalOffsetPx = 1;

} // namespace

BookmarksItem::BookmarksItem(QString _dirName, QString _dirPath, QWidget *parent)
    : QWidget(parent), dirName(_dirName), dirPath(_dirPath), mHighlighted(false)
{
    this->setContentsMargins(0,0,0,0);
    layout.setContentsMargins(10,6,10,6);
    setAcceptDrops(true);
    dirNameLabel.setText(dirName);

    spacer = new QSpacerItem(16, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    folderIconWidget.setAttribute(Qt::WA_TransparentForMouseEvents, true);
    folderIconWidget.setIcon(FluentIcon::Folder16, kFolderIconSizePx);
    folderIconWidget.setColorMode(ICON_COLOR_THEME_FOLDER);
    folderIconWidget.setMinimumSize(kFolderIconSizePx, kFolderIconSizePx);
    folderIconWidget.installEventFilter(this);

    removeItemButton.setIcon(FluentIcon::BookmarkRemove20, UiMetrics::kCompactIconSizePx);
    removeItemButton.setIconOffset(0, kRemoveIconVerticalOffsetPx);
    removeItemButton.setMinimumSize(UiMetrics::kCompactIconSizePx,
                                    UiMetrics::kCompactIconSizePx);
    removeItemButton.installEventFilter(this);
    removeItemButton.hide();

    removeItemButton.setAccessibleName("BookmarksItemRemoveLabel");

    connect(&removeItemButton, &IconButton::clicked, this, &BookmarksItem::onRemoveClicked);

    layout.addWidget(&folderIconWidget);
    layout.addWidget(&dirNameLabel);
    layout.addSpacerItem(spacer);
    layout.addWidget(&removeItemButton);

    setMouseTracking(true);

    setLayout(&layout);
}

QString BookmarksItem::path() {
    return dirPath;
}

void BookmarksItem::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    removeItemButton.show();
}

void BookmarksItem::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    removeItemButton.hide();
}

void BookmarksItem::setHighlighted(bool mode) {
    if(mode != mHighlighted) {
        mHighlighted = mode;
        setProperty("highlighted", mHighlighted);
        style()->unpolish(this);
        style()->polish(this);
    }
}

void BookmarksItem::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    if(event->button() == Qt::LeftButton)
        emit clicked(dirPath);
}

void BookmarksItem::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton)
        dragStartPosition = event->pos();
    event->accept();
}

void BookmarksItem::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if ((event->pos() - dragStartPosition).manhattanLength()
         < QApplication::startDragDistance())
        return;

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;

    mimeData->setData("application/x-qimgv-bookmark", dirPath.toUtf8());
    drag->setMimeData(mimeData);

    drag->exec(Qt::MoveAction);
}

void BookmarksItem::onRemoveClicked() {
    emit removeClicked(dirPath);
}

void BookmarksItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void BookmarksItem::dropEvent(QDropEvent *event) {
    QList<QString> files;
    QList<QString> folders;
    const auto urls = event->mimeData()->urls();
    for(const auto &url : urls) {
        QString localPath = url.toLocalFile();
        if(!localPath.isEmpty()) {
            QFileInfo fi(localPath);
            bool isDir = fi.isDir();
            if (!isDir && fi.isSymLink()) {
                QFileInfo target(fi.symLinkTarget());
                isDir = target.isDir();
            }
            if (isDir) {
                folders << localPath;
            } else if (fi.isFile()) {
                files << localPath;
            }
        }
    }

    if(!folders.isEmpty()) {
        auto parent = qobject_cast<BookmarksWidget*>(parentWidget());
        if(parent) {
            for(const auto &folder : folders) {
                parent->addBookmark(folder);
            }
        }
    }

    if(!files.isEmpty()) {
        emit droppedIn(files, dirPath, event->dropAction());
    }

    setProperty("hover", false);
    update();
    event->acceptProposedAction();
}

void BookmarksItem::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
        return;
    }
    setProperty("hover", true);
    update();
}

void BookmarksItem::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event)
    setProperty("hover", false);
    update();
}
