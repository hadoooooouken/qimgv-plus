#pragma once

#include <QString>
#include <QStringList>

#include "editorsession.h"
#include "settingsvalues.h"

class IShortcutScriptStore;

// The shortcut creator of the settings dialog (ShortcutCreatorDialog): binds
// a shortcut to an action or to a script ("s:<name>"). The shortcut comes
// from the key, mouse button or wheel input the dialog captures; a shortcut
// the application already binds shows a warning (it is replaced when the
// dialog is accepted). It can be accepted once a shortcut was entered.
//
// GUI thread only.
class ShortcutEditorModel final : public EditorSession {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY requestChanged FINAL)
    Q_PROPERTY(QStringList actions READ actions NOTIFY requestChanged FINAL)
    Q_PROPERTY(QStringList scripts READ scripts NOTIFY requestChanged FINAL)
    Q_PROPERTY(bool scriptSelected READ isScriptSelected WRITE setScriptSelected NOTIFY selectionChanged FINAL)
    Q_PROPERTY(int actionIndex READ actionIndex WRITE setActionIndex NOTIFY selectionChanged FINAL)
    Q_PROPERTY(int scriptIndex READ scriptIndex WRITE setScriptIndex NOTIFY selectionChanged FINAL)
    Q_PROPERTY(QString shortcut READ shortcut NOTIFY shortcutChanged FINAL)
    // The shortcut, or a prompt while none was entered.
    Q_PROPERTY(QString shortcutText READ shortcutText NOTIFY shortcutChanged FINAL)
    Q_PROPERTY(QString warning READ warning NOTIFY shortcutChanged FINAL)
    Q_PROPERTY(bool canAccept READ canAccept NOTIFY shortcutChanged FINAL)

public:
    // store must outlive the model.
    explicit ShortcutEditorModel(IShortcutScriptStore &store, QObject *parent = nullptr);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QStringList actions() const;
    [[nodiscard]] QStringList scripts() const;
    [[nodiscard]] bool isScriptSelected() const;
    void setScriptSelected(bool selected);
    [[nodiscard]] int actionIndex() const;
    void setActionIndex(int index);
    [[nodiscard]] int scriptIndex() const;
    void setScriptIndex(int index);
    [[nodiscard]] QString shortcut() const;
    [[nodiscard]] QString shortcutText() const;
    [[nodiscard]] QString warning() const;
    [[nodiscard]] bool canAccept() const;

    // Opens the creator for a new shortcut.
    void startAdd();
    // Opens the editor for entry, the table row at row.
    void startEdit(const ShortcutEntry &entry, int row);

    // A captured shortcut; empty ones (input without a shortcut) are ignored.
    Q_INVOKABLE void setShortcut(const QString &shortcut);

    // The binding the dialog was accepted with.
    [[nodiscard]] ShortcutEntry result() const;
    // The edited table row, -1 for a new shortcut.
    [[nodiscard]] int editedRow() const;

signals:
    void requestChanged();
    void selectionChanged();
    void shortcutChanged();

protected:
    [[nodiscard]] bool acceptable() const override;

private:
    void startRequest(const QString &title, int row);

    IShortcutScriptStore &mStore;
    QString mTitle;
    QStringList mActions;
    QStringList mScripts;
    bool mScriptSelected = false;
    int mActionIndex = 0;
    int mScriptIndex = 0;
    QString mShortcut;
    QString mWarning;
    int mEditedRow = -1;
};
