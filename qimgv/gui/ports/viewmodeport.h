#pragma once

#include "settings_types.h"

// Port for the document/folder view mode. The state is owned by
// ViewModeController; user interfaces follow it.
class IViewModePort {
public:
    virtual ~IViewModePort() = default;

    [[nodiscard]] virtual ViewMode currentViewMode() const = 0;
    virtual void enableDocumentView() = 0;
    virtual void enableFolderView() = 0;

protected:
    IViewModePort() = default;
    IViewModePort(const IViewModePort &) = default;
    IViewModePort &operator=(const IViewModePort &) = default;
};
