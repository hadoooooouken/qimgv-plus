#pragma once

#include <QObject>

#include "gui/ports/viewmodeport.h"

// Owns the document/folder view mode. Core and the cold-start controller
// query and change it through IViewModePort; the active user interface
// follows viewModeApplied() to switch its views.
class ViewModeController final : public QObject, public IViewModePort {
    Q_OBJECT
public:
    explicit ViewModeController(ViewMode initialMode, QObject *parent = nullptr);

    [[nodiscard]] ViewMode currentViewMode() const override;
    void enableDocumentView() override;
    void enableFolderView() override;

signals:
    // Emitted for every request, including when the mode is already active,
    // so the UI can refresh the state that belongs to the mode (overlays,
    // info bar). currentViewMode() already returns `mode` when it fires.
    void viewModeApplied(ViewMode mode);

private:
    void apply(ViewMode mode);

    ViewMode mode;
};
