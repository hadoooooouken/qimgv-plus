#pragma once

#include <QObject>
#include <QDebug>
#include <QMutex>
#include <QClipboard>
#include <QDrag>
#include <QImage>
#include <QQueue>
#include <QFileSystemModel>
#include <QDesktopServices>
#include <QTranslator>
#include <QSet>
#include <QTimer>
#include <optional>
#include "appversion.h"
#include "settings_types.h"
#include "components/directorymodel.h"
#include "components/directorypresenter.h"
#include "components/thumbnailer/thumbnailer.h"
#include "components/scriptmanager/scriptmanager.h"
#include "gui/mainwindow.h"
#include "gui/controllers/coldstartwindowcontroller.h"
#include "utils/randomizer.h"
#include "gui/dialogs/printdialog.h"

class ImageStatic;

#ifdef __GLIBC__
#include <malloc.h>
#endif

struct State {
    bool hasActiveImage = false;
    bool delayModel = false;
    QString currentFilePath = "";
    QString directoryPath = "";
    std::shared_ptr<Image> currentImg;
};

enum MimeDataTarget {
    TARGET_CLIPBOARD,
    TARGET_DROP
};

class Core : public QObject {
    Q_OBJECT
public:
    Core();
    ~Core();
    void showGui();
    bool hasActiveState() const;
    void loadDefaultPath();

public slots:
    void updateInfoString();
    bool loadPath(QString);
    void raiseWindow(const QString &pathReceived = QString());
    void suspendToStandby();
    void forceExit();


private:
    QElapsedTimer t;

    void initGui();
    void initComponents();
    void connectComponents();
    void initActions();
    void loadTranslation();
    void onUpdate();
    void onFirstRun();
    void processRaiseWindowRequest(const QString &pathReceived);
    void drainRaiseWindowRequests();

    // ui stuff
    MW *mw;
    std::unique_ptr<ColdStartWindowController> coldStartWindowController;

    State state;
    bool loopSlideshow, slideshow, shuffle;
    FolderEndAction folderEndAction;

    // components
    std::shared_ptr<DirectoryModel> model;

    // Shared between thumbPanelPresenter and folderViewPresenter (they
    // always mirror the same DirectoryModel/folder) so a thumbnail
    // requested by both panels at once is decoded once, not twice. See
    // DirectoryPresenter::setThumbnailer().
    std::shared_ptr<Thumbnailer> thumbnailer;

    DirectoryPresenter thumbPanelPresenter, folderViewPresenter;

    void rotateByDegrees(int degrees);
    void reset();
    bool setDirectory(QString path);

    QDrag *mDrag;
    QMimeData *getMimeDataForImage(std::shared_ptr<Image> img, MimeDataTarget target);
    QTranslator *translator = nullptr;

    Randomizer randomizer;
    void syncRandomizer();

    void attachModel(DirectoryModel *_model);
    QString selectedPath();
    void guiSetImage(std::shared_ptr<Image> img);
    void maybeShowPageHint(const std::shared_ptr<Image> &img);
    void showPageChangeMessage(const QString &path);
    QTimer slideshowTimer;
    QTimer preloadTimer;

    // paths for which the "this document has multiple pages" hint was
    // already shown once during the current folder visit; cleared in reset()
    QSet<QString> autoPageHintShown;

    void startSlideshowTimer();
    void startSlideshow();
    void stopSlideshow();

    [[nodiscard]] ImageSaveResult saveFile(const QString &filePath,
                                           const QString &newPath);
    [[nodiscard]] ImageSaveResult saveFile(const QString &filePath);

    std::shared_ptr<ImageStatic> getEditableImage(const QString &filePath);
    QList<QString> currentSelection();

    template<typename... Args>
    void edit_template(bool save, QString actionName, const std::function<QImage(std::shared_ptr<const QImage>, Args...)>& func, Args&&... as);

    void doInteractiveCopy(QString path, QString destDirectory, DialogResult &overwriteAllFiles);
    void doInteractiveMove(QString path, QString destDirectory, DialogResult &overwriteAllFiles);

private slots:
    void readSettings();
    void nextImage();
    void prevImage();
    void nextImageSlideshow();
    void jumpToFirst();
    void jumpToLast();
    void onModelItemReady(std::shared_ptr<Image>, const QString&);
    void onModelItemUpdated(QString fileName);
    void onModelSortingChanged(SortingMode mode);
    void onFolderSortingSelected(SortingMode mode);
    void onFormatFilterSelected(QStringList extensions);
    void onLoadFailed(const QString &path);
    void rotateLeft();
    void rotateRight();
    void nextPage();
    void prevPage();
    void scalingRequest(QSize, ScalingFilter);
    void onScalingFinished(QImage scaled, ScalerRequest req);
    void copyCurrentFile(QString destDirectory);
    void moveCurrentFile(QString destDirectory);
    void copyPathsTo(QList<QString> paths, QString destDirectory);
    void interactiveCopy(QList<QString> paths, QString destDirectory);
    void interactiveMove(QList<QString> paths, QString destDirectory);
    void movePathsTo(QList<QString> paths, QString destDirectory);
    void onDirectoryPresenterDroppedInto(QList<QString> paths, QString destDirectory, Qt::DropAction action);
    FileOpResult removeFile(QString fileName, bool trash);
    void onFileRemoved(QString filePath, int index);
    void onFileRenamed(QString fromPath, int indexFrom, QString toPath, int indexTo);
    void onFileAdded(QString filePath);
    void onFileModified(QString filePath);
    void showResizeDialog();
    void showBatchConverter();
    void resize(QSize size, ScalingFilter filter, bool useUpscayl = false, QString upscaylModel = "");
    void flipH();
    void flipV();
    void crop(QRect rect);
    void cropAndSave(QRect rect);
    void discardEdits();
    void applyColorAdjustments(float exposure, float contrast, float brightness, float temperature, float tint, float saturation, float hue);
    void toggleCropPanel();
    void toggleFullscreenInfoBar();
    void requestSavePath();
    void saveCurrentFile();
    void saveCurrentFileAs(QString);
    void runScript(const QString&);
    void setWallpaper();
    void removePermanent();
    void moveToTrash();
    void reloadImage();
    void reloadImage(QString fileName);
    void copyFileClipboard();
    void copyPathClipboard();
    void openFromClipboard();
    void renameCurrentSelection(QString newName);
    void sortBy(SortingMode mode);
    void sortByName();
    void sortByTime();
    void sortBySize();
    void showRenameDialog();
    void onDraggedOut();
    void onDraggedOut(QList<QString> paths);
    void onDropIn(const QMimeData *mimeData, QObject* source);
    void toggleShuffle();
    void onModelLoaded();
    void outputError(const FileOpResult &error) const;
    void showInDirectory();
    void createDirectory();
    void onDirectoryViewFileActivated(QString filePath);
    void onDirectoryViewFilesActivated(QList<QString> filePaths, QString activePath);
    // Opens the batch converter dialog once folderViewPresenter's background
    // scan (kicked off by showBatchConverter()) has expanded the current
    // selection into a concrete file list; see DirectoryPresenter::
    // requestExpandedSelectedPathsAsync().
    void onBatchConverterPathsReady(QList<QString> filePaths, QString defaultOutputDir);
    bool loadFileList(const QList<QString> &filePaths, QString activePath = "");
    bool loadFileIndex(int index, bool async, bool preload);
    void enableDocumentView();
    void enableFolderView();
    void toggleFolderView();
    void toggleSlideshow();
    void onPlaybackFinished();
    void setFoldersDisplay(bool mode);
    void loadParentDir();
    void nextDirectory();
    void prevDirectory(bool selectLast);
    void prevDirectory();
    void print();
    void historyBack();
    void historyForward();
    void modelDelayLoad();
    void preloadNeighbors();

private:
    struct AiResizeOperation {
        int generation = 0;
        QString path;
        quint64 sourceRevision = 0;
        std::weak_ptr<ImageStatic> sourceImage;
    };

    std::unique_ptr<class Upscaler> upscaler;
    std::unique_ptr<class WallpaperController> wallpaperController;
    int aiResizeGeneration = 0;
    std::optional<AiResizeOperation> activeAiResizeOperation;
    bool aiResizeBusyUiActive = false;
    void clearAiResizeBusyUi();

private slots:
    void onAiResizeFinished(int generation, QString path, QImage image, bool success, QString error);

private:
    // Keeps one invocation responsible for draining re-entrant raise-window
    // requests. Sets the flag for the guard's lifetime and resets it on every
    // exit path via RAII.
    class RaiseWindowGuard {
    public:
        explicit RaiseWindowGuard(bool &flag) : active(flag) { active = true; }
        ~RaiseWindowGuard() { active = false; }
        RaiseWindowGuard(const RaiseWindowGuard &) = delete;
        RaiseWindowGuard &operator=(const RaiseWindowGuard &) = delete;
    private:
        bool &active;
    };
    bool m_raiseWindowActive = false;
    bool m_raiseWindowConcealed = false;
    bool m_raiseWindowAwaitingDocumentRendering = false;
    bool m_raiseWindowDocumentRenderingSettled = false;
    QQueue<QString> m_pendingRaiseWindowRequests;
    QTimer m_raiseWindowRevealTimer;

    QStringList backHistory, forwardHistory;
    bool blockHistory = false;
    // Directory scanning (DirectoryManager/DirectoryScanner) runs on a
    // background thread, so the folder view's model is not ready yet when
    // loadPath() returns - selecting/scrolling to a path synchronously right
    // after it would silently no-op. Instead, the path we want highlighted
    // once the new directory finishes loading is stashed here and applied
    // in onModelLoaded() (mirrors the dirTreeView's m_pendingScrollPath
    // approach in FolderView).
    QString pendingFolderViewSelectPath;
    // modelDelayLoad() displays the already-decoded image before the
    // directory scan has run, so DirectoryModel::updateImage() can't
    // register it in the model's cache yet - containsFile() stays false
    // until the async scan populates fileLookupMap. Stash the need to do
    // so and flush it once the scan actually finishes, in onModelLoaded()
    // (same pattern as pendingFolderViewSelectPath above).
    bool pendingModelImageSync = false;
    ViewMode m_lastViewMode = MODE_DOCUMENT;
    QString m_lastFilePath;
    bool m_resumeFromStandby = false;
    bool lastCMEnabled = false;
    QString lastCMType = "";
    QString lastCMPath = "";
    int lastThumbnailResolution = 0;
    bool lastShowSubfoldersInPanel = false;
    bool lastSquareThumbnails = false;
    bool lastShowHiddenFiles = false;
    int lastPanelPreviewsSize = 0;
    bool lastSortFolders = false;
    SortingMode lastFolderIconSortingMode = SORT_NAME;
    ThumbPanelStyle lastThumbPanelStyle = TH_PANEL_SIMPLE;
};
