#include "mainwindowshell.h"

#include <QDebug>

bool MainWindowShell::folderViewActive() const {
    return mFolderViewActive;
}

void MainWindowShell::setFolderViewActive(bool active) {
    if (mFolderViewActive == active)
        return;
    mFolderViewActive = active;
    emit folderViewActiveChanged();
}

bool MainWindowShell::fullscreen() const {
    return mFullscreen;
}

void MainWindowShell::setFullscreen(bool fullscreen) {
    if (mFullscreen == fullscreen)
        return;
    mFullscreen = fullscreen;
    emit fullscreenChanged();
}

void MainWindowShell::dropUrls(const QList<QUrl> &urls, QObject *source) {
    if (urls.isEmpty()) {
        qWarning() << "MainWindowShell: a drop without URLs was ignored";
        return;
    }
    emit urlsDropped(urls, source);
}
