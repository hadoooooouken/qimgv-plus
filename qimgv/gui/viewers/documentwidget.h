#pragma once

#include <memory>
#include <QBoxLayout>
#include "gui/customwidgets/floatingwidgetcontainer.h"
#include "gui/viewers/viewerwidget.h"
#include "gui/panels/mainpanel/mainpanel.h"

class DocumentWidget : public FloatingWidgetContainer {
public:
    DocumentWidget(std::shared_ptr<ViewerWidget> viewWidget, QWidget* parent = nullptr);
    std::shared_ptr<ViewerWidget> viewWidget();
    std::shared_ptr<ThumbnailStripProxy> thumbPanel();
    void setFocus();
    void hideFloatingPanel();
    void hideFloatingPanel(bool animated);
    void setPanelEnabled(bool mode);
    bool panelEnabled();
    void setupMainPanel();
    void setInteractionEnabled(bool mode);
    void allowPanelInit();

public slots:
    void onFullscreenModeChanged(bool mode);

private slots:
    void setPanelPinned(bool mode);
    bool panelPinned();
    void readSettings();
    void hideFloatingPanelDelayed();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class PanelHideSource {
        None,
        PointerExit,
        WindowExit
    };

    bool isSizeAllowed() const;
    void updatePanelVisibility();
    void handlePointerMove(const QPoint &position);
    void scheduleFloatingPanelHide(PanelHideSource source);
    void cancelFloatingPanelHide();

    QBoxLayout *layout;
    std::shared_ptr<ViewerWidget> mViewWidget;
    std::shared_ptr<MainPanel> mainPanel;
    bool avoidPanelFlag, mPanelEnabled, mPanelFullscreenOnly, mIsFullscreen, mPanelPinned, mInteractionEnabled, mAllowPanelInit;
    QTimer hideTimer;
    PanelHideSource mHideTimerSource = PanelHideSource::None;
};
