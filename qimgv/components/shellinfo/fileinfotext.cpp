#include "fileinfotext.h"

#include <QCoreApplication>
#include <QStringList>

#include <numeric>

namespace {
using namespace Qt::StringLiterals;

// Decimal places of the file size.
constexpr int kFileSizePrecision = 1;
// Separator of the info bar details and of the edited marker.
const QString kDetailSeparator = u"  "_s;
const QString kEditedMarker = u"*"_s;
} // namespace

QString filePositionText(const ShellFileInfo &info) {
    if (info.fileCount <= 0)
        return {};
    return u"[ %1/%2 ]"_s.arg(info.index + 1).arg(info.fileCount);
}

QString imageResolutionText(QSize size) {
    if (size.width() <= 0)
        return {};
    QString text = u"%1 x %2"_s.arg(size.width()).arg(size.height());
    if (size.height() > 0) {
        const int divisor = std::gcd(size.width(), size.height());
        text += u" (%1:%2)"_s.arg(size.width() / divisor).arg(size.height() / divisor);
    }
    return text;
}

QString fileSizeText(qint64 bytes, const QLocale &locale) {
    if (bytes <= 0)
        return {};
    return locale.formattedDataSize(bytes, kFileSizePrecision);
}

FullscreenInfoText fullscreenInfoFor(const ShellFileInfo &info, ViewMode viewMode,
                                     const QLocale &locale) {
    if (viewMode == MODE_FOLDERVIEW || info.fileName.isEmpty())
        return {.position = {},
                .name = QCoreApplication::translate("MW", "No file opened."),
                .details = {}};

    QStringList details;
    for (const QString &detail : {imageResolutionText(info.imageSize), info.colorProfile,
                                  info.format.toUpper(), fileSizeText(info.fileSize, locale)}) {
        if (!detail.isEmpty())
            details << detail;
    }
    return {.position = filePositionText(info),
            .name = info.edited ? info.fileName + kDetailSeparator + kEditedMarker
                                : info.fileName,
            .details = details.join(kDetailSeparator)};
}
