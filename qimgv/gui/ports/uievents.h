#pragma once

#include <QList>
#include <QMimeData>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

#include "settings_types.h"
#include "utils/coloradjustments.h"

// Inbound UI port: user intents and UI readiness notifications that a user
// interface reports to the core. The active UI emits these signals (usually
// by forwarding its own); Core and its controllers connect to them. Lives on
// the GUI thread.
class UiEvents final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

signals:
    // Navigation and file activation
    void pathOpened(QString path);
    void droppedIn(const QMimeData *mimeData, QObject *source);
    void draggedOut();
    void nextImageRequested();
    void prevImageRequested();

    // File operations
    void copyRequested(QString destDirectory);
    void moveRequested(QString destDirectory);
    void copyUrlsRequested(QList<QString> paths, QString destDirectory);
    void moveUrlsRequested(QList<QString> paths, QString destDirectory);
    void renameRequested(QString newName);
    void batchConversionRequested();

    // Editing and saving
    void cropRequested(QRect rect);
    void cropAndSaveRequested(QRect rect);
    void colorAdjustmentsApplyRequested(ColorAdjustments adjustments);
    void discardEditsRequested();
    void saveRequested();
    void saveAsRequested();

    // Folder view configuration
    void sortingSelected(SortingMode mode);
    void folderSortingSelected(SortingMode mode);
    void formatFilterSelected(QStringList extensions);
    void nameFilterSelected(QString nameFilter);
    void showFoldersChanged(bool showFolders);

    // Viewer
    void scalingRequested(QSize size, ScalingFilter filter);

    // Application
    void clearThumbnailCacheRequested();
    void suspendRequested();

    // Readiness (used to reveal the window without flashing)
    void documentRenderingSettled();
    void visibleThumbnailsReady();
    void filesystemViewReady();
};
