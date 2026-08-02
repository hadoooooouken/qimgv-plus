#include "coldstartwindowcontroller.h"

#include "gui/folderview/folderviewproxy.h"
#include "gui/mainwindow.h"

#include <QDebug>

ColdStartWindowController::ColdStartWindowController(
    MW &window, FolderViewProxy &folderView)
    : window(&window) {
    maximumWaitTimer.setSingleShot(true);
    maximumWaitTimer.setInterval(kMaximumWaitMs);

    connect(&folderView, &FolderViewProxy::visibleThumbnailsReady,
            this, &ColdStartWindowController::onVisibleThumbnailsReady);
    connect(&maximumWaitTimer, &QTimer::timeout, this, [this]() {
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

    if(state == State::WaitingForFolderView ||
       state == State::RevealScheduled) {
        if(window->currentViewMode() != MODE_FOLDERVIEW)
            revealWindow();
        return;
    }

    if(state != State::Initial || window->isVisible()) {
        state = State::Shown;
        window->showDefault();
        return;
    }

    if(window->currentViewMode() != MODE_FOLDERVIEW) {
        state = State::Shown;
        window->showDefault();
        return;
    }

    state = State::WaitingForFolderView;
    window->setWindowOpacity(kHiddenWindowOpacity);
    maximumWaitTimer.start();
    window->showDefault();
}

void ColdStartWindowController::onVisibleThumbnailsReady() {
    if(state != State::WaitingForFolderView)
        return;

    state = State::RevealScheduled;
    QTimer::singleShot(kLayoutSettleDelayMs, this,
                       &ColdStartWindowController::revealWindow);
}

void ColdStartWindowController::revealWindow() {
    if(state != State::WaitingForFolderView &&
       state != State::RevealScheduled)
        return;

    maximumWaitTimer.stop();
    state = State::Shown;
    if(window) {
        window->setWindowOpacity(kVisibleWindowOpacity);
    } else {
        qWarning() << "Cannot reveal the application window: the window was"
                      " destroyed";
    }
}
