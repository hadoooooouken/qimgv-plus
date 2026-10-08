#include "viewmodecontroller.h"

ViewModeController::ViewModeController(ViewMode initialMode, QObject *parent)
    : QObject(parent),
      mode(initialMode) {
}

ViewMode ViewModeController::currentViewMode() const {
    return mode;
}

void ViewModeController::enableDocumentView() {
    apply(MODE_DOCUMENT);
}

void ViewModeController::enableFolderView() {
    apply(MODE_FOLDERVIEW);
}

void ViewModeController::apply(ViewMode newMode) {
    mode = newMode;
    emit viewModeApplied(mode);
}
