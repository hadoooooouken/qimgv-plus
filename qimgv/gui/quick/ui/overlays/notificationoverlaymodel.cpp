#include "notificationoverlaymodel.h"

#include <QCoreApplication>

namespace {
// Display times of the widget UI (mainwindow.cpp).
constexpr int kDefaultMessageDurationMs = 1500;
constexpr int kErrorMessageDurationMs = 2800;
constexpr int kDirectoryMessageDurationMs = 1700;
constexpr int kDirectoryEdgeMessageDurationMs = 600;

struct KindPresentation {
    FluentIcon icon;
    int durationMs;
};

KindPresentation presentationOf(NotificationKind kind) {
    switch (kind) {
    case NotificationKind::Info:
        break;
    case NotificationKind::Success:
        return {FluentIcon::CheckmarkCircle20, kDefaultMessageDurationMs};
    case NotificationKind::Warning:
        return {FluentIcon::Warning20, kDefaultMessageDurationMs};
    case NotificationKind::Error:
        return {FluentIcon::ErrorCircle20, kErrorMessageDurationMs};
    case NotificationKind::AiUpscale:
        return {FluentIcon::AiUpscale20, kDefaultMessageDurationMs};
    case NotificationKind::Directory:
        return {FluentIcon::Folder20, kDirectoryMessageDurationMs};
    case NotificationKind::DirectoryStart:
        return {FluentIcon::ArrowPrevious20, kDirectoryEdgeMessageDurationMs};
    case NotificationKind::DirectoryEnd:
        return {FluentIcon::ArrowNext20, kDirectoryEdgeMessageDurationMs};
    }
    return {FluentIcon::Info20, kDefaultMessageDurationMs};
}

QString defaultTextOf(NotificationKind kind) {
    if (kind == NotificationKind::DirectoryStart)
        return QCoreApplication::translate("MW", "Start of directory");
    if (kind == NotificationKind::DirectoryEnd)
        return QCoreApplication::translate("MW", "End of directory");
    return {};
}
} // namespace

NotificationPresentation notificationPresentationFor(const NotificationRequest &request) {
    const KindPresentation kind = presentationOf(request.kind);
    return {.icon = kind.icon,
            .text = request.text.isEmpty() ? defaultTextOf(request.kind) : request.text,
            .durationMs = request.durationMs.value_or(kind.durationMs)};
}

//------------------------------------------------------------------------------
NotificationOverlayModel::NotificationOverlayModel(QObject *parent) : QObject(parent) {
    mHideTimer.setSingleShot(true);
    connect(&mHideTimer, &QTimer::timeout, this, [this]() { setVisible(false); });
}

bool NotificationOverlayModel::isVisible() const {
    return mVisible;
}

bool NotificationOverlayModel::isCreated() const {
    return mCreated;
}

QString NotificationOverlayModel::text() const {
    return mPresentation.text;
}

FluentIcon NotificationOverlayModel::icon() const {
    return mPresentation.icon;
}

void NotificationOverlayModel::showNotification(const NotificationRequest &request) {
    const NotificationPresentation presentation = notificationPresentationFor(request);
    if (presentation != mPresentation) {
        mPresentation = presentation;
        emit messageChanged();
    }
    if (!mCreated) {
        mCreated = true;
        emit createdChanged();
    }
    setVisible(true);
    mHideTimer.start(mPresentation.durationMs);
}

void NotificationOverlayModel::hideNotifications() {
    mHideTimer.stop();
    setVisible(false);
}

void NotificationOverlayModel::setVisible(bool visible) {
    if (mVisible == visible)
        return;
    mVisible = visible;
    emit visibleChanged();
}
