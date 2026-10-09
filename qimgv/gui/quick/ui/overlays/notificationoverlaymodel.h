#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "gui/ports/notificationport.h"
#include "utils/fluenticon.h"

// How a notification is shown: icon, text and display time.
struct NotificationPresentation {
    FluentIcon icon = FluentIcon::Info20;
    QString text;
    int durationMs = 0;

    friend bool operator==(const NotificationPresentation &,
                           const NotificationPresentation &) = default;
};

// Presentation of request with the defaults of the widget UI
// (MW::showNotification): icon and display time by kind, the default texts
// of the directory edge messages ("MW" translation context).
[[nodiscard]] NotificationPresentation notificationPresentationFor(
    const NotificationRequest &request);

// The floating message of the Qt Quick UI (FloatingMessage.qml) and its
// notification port: the latest notification stays visible for its display
// time, a newer one replaces it and restarts the time. QML fades the message
// in and out.
//
// Owned by OverlayCoordinator. GUI thread only.
class NotificationOverlayModel final : public QObject, public INotificationPort {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by OverlayCoordinator")
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged FINAL)
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged FINAL)
    Q_PROPERTY(QString text READ text NOTIFY messageChanged FINAL)
    Q_PROPERTY(FluentIcons::FluentIcon icon READ icon NOTIFY messageChanged FINAL)

public:
    explicit NotificationOverlayModel(QObject *parent = nullptr);

    [[nodiscard]] bool isVisible() const;
    // True once the first notification arrived (QML creates the message
    // then).
    [[nodiscard]] bool isCreated() const;
    [[nodiscard]] QString text() const;
    [[nodiscard]] FluentIcon icon() const;

    // INotificationPort
    void showNotification(const NotificationRequest &request) override;
    void hideNotifications() override;

signals:
    void visibleChanged();
    void createdChanged();
    void messageChanged();

private:
    void setVisible(bool visible);

    QTimer mHideTimer;
    NotificationPresentation mPresentation;
    bool mVisible = false;
    bool mCreated = false;
};
