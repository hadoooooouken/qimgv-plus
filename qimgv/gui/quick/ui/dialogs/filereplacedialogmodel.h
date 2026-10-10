#pragma once

#include <QString>
#include <QtQml/qqmlregistration.h>

#include "dialogsession.h"
#include "gui/ports/dialogport.h"

// Name collision question of a copy or move (FileReplaceDialog.qml), the
// rules of the widget FileReplaceDialog: Yes replaces (or merges), No skips
// the item, both remember "Apply to all", which is offered only when more
// collisions may follow; Cancel stops the whole operation. Escape and
// closing the window answer No for this item only, as the widget dialog's
// reject did. Abandoned requests cancel the operation.
//
// Owned by DialogCoordinator. GUI thread only.
class FileReplaceDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(Collision collision READ collision NOTIFY requestChanged FINAL)
    Q_PROPERTY(QString source READ source NOTIFY requestChanged FINAL)
    Q_PROPERTY(QString destination READ destination NOTIFY requestChanged FINAL)
    Q_PROPERTY(bool multiple READ isMultiple NOTIFY requestChanged FINAL)

public:
    // What exists at the destination; selects the title and the question.
    enum class Collision {
        FileOverFile,
        DirectoryOverDirectory,
        FileOverDirectory,
        DirectoryOverFile,
    };
    Q_ENUM(Collision)

    explicit FileReplaceDialogModel(QObject *parent = nullptr);

    [[nodiscard]] Collision collision() const;
    [[nodiscard]] QString source() const;
    [[nodiscard]] QString destination() const;
    [[nodiscard]] bool isMultiple() const;
    [[nodiscard]] FileReplaceDecision result() const;

    // Opens the dialog for request; false while another request is open.
    [[nodiscard]] bool start(const FileReplaceRequest &request);

    // Yes (replace true) or No; applyToAll counts only for multiple requests.
    Q_INVOKABLE void answer(bool replace, bool applyToAll);
    Q_INVOKABLE void cancel();
    // Escape or the window's close button.
    Q_INVOKABLE void dismiss();

signals:
    void requestChanged();

private:
    void finish(FileReplaceDecision decision);

    FileReplaceRequest mRequest;
    FileReplaceDecision mResult;
};
