#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class FolderViewProxy;
class MW;

class ColdStartWindowController final : public QObject {
    Q_OBJECT
public:
    ColdStartWindowController(MW &window, FolderViewProxy &folderView);

    void show();

private slots:
    void onVisibleThumbnailsReady();
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

    static constexpr int kMaximumWaitMs = 30000;
    static constexpr int kLayoutSettleDelayMs = 0;
    static constexpr int kDocumentReadyFallbackMs = 1000;
    static constexpr qreal kHiddenWindowOpacity = 0.0;
    static constexpr qreal kVisibleWindowOpacity = 1.0;

    void waitForFolderView();
    void waitForDocumentLayout();

    QPointer<MW> window;
    QTimer maximumWaitTimer;
    QTimer documentReadyFallbackTimer;
    State state = State::Initial;
};
