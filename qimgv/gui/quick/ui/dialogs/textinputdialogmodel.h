#pragma once

#include <QString>
#include <QtQml/qqmlregistration.h>

#include "dialogsession.h"
#include "gui/ports/dialogport.h"

// Single line text prompt (TextInputDialog.qml), the counterpart of
// QInputDialog::getText(): OK and Enter accept the text as entered (also an
// empty one, the caller validates it), Cancel, Escape and closing the window
// reject it. Abandoned requests are not accepted.
//
// Owned by DialogCoordinator. GUI thread only.
class TextInputDialogModel final : public DialogSession {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(QString title READ title NOTIFY requestChanged FINAL)
    Q_PROPERTY(QString label READ label NOTIFY requestChanged FINAL)
    Q_PROPERTY(QString initialText READ initialText NOTIFY requestChanged FINAL)

public:
    explicit TextInputDialogModel(QObject *parent = nullptr);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString label() const;
    [[nodiscard]] QString initialText() const;
    [[nodiscard]] TextInputResult result() const;

    // Opens the dialog for request; false while another request is open.
    [[nodiscard]] bool start(const TextInputRequest &request);

    Q_INVOKABLE void accept(const QString &text);
    Q_INVOKABLE void reject();

signals:
    void requestChanged();

private:
    TextInputRequest mRequest;
    TextInputResult mResult;
};
