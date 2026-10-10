#pragma once

#include <QString>
#include <QtQml/qqmlregistration.h>

#include "dialogsession.h"
#include "gui/ports/dialogport.h"

// Yes / No question (ConfirmationDialog.qml), the rules of the widget UI's
// QMessageBox: Yes is the default answer, Escape and closing the window
// answer No. Abandoned requests are not accepted.
//
// Owned by DialogCoordinator. GUI thread only.
class ConfirmationDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(QString title READ title NOTIFY requestChanged FINAL)
    Q_PROPERTY(QString message READ message NOTIFY requestChanged FINAL)

public:
    explicit ConfirmationDialogModel(QObject *parent = nullptr);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString message() const;
    [[nodiscard]] ConfirmationResult result() const;

    // Opens the dialog for request; false while another request is open.
    [[nodiscard]] bool start(const ConfirmationRequest &request);

    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();

signals:
    void requestChanged();

private:
    void finish(bool accepted);

    ConfirmationRequest mRequest;
    ConfirmationResult mResult;
};
