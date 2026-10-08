#pragma once

#include <memory>

#include "gui/idirectoryview.h"
#include "gui/ports/dialogport.h"
#include "gui/ports/notificationport.h"
#include "gui/ports/shellport.h"
#include "gui/ports/uievents.h"
#include "gui/ports/viewerport.h"
#include "gui/ports/viewmodeport.h"
#include "gui/ports/windowport.h"

// Everything Core needs from a user interface. The referenced objects are
// owned by the UI host and must outlive the Core they are handed to.
struct UiPorts {
    INotificationPort &notifications;
    IDialogPort &dialogs;
    IViewerPort &viewer;
    IShellPort &shell;
    IWindowPort &window;
    IViewModePort &viewMode;
    UiEvents &events;
    std::shared_ptr<IDirectoryView> thumbnailPanelView;
    std::shared_ptr<IDirectoryView> folderView;
};
