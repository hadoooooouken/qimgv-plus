#pragma once

#include <QVBoxLayout>
#include "gui/customwidgets/slidepanel.h"
#include "gui/customwidgets/actionbutton.h"
#include "gui/panels/mainpanel/thumbnailstripproxy.h"

class MainPanel : public SlidePanel {
    Q_OBJECT
public:
    MainPanel(FloatingWidgetContainer *parent);
    ~MainPanel();
    void setPosition(PanelPosition);
    void setExitButtonEnabled(bool mode);
    std::shared_ptr<ThumbnailStripProxy> getThumbnailStrip();
    void setupThumbnailStrip();
    QSize sizeHint() const;

public slots:
    void readSettings();

signals:
    void pinned(bool mode);

private slots:
    void onPinClicked();

private:
    // buttonsWidget must be declared before buttonsLayout: the layout is
    // destroyed first and detaches itself, otherwise ~QWidget would delete
    // the non-heap layout member.
    QWidget buttonsWidget;
    QVBoxLayout buttonsLayout;
    std::shared_ptr<ThumbnailStripProxy> thumbnailStrip;
    ActionButton *settingsButton, *exitButton, *folderViewButton, *pinButton;

protected:
    virtual void paintEvent(QPaintEvent* event);
};
