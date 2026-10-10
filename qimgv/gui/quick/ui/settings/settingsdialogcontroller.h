#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include "settingseditortypes.h"

// The settings window of the Qt Quick UI (SettingsDialog.qml) over the
// shared SettingsEditorModel. show() reloads the editor and opens the window
// on a page; OK applies and closes, Apply applies, Cancel and the window's
// close button close without applying (the previewed theme values stay, as
// in the widget dialog). `created` turns true with the first show() and
// stays true, so nothing of the window exists at startup.
//
// GUI thread only.
class SettingsDialogController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged FINAL)
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged FINAL)
    // A SettingsEditorModel::Page value.
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY pageChanged FINAL)
    Q_PROPERTY(int pageCount READ pageCount CONSTANT FINAL)
    Q_PROPERTY(SettingsEditorModel *editor READ editor CONSTANT FINAL)

public:
    // editor must outlive the controller.
    explicit SettingsDialogController(SettingsEditorModel &editor, QObject *parent = nullptr);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isCreated() const;
    [[nodiscard]] int page() const;
    void setPage(int page);
    [[nodiscard]] static int pageCount();
    [[nodiscard]] SettingsEditorModel *editor();

    // Opens the window on page, with the values reloaded; an open window
    // only switches to page.
    void show(SettingsEditorModel::Page page);

    Q_INVOKABLE void accept();
    Q_INVOKABLE void applyChanges();
    Q_INVOKABLE void dismiss();

signals:
    void openChanged();
    void createdChanged();
    void pageChanged();

private:
    void close();

    SettingsEditorModel &mEditor;
    bool mOpen = false;
    bool mCreated = false;
    int mPage = 0;
};
