#include "windowtitle.h"

#include <QCoreApplication>
#include <QStringList>

#include <numeric>

namespace {
using namespace Qt::StringLiterals;

// Decimal places of the file size.
constexpr int kFileSizePrecision = 1;

QString positionText(const ShellFileInfo &info) {
    if (info.fileCount <= 0)
        return {};
    return u"[ %1/%2 ]"_s.arg(info.index + 1).arg(info.fileCount);
}

QString resolutionText(QSize size) {
    if (size.width() <= 0)
        return {};
    QString text = u"%1 x %2"_s.arg(size.width()).arg(size.height());
    if (size.height() > 0) {
        const int divisor = std::gcd(size.width(), size.height());
        text += u" (%1:%2)"_s.arg(size.width() / divisor).arg(size.height() / divisor);
    }
    return text;
}

QString statesText(const WindowTitleState &state) {
    QString states;
    if (state.info.slideshow)
        states += u" [slideshow]"_s;
    if (state.info.shuffle)
        states += u" [shuffle]"_s;
    if (state.zoomLocked)
        states += u" [zoom lock]"_s;
    if (state.viewLocked)
        states += u" [view lock]"_s;
    return states;
}
} // namespace

QString windowTitleFor(const WindowTitleState &state, const QLocale &locale) {
    if (state.viewMode == MODE_FOLDERVIEW)
        return QCoreApplication::translate("MW", "Folder view");
    const ShellFileInfo &info = state.info;
    if (info.fileName.isEmpty())
        return QCoreApplication::applicationName();

    QString title = info.fileName + u" [%1%]"_s.arg(state.scalePercent);

    if (state.extendedInfo) {
        title.prepend(positionText(info) + u"  "_s);
        QStringList details;
        details << resolutionText(info.imageSize) << info.colorProfile
                << info.format.toUpper();
        if (info.fileSize > 0)
            details << locale.formattedDataSize(info.fileSize, kFileSizePrecision);
        for (const QString &detail : std::as_const(details)) {
            if (!detail.isEmpty())
                title += u" - "_s + detail;
        }
    }

    const QString states = statesText(state);
    if (!states.isEmpty())
        title += u" -"_s + states;
    if (info.edited)
        title.prepend(u"* "_s);
    return title;
}
