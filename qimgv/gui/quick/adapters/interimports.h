#pragma once

#include "gui/ports/dialogport.h"

// Ports of the Qt Quick UI whose user interface does not exist yet. They keep
// Core fully functional and safe until the real implementations replace them:
// QuickDialogPort forwards the batch converter and printing here until their
// Quick dialogs exist (S3.4), so nothing is converted or printed without the
// user's choices.
// GUI thread only.

// Declines every request (confirm: not accepted, file replace: cancel, save
// path / resize / text input: cancelled, batch conversion: not started,
// print: not printed) and logs a warning naming the dialog.
class DecliningDialogPort final : public IDialogPort {
public:
    ConfirmationResult confirm(const ConfirmationRequest &request) override;
    FileReplaceDecision resolveFileReplace(const FileReplaceRequest &request) override;
    SavePathResult requestSavePath(const SavePathRequest &request) override;
    std::optional<ResizeRequest> requestResize(QSize initialSize) override;
    TextInputResult requestText(const TextInputRequest &request) override;
    BatchConversionResult runBatchConverter(const BatchConversionRequest &request) override;
    void print(const PrintRequest &request) override;
};
