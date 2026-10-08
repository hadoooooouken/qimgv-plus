#pragma once

#include <QObject>
#include <QTimer>

class IViewerPort;
class IViewModePort;
class IWindowPort;
class UiEvents;

// Keeps the main window concealed on first show until the initial view
// (folder view thumbnails or the first document) has been laid out, then
// reveals it. Talks to the UI only through ports; the referenced ports must
// outlive this controller.
class ColdStartWindowController final : public QObject {
    Q_OBJECT
public:
    ColdStartWindowController(IWindowPort &window, IViewModePort &viewMode,
                              IViewerPort &viewer, UiEvents &events);

    void show();
    void onDirectoryModelLoaded();

private slots:
    void onVisibleThumbnailsReady();
    void onFilesystemViewReady();
    void onDocumentRenderingSettled();
    void revealWindow();

private:
    enum class State {
        Initial,
        WaitingForFolderView,
        WaitingForDocumentLayout,
        RevealScheduled,
        Shown
    };

    static constexpr int kFolderViewReadinessTimeoutMs = 2000;
    static constexpr int kLayoutSettleDelayMs = 0;
    static constexpr int kDocumentReadyFallbackMs = 1000;

    void waitForFolderView();
    void waitForDocumentLayout();
    void tryRevealFolderView();

    IWindowPort &window;
    IViewModePort &viewMode;
    IViewerPort &viewer;
    QTimer maximumWaitTimer;
    QTimer documentReadyFallbackTimer;
    State state = State::Initial;
    bool directoryModelLoaded = false;
    bool visibleThumbnailsReady = false;
    bool filesystemViewReady = false;
};
