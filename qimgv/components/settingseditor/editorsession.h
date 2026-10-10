#pragma once

#include <QObject>

// Open state of a sub-dialog of the settings dialog (the shortcut creator,
// the script editor). A request opens it (`open`); it closes with the
// person's answer: accepted() or rejected() is emitted after `open` turned
// false. `created` turns true with the first request and stays true, so QML
// creates the dialog window on first use. A request while open restarts it.
//
// GUI thread only.
class EditorSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged FINAL)
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged FINAL)

public:
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isCreated() const;

    // Closes the dialog with the person's answer. accept() does nothing while
    // the request cannot be accepted (canAccept() of the derived class).
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();

signals:
    void openChanged();
    void createdChanged();
    void accepted();
    void rejected();

protected:
    explicit EditorSession(QObject *parent = nullptr);

    // Opens the dialog for the request the derived class published before.
    void open();
    [[nodiscard]] virtual bool acceptable() const = 0;

private:
    void close();

    bool mOpen = false;
    bool mCreated = false;
};
