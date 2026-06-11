#include "treeviewcustom.h"

TreeViewCustom::TreeViewCustom(QWidget *parent) : QTreeView(parent) {
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragEnabled(true);

    // proxy scrollbar
    this->verticalScrollBar()->setStyleSheet("max-width: 0px;");
    overlayScrollbar.setParent(this);
    overlayScrollbar.setProperty("treeHovered", false);
    initScrollbarStyle();
    connect(settings, &Settings::settingsChanged, this, &TreeViewCustom::initScrollbarStyle);
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, &overlayScrollbar, &QScrollBar::setRange);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, &overlayScrollbar, &QScrollBar::setValue);
    connect(&overlayScrollbar, &QScrollBar::valueChanged, [this]() {
        this->verticalScrollBar()->setValue(overlayScrollbar.value());
    });
}

void TreeViewCustom::dropEvent(QDropEvent *event) {
    QModelIndex dropIndex = indexAt(event->position().toPoint());
    if(dropIndex.isValid()) {
        QList<QString> paths;
        const auto urls = event->mimeData()->urls();
        for(const auto &url : urls)
            paths << url.toLocalFile();
        emit droppedIn(paths, dropIndex);
    }
}

void TreeViewCustom::dragEnterEvent(QDragEnterEvent *event) {
    if(event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void TreeViewCustom::showEvent(QShowEvent *event) {
    QTreeView::showEvent(event);
}

void TreeViewCustom::enterEvent(QEnterEvent *event) {
    QTreeView::enterEvent(event);
    updateScrollbarStyle();
}
void TreeViewCustom::leaveEvent(QEvent *event) {
    QTreeView::leaveEvent(event);
    updateScrollbarStyle();
}

QSize TreeViewCustom::minimumSizeHint() const {
    QSize sz(QTreeView::minimumSizeHint().width(), 0);
    return sz;
}

void TreeViewCustom::resizeEvent(QResizeEvent *event) {
    QTreeView::resizeEvent(event);
    updateScrollbarStyle();
}

void TreeViewCustom::initScrollbarStyle() {
    auto scheme = settings->colorScheme();
    QString baseStyle = 
        "QScrollBar { background-color: transparent; }"
        "QScrollBar::handle:vertical { background-color: %1; }"
        "QScrollBar[treeHovered=\"true\"]::handle:vertical { background-color: %2; }"
        "QScrollBar::handle:vertical:hover { background-color: %3; }";
    overlayScrollbar.setStyleSheet(baseStyle.arg(
        scheme.folderview_hc.name(),
        scheme.scrollbar.name(),
        scheme.scrollbar_hover.name()
    ));
}

void TreeViewCustom::updateScrollbarStyle() {
    bool isHovered = rect().contains(mapFromGlobal(cursor().pos()));

    if (overlayScrollbar.property("treeHovered").toBool() != isHovered) {
        overlayScrollbar.setProperty("treeHovered", isHovered);
        overlayScrollbar.style()->unpolish(&overlayScrollbar);
        overlayScrollbar.style()->polish(&overlayScrollbar);
        overlayScrollbar.update();
    }
    overlayScrollbar.setGeometry(width() - SCROLLBAR_WIDTH, 0, SCROLLBAR_WIDTH, height());

    overlayScrollbar.setVisible( (this->verticalScrollBar()->maximum()) );
}

void TreeViewCustom::keyPressEvent(QKeyEvent* event) {
    QModelIndex currentIndex = this->currentIndex();
    if( (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ) && currentIndex.isValid() ) {
        emit clicked(currentIndex);
    } else {
        QTreeView::keyPressEvent(event);
    }
}
