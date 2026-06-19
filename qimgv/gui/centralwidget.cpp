#include "centralwidget.h"
#include "settings.h"
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QResizeEvent>

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
    switchView(0);
}

void CentralWidget::showFolderView() {
    if(mode == MODE_FOLDERVIEW)
        return;

    mode = MODE_FOLDERVIEW;
    switchView(1);
}

void CentralWidget::toggleViewMode() {
    (mode == MODE_DOCUMENT) ? showFolderView() : showDocumentView();
}

ViewMode CentralWidget::currentViewMode() {
    return mode;
}

void CentralWidget::switchView(int index) {
    if (isVisible()) {
        QPixmap oldPixmap = grab();

        if (overlayLabel) {
            delete overlayLabel;
        }

        setUpdatesEnabled(false);

        overlayLabel = new QLabel(this);
        overlayLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        overlayLabel->setPixmap(oldPixmap);
        overlayLabel->setGeometry(rect());
        overlayLabel->show();

        setCurrentIndex(index);
        QWidget *w = widget(index);
        if (w) {
            w->show();
            w->setFocus();
        }

        overlayLabel->raise();

        setUpdatesEnabled(true);
        overlayLabel->repaint();

        QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(overlayLabel);
        effect->setOpacity(1.0);
        overlayLabel->setGraphicsEffect(effect);

        QPropertyAnimation *animation = new QPropertyAnimation(effect, "opacity", overlayLabel);
        animation->setDuration(160);
        animation->setStartValue(1.0);
        animation->setEndValue(0.0);
        animation->setEasingCurve(QEasingCurve::OutQuad);
        connect(animation, &QPropertyAnimation::finished, overlayLabel, &QLabel::deleteLater);

        animation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        setCurrentIndex(index);
        QWidget *w = widget(index);
        if (w) {
            w->show();
            w->setFocus();
        }
    }
}

void CentralWidget::resizeEvent(QResizeEvent *event) {
    QStackedWidget::resizeEvent(event);
    if (overlayLabel) {
        overlayLabel->setGeometry(rect());
    }
}
