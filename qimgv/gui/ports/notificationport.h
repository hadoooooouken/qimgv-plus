#pragma once

#include <QMetaType>
#include <QString>
#include <optional>

// Visual category of a transient notification. The UI picks the icon and,
// unless NotificationRequest::durationMs is set, the display time from it.
enum class NotificationKind {
    Info,
    Success,
    Warning,
    Error,
    AiUpscale,
    // Name of the directory that was just entered.
    Directory,
    // First/last item of the directory reached; the UI supplies the text.
    DirectoryStart,
    DirectoryEnd
};

struct NotificationRequest {
    QString text;
    NotificationKind kind = NotificationKind::Info;
    // Display time in milliseconds; std::nullopt selects the UI default for
    // the kind.
    std::optional<int> durationMs;
};

Q_DECLARE_METATYPE(NotificationRequest)

// Outbound UI port for transient, non-blocking notifications. Implemented by
// each user interface; must only be called on the GUI thread.
class INotificationPort {
public:
    virtual ~INotificationPort() = default;

    virtual void showNotification(const NotificationRequest &request) = 0;
    // Hides every notification that is currently visible.
    virtual void hideNotifications() = 0;

    void showMessage(const QString &text) {
        showNotification({text, NotificationKind::Info, std::nullopt});
    }
    void showMessage(const QString &text, int durationMs) {
        showNotification({text, NotificationKind::Info, durationMs});
    }
    void showSuccess(const QString &text) {
        showNotification({text, NotificationKind::Success, std::nullopt});
    }
    void showWarning(const QString &text) {
        showNotification({text, NotificationKind::Warning, std::nullopt});
    }
    void showError(const QString &text) {
        showNotification({text, NotificationKind::Error, std::nullopt});
    }
    void showAiUpscale(const QString &text) {
        showNotification({text, NotificationKind::AiUpscale, std::nullopt});
    }
    void showAiUpscale(const QString &text, int durationMs) {
        showNotification({text, NotificationKind::AiUpscale, durationMs});
    }
    void showDirectory(const QString &directoryName) {
        showNotification({directoryName, NotificationKind::Directory, std::nullopt});
    }
    void showDirectoryStart() {
        showNotification({QString(), NotificationKind::DirectoryStart, std::nullopt});
    }
    void showDirectoryEnd() {
        showNotification({QString(), NotificationKind::DirectoryEnd, std::nullopt});
    }

protected:
    INotificationPort() = default;
    INotificationPort(const INotificationPort &) = default;
    INotificationPort &operator=(const INotificationPort &) = default;
};
