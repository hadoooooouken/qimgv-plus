#pragma once

#include <QList>
#include <QPair>
#include <QSize>
#include <QString>
#include <QtTypes>

#include "settings_types.h"

// Snapshot of the current file and navigation state shown in the window
// title, info bar and overlays.
struct ShellFileInfo {
    int index = -1;
    int fileCount = 0;
    QString filePath;
    QString fileName;
    QSize imageSize;
    qint64 fileSize = 0;
    QString format;
    QString colorProfile;
    bool slideshow = false;
    bool shuffle = false;
    bool edited = false;
};

// Ordered (label, value) pairs; the order is kept for display.
using MetadataEntries = QList<QPair<QString, QString>>;

// Outbound UI port for application-shell state: the window title and info
// bars, metadata overlay, folder view header and the panels/overlays the
// core opens. Must only be called on the GUI thread.
class IShellPort {
public:
    virtual ~IShellPort() = default;

    virtual void setDirectoryPath(const QString &path) = 0;
    virtual void setCurrentInfo(const ShellFileInfo &info) = 0;
    virtual void setMetadata(const MetadataEntries &entries) = 0;
    virtual void notifySortingChanged(SortingMode mode) = 0;
    virtual void notifyFolderSortingChanged(SortingMode mode) = 0;
    // Re-reads the given directory in the folder tree.
    virtual void refreshFolderTree(const QString &directoryPath) = 0;

    virtual void setSaveOverlayVisible(bool visible) = 0;
    [[nodiscard]] virtual bool isCropPanelActive() const = 0;
    virtual void toggleCropPanel() = 0;
    virtual void toggleFullscreenInfoBar() = 0;
    // Opens (or closes, when open) the rename prompt pre-filled with
    // currentName. The chosen name arrives via UiEvents::renameRequested.
    virtual void toggleRenamePrompt(const QString &currentName) = 0;

protected:
    IShellPort() = default;
    IShellPort(const IShellPort &) = default;
    IShellPort &operator=(const IShellPort &) = default;
};
