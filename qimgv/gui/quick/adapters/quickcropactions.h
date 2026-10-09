#pragma once

#include <QObject>

class ContextMenuModel;
class CropController;
class ImageViewportController;
class Settings;
class ThumbnailPanelController;
class UiEvents;

// Everything the crop mode works with. All referenced objects must outlive
// QuickCropActions.
struct QuickCropContext {
    CropController &crop;
    ImageViewportController &viewport;
    ThumbnailPanelController &thumbnailPanel;
    ContextMenuModel &contextMenu;
    UiEvents &events;
    Settings &settings;
};

// Connects the Qt Quick crop mode (CropController) to the application, as
// MW::showCropPanel() / hideCropPanel() do for the widget UI:
// - opening enlarges small images in fit-to-window mode, fits the image to
//   the window and turns the viewer input, the context menu and the
//   floating thumbnail panel off; closing turns them on again (the image
//   keeps its scale until the next fit, as in the widget UI);
// - keeps the image size, the image area on screen and the device pixel
//   ratio of the controller current;
// - sends the crop requests to Core (UiEvents) and stores the chosen
//   default action in Settings.
// GUI thread only.
class QuickCropActions final : public QObject {
    Q_OBJECT
public:
    explicit QuickCropActions(const QuickCropContext &context, QObject *parent = nullptr);

private:
    void onActiveChanged();
    void syncImage();
    void syncGeometry();

    CropController &crop;
    ImageViewportController &viewport;
    ThumbnailPanelController &thumbnailPanel;
    ContextMenuModel &contextMenu;
};
