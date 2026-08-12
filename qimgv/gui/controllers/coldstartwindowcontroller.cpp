#include "coldstartwindowcontroller.h"

#include "gui/folderview/folderviewproxy.h"
#include "gui/mainwindow.h"

#include <QDebug>

ColdStartWindowController::ColdStartWindowController(
    MW &window, FolderViewProxy &folderView)
    : window(&window) {
    maximumWaitTimer.setSingleShot(true);
    maximumWaitTimer.setInterval(kMaximumWaitMs);
    documentReadyFallbackTimer.setSingleShot(true);
    documentReadyFallbackTimer.setInterval(kDocumentReadyFallbackMs);

    connect(&folderView, &FolderViewProxy::visibleThumbnailsReady,
            this, &ColdStartWindowController::onVisibleThumbnailsReady);
    connect(&window, &MW::documentRenderingSettled,
            this, &ColdStartWindowController::onDocumentRenderingSettled);
    connect(&documentReadyFallbackTimer, &QTimer::timeout, this, [this]() {
        if(state != State::WaitingForDocumentLayout)
            return;
        qWarning() << "Cold-start document rendering did not settle within"
                   << kDocumentReadyFallbackMs << "ms; revealing the window";
        revealWindow();
    });
    connect(&maximumWaitTimer, &QTimer::timeout, this, [this]() {
        if(state != State::WaitingForFolderView)
            return;
        qWarning() << "Cold-start folder view did not become ready within"
                   << kMaximumWaitMs << "ms; revealing the window";
        revealWindow();
    });
}

void ColdStartWindowController::show() {
    if(!window) {
        qWarning() << "Cannot show the application window: the window was"
                      " destroyed";
        return;
    }

    if(state == State::WaitingForFolderView) {
        if(window->currentViewMode() != MODE_FOLDERVIEW)
            waitForDocumentLayout();
        return;
    }

    if(state == State::WaitingForDocumentLayout) {
        if(window->currentViewMode() == MODE_FOLDERVIEW)
            waitForFolderView();
        else if(window->isDocumentRenderingSettled())
            onDocumentRenderingSettled();
        else
            documentReadyFallbackTimer.start();
        return;
    }

    if(state == State::RevealScheduled) {
        if(window->currentViewMode() != MODE_FOLDERVIEW &&
           !window->isDocumentRenderingSettled())
            waitForDocumentLayout();
        return;
    }

    if(state != State::Initial || window->isVisible()) {
        state = State::Shown;
        window->showDefault();
        return;
    }

    if(window->currentViewMode() != MODE_FOLDERVIEW) {
        waitForDocumentLayout();
        return;
    }

    waitForFolderView();
}

void ColdStartWindowController::waitForFolderView() {
    documentReadyFallbackTimer.stop();
    state = State::WaitingForFolderView;
    window->setWindowOpacity(kHiddenWindowOpacity);
    maximumWaitTimer.start();
    window->showDefault();
}

void ColdStartWindowController::waitForDocumentLayout() {
    maximumWaitTimer.stop();
    state = State::WaitingForDocumentLayout;
    window->setWindowOpacity(kHiddenWindowOpacity);
    window->showDefault();
    if(state == State::WaitingForDocumentLayout &&
       window->isDocumentRenderingSettled()) {
        onDocumentRenderingSettled();
    } else if(state == State::WaitingForDocumentLayout) {
        documentReadyFallbackTimer.start();
    }
}

void ColdStartWindowController::onVisibleThumbnailsReady() {
    if(state != State::WaitingForFolderView)
        return;

    state = State::RevealScheduled;
    QTimer::singleShot(kLayoutSettleDelayMs, this, [this]() {
        if(state == State::RevealScheduled)
            revealWindow();
    });
}

void ColdStartWindowController::onDocumentRenderingSettled() {
    if(state != State::WaitingForDocumentLayout)
        return;

    documentReadyFallbackTimer.stop();
    state = State::RevealScheduled;
    QTimer::singleShot(kLayoutSettleDelayMs, this, [this]() {
        if(state != State::RevealScheduled)
            return;
        if(window && window->currentViewMode() == MODE_FOLDERVIEW) {
            waitForFolderView();
            return;
        }
        if(window && !window->isDocumentRenderingSettled()) {
            waitForDocumentLayout();
            return;
        }
        revealWindow();
    });
}

void ColdStartWindowController::revealWindow() {
    if(state != State::WaitingForFolderView &&
       state != State::WaitingForDocumentLayout &&
       state != State::RevealScheduled)
        return;

    maximumWaitTimer.stop();
    documentReadyFallbackTimer.stop();
    state = State::Shown;
    if(window) {
        window->setWindowOpacity(kVisibleWindowOpacity);
    } else {
        qWarning() << "Cannot reveal the application window: the window was"
                      " destroyed";
    }
}
