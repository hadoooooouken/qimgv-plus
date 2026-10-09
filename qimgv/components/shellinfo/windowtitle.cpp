#include "windowtitle.h"

#include <QCoreApplication>
#include <QStringList>

#include "components/shellinfo/fileinfotext.h"

namespace {
using namespace Qt::StringLiterals;

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
        title.prepend(filePositionText(info) + u"  "_s);
        const QStringList details{imageResolutionText(info.imageSize), info.colorProfile,
                                  info.format.toUpper(), fileSizeText(info.fileSize, locale)};
        for (const QString &detail : details) {
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
