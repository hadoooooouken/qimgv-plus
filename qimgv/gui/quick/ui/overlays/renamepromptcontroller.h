#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// State and decisions of the rename prompt (RenameOverlay.qml), with the
// rules of the widget overlay: the field starts with the current file name,
// its base name (up to the last dot) selected; an empty name is not
// accepted; cancelling restores the name. Shortcuts of the pass-through
// actions (exit, rename) reach the action system while the field has focus,
// every other key stays in the field.
//
// Owned by OverlayCoordinator, which opens and closes the prompt. GUI thread
// only.
class RenamePromptController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")
    Q_PROPERTY(QString name READ name NOTIFY nameChanged FINAL)
    Q_PROPERTY(int selectionEnd READ selectionEnd NOTIFY nameChanged FINAL)
    Q_PROPERTY(bool backdrop READ backdrop NOTIFY backdropChanged FINAL)

public:
    explicit RenamePromptController(QObject *parent = nullptr);

    // The current file name; the field shows it when the prompt opens.
    [[nodiscard]] QString name() const;
    // End of the selected base name in name().
    [[nodiscard]] int selectionEnd() const;
    // The prompt dims everything behind it (folder view).
    [[nodiscard]] bool backdrop() const;

    void setName(const QString &name);
    void setBackdrop(bool backdrop);
    // Shortcut texts that pass through to the action system.
    void setPassThroughShortcuts(const QStringList &shortcuts);

    // Requests renaming to text; returns false (and keeps the prompt) when
    // text is empty.
    Q_INVOKABLE bool accept(const QString &text);
    // Closes the prompt without renaming.
    Q_INVOKABLE void cancel();
    // True when shortcut (ActionBridge::shortcutText()) belongs to a
    // pass-through action.
    Q_INVOKABLE bool passesThrough(const QString &shortcut) const;

signals:
    void nameChanged();
    void backdropChanged();
    void renameRequested(const QString &newName);
    // Accepting or cancelling closes the prompt.
    void closeRequested();

private:
    QString mName;
    bool mBackdrop = false;
    QStringList mPassThroughShortcuts;
};
