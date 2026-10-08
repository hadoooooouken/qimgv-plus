#pragma once

#include <QTranslator>

// Installs the translation selected in settings for the lifetime of one
// application run and removes it again on destruction. Must be constructed
// before any user interface, so that strings translated during UI
// construction already use it.
class AppTranslator {
public:
    AppTranslator();
    ~AppTranslator();

    AppTranslator(const AppTranslator &) = delete;
    AppTranslator &operator=(const AppTranslator &) = delete;
    AppTranslator(AppTranslator &&) = delete;
    AppTranslator &operator=(AppTranslator &&) = delete;

private:
    QTranslator translator;
    bool installed = false;
};
