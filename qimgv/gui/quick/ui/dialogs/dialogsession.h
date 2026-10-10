#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Lifetime of one modal dialog request of the Qt Quick UI, the base of the
// dialog view-models. A request opens the dialog (`open`), and the dialog
// finishes either with the person's answer or abandoned (the application is
// quitting); `finished` is emitted in both cases, after `open` turned false.
// `created` turns true with the first request and stays true, so QML
// creates each dialog window on first use (Loader) and nothing exists at
// startup.
//
// One request at a time: a request while the dialog is open is refused.
// Each derived view-model resets its result to the declined answer when a
// request starts, so an abandoned dialog reports that answer. GUI thread
// only.
class DialogSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by DialogCoordinator")
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged FINAL)
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged FINAL)

public:
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isCreated() const;
    // True when the last request finished with the person's answer.
    [[nodiscard]] bool wasAnswered() const;

    // Closes an open dialog without an answer, keeping the declined result.
    void abandon();

signals:
    void openChanged();
    void createdChanged();
    void finished();

protected:
    explicit DialogSession(QObject *parent = nullptr);

    // True when a new request may start; false (logging a warning naming
    // dialogName) while a request is still open.
    [[nodiscard]] bool canStart(const char *dialogName) const;
    // Opens the dialog for the new request, which the derived view-model
    // published before.
    void open();
    // Closes the dialog with the person's answer, which the derived
    // view-model stored before.
    void conclude();

private:
    void close(bool answered);

    bool mOpen = false;
    bool mCreated = false;
    bool mAnswered = false;
};

// Shows dialog's open request and waits for its answer in a nested event
// loop, as QDialog::exec() does. Abandons the dialog when the loop ends for
// another reason (QCoreApplication::exit() ends every running loop). Returns
// wasAnswered(); false without waiting when dialog is not open. GUI thread
// only.
bool runModalDialog(DialogSession &dialog);
