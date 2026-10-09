#pragma once

#include <QList>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// State and intents of the Qt Quick main window (Main.qml) that do not belong
// to the image viewport: which page is shown, whether the window is in
// fullscreen, and files dropped onto the window. Main.qml binds the
// properties and forwards drops; the Quick UI host decides what they mean.
//
// Owned by the Quick UI host and handed to QML as a required property.
// GUI thread only.
class MainWindowShell final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the Quick UI host")
    Q_PROPERTY(bool folderViewActive READ folderViewActive NOTIFY folderViewActiveChanged FINAL)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged FINAL)

public:
    using QObject::QObject;

    [[nodiscard]] bool folderViewActive() const;
    void setFolderViewActive(bool active);
    [[nodiscard]] bool fullscreen() const;
    void setFullscreen(bool fullscreen);

    // Files dropped onto the window. source is the object that started the
    // drag (nullptr for drags from other applications).
    Q_INVOKABLE void dropUrls(const QList<QUrl> &urls, QObject *source);

signals:
    void folderViewActiveChanged();
    void fullscreenChanged();
    // Never emitted with an empty list.
    void urlsDropped(const QList<QUrl> &urls, QObject *source);

private:
    bool mFolderViewActive = false;
    bool mFullscreen = false;
};
