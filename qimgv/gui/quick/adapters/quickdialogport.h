#pragma once

#include "gui/ports/dialogport.h"
#include "gui/quick/adapters/interimports.h"

class DialogCoordinator;
class Settings;

// Dialog port of the Qt Quick UI. Gathers what each dialog starts from (the
// writable image formats, the primary screen size, the Upscayl models and
// preferences), opens the dialog through the DialogCoordinator and waits for
// the answer in a nested event loop (runModalDialog()), so Core keeps its
// blocking calls as with QDialog::exec(). Stores the Upscayl preferences of
// an accepted resize. The batch converter and printing are declined until
// their Quick dialogs exist (S3.4).
//
// The coordinator and settings must outlive the port. GUI thread only.
class QuickDialogPort final : public IDialogPort {
public:
    QuickDialogPort(DialogCoordinator &dialogs, Settings &settings);

    ConfirmationResult confirm(const ConfirmationRequest &request) override;
    FileReplaceDecision resolveFileReplace(const FileReplaceRequest &request) override;
    SavePathResult requestSavePath(const SavePathRequest &request) override;
    std::optional<ResizeRequest> requestResize(QSize initialSize) override;
    TextInputResult requestText(const TextInputRequest &request) override;
    BatchConversionResult runBatchConverter(const BatchConversionRequest &request) override;
    void print(const PrintRequest &request) override;

private:
    DialogCoordinator &mDialogs;
    Settings &mSettings;
    DecliningDialogPort mNotYetAvailable;
};
