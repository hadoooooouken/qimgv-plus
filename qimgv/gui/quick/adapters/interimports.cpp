#include "interimports.h"

#include <QDebug>

namespace {
const char *kindName(NotificationKind kind) {
    switch (kind) {
    case NotificationKind::Info: return "info";
    case NotificationKind::Success: return "success";
    case NotificationKind::Warning: return "warning";
    case NotificationKind::Error: return "error";
    case NotificationKind::AiUpscale: return "ai-upscale";
    case NotificationKind::Directory: return "directory";
    case NotificationKind::DirectoryStart: return "directory-start";
    case NotificationKind::DirectoryEnd: return "directory-end";
    }
    return "unknown";
}

void reportDeclined(const char *dialog, const QString &detail) {
    qWarning().noquote() << "Qt Quick UI: the" << dialog
                         << "dialog is not available yet; the request was declined:"
                         << detail;
}
} // namespace

void LoggingNotificationPort::showNotification(const NotificationRequest &request) {
    qInfo().noquote() << "Qt Quick UI: notification" << kindName(request.kind)
                      << request.text;
}

void LoggingNotificationPort::hideNotifications() {
    // Nothing is displayed, so there is nothing to hide.
}

ConfirmationResult DecliningDialogPort::confirm(const ConfirmationRequest &request) {
    reportDeclined("confirmation", request.title + QStringLiteral(": ") + request.message);
    return {.accepted = false};
}

FileReplaceDecision DecliningDialogPort::resolveFileReplace(const FileReplaceRequest &request) {
    reportDeclined("file replace", request.targetPath);
    return {.yes = false, .all = false, .cancel = true};
}

SavePathResult DecliningDialogPort::requestSavePath(const SavePathRequest &request) {
    reportDeclined("save as", request.suggestedPath);
    return {};
}

std::optional<ResizeRequest> DecliningDialogPort::requestResize(QSize initialSize) {
    reportDeclined("resize", QStringLiteral("%1 x %2").arg(initialSize.width()).arg(initialSize.height()));
    return std::nullopt;
}

TextInputResult DecliningDialogPort::requestText(const TextInputRequest &request) {
    reportDeclined("text input", request.title);
    return {.accepted = false, .text = {}};
}

BatchConversionResult DecliningDialogPort::runBatchConverter(const BatchConversionRequest &request) {
    reportDeclined("batch converter", QString::number(request.filePaths.size()) + QStringLiteral(" files"));
    return {.conversionStarted = false};
}

void DecliningDialogPort::print(const PrintRequest &request) {
    reportDeclined("print", request.pdfOutputPath);
}
