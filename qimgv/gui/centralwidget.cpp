#include "centralwidget.h"
#include "settings.h"

CentralWidget::CentralWidget(std::shared_ptr<DocumentWidget> _docWidget, std::shared_ptr<FolderViewProxy> _folderView, QWidget *parent)
    : QStackedWidget(parent),
      documentView(_docWidget),
      folderView(_folderView)
{
    setMouseTracking(true);
    if(!documentView || !folderView)
        qWarning() << "[CentralWidget] Error: child widget is null. We will crash now.  Bye.";

    // docWidget - 0, folderView - 1
    addWidget(documentView.get());
    if(folderView)
        addWidget(folderView.get());

    if (settings->defaultViewMode() == MODE_FOLDERVIEW) {
        mode = MODE_DOCUMENT;
        showFolderView();
    } else {
        mode = MODE_FOLDERVIEW;
        showDocumentView();
    }
}

void CentralWidget::showDocumentView() {
    if(mode == MODE_DOCUMENT)
        return;
    mode = MODE_DOCUMENT;
    setCurrentIndex(0);
    widget(0)->setFocus();
}

void CentralWidget::showFolderView() {
    if(mode == MODE_FOLDERVIEW)
        return;

    mode = MODE_FOLDERVIEW;
    setCurrentIndex(1);
    widget(1)->show();
    widget(1)->setFocus();
}

void CentralWidget::toggleViewMode() {
    (mode == MODE_DOCUMENT) ? showFolderView() : showDocumentView();
}

ViewMode CentralWidget::currentViewMode() {
    return mode;
}
