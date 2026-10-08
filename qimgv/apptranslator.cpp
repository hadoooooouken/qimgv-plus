#include "apptranslator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QLocale>

#include "settings.h"

namespace {
const QString kSystemLocaleName = QStringLiteral("system");
const QString kSourceLocaleName = QStringLiteral("en_US");
const QString kTranslationsDirectoryName = QStringLiteral("translations");
} // namespace

AppTranslator::AppTranslator() {
    const QString trPathFallback =
        QCoreApplication::applicationDirPath() + "/" + kTranslationsDirectoryName;
#ifdef TRANSLATIONS_PATH
    const QString trPath = QString(TRANSLATIONS_PATH);
#else
    const QString trPath = trPathFallback;
#endif
    QString localeName = settings->language();
    if (localeName == kSystemLocaleName)
        localeName = QLocale::system().name();
    // The source strings are English; no translator is needed for them.
    if (localeName.isEmpty() || localeName == kSourceLocaleName)
        return;

    const QString trFile = trPath + "/" + localeName;
    const QString trFileFallback = trPathFallback + "/" + localeName;
    if (!translator.load(trFile)) {
        qWarning() << "Could not load translation file: " << trFile;
        if (!translator.load(trFileFallback)) {
            qWarning() << "Could not load translation file: " << trFileFallback;
            return;
        }
    }
    installed = QCoreApplication::installTranslator(&translator);
    if (!installed)
        qWarning() << "Could not install translation for locale" << localeName;
}

AppTranslator::~AppTranslator() {
    if (installed)
        QCoreApplication::removeTranslator(&translator);
}
