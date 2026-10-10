#pragma once

#include <QString>
#include <QUrl>

#include "editorsession.h"
#include "settingsvalues.h"

class IShortcutScriptStore;

// The script editor of the settings dialog (ScriptEditorDialog): name,
// command line and "Wait to finish" of an "Open with" entry. A name is
// required; a name another script already has is replaced on accept. The
// accept button reads Create, Save (the edited script keeps its name) or
// Replace. Picking an executable sets the command to run it on the file.
//
// GUI thread only.
class ScriptEditorModel final : public EditorSession {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY requestChanged FINAL)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL)
    Q_PROPERTY(QString command READ command WRITE setCommand NOTIFY commandChanged FINAL)
    Q_PROPERTY(bool blocking READ isBlocking WRITE setBlocking NOTIFY blockingChanged FINAL)
    Q_PROPERTY(QString message READ message NOTIFY nameChanged FINAL)
    Q_PROPERTY(QString acceptText READ acceptText NOTIFY nameChanged FINAL)
    Q_PROPERTY(bool canAccept READ canAccept NOTIFY nameChanged FINAL)
    Q_PROPERTY(QString keywordsText READ keywordsText CONSTANT FINAL)
    Q_PROPERTY(QString executableDialogTitle READ executableDialogTitle CONSTANT FINAL)
    Q_PROPERTY(QStringList executableFilters READ executableFilters CONSTANT FINAL)

public:
    // store must outlive the model.
    explicit ScriptEditorModel(IShortcutScriptStore &store, QObject *parent = nullptr);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString name() const;
    void setName(const QString &name);
    [[nodiscard]] QString command() const;
    void setCommand(const QString &command);
    [[nodiscard]] bool isBlocking() const;
    void setBlocking(bool blocking);
    [[nodiscard]] QString message() const;
    [[nodiscard]] QString acceptText() const;
    [[nodiscard]] bool canAccept() const;
    [[nodiscard]] static QString keywordsText();
    [[nodiscard]] static QString executableDialogTitle();
    [[nodiscard]] static QStringList executableFilters();

    // Opens the editor for a new script.
    void startNew();
    // Opens the editor for the script called name.
    void startEdit(const QString &name);

    // Sets the command to run the executable at path on the current file.
    void setExecutablePath(const QString &path);
    // setExecutablePath() for a picked file; non-local URLs are ignored.
    Q_INVOKABLE void setExecutable(const QUrl &url);

    // The name and script the editor was accepted with.
    [[nodiscard]] QString resultName() const;
    [[nodiscard]] ScriptDefinition resultScript() const;

signals:
    void requestChanged();
    void nameChanged();
    void commandChanged();
    void blockingChanged();

protected:
    [[nodiscard]] bool acceptable() const override;

private:
    [[nodiscard]] bool nameTaken() const;

    IShortcutScriptStore &mStore;
    bool mEditing = false;
    QString mEditTarget;
    QString mName;
    QString mCommand;
    bool mBlocking = false;
};
