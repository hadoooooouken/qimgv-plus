#include <QTest>

#include "testsuites.h"
#include "utils/uimode.h"

class UiModeTests : public QObject {
  Q_OBJECT

private slots:
  void theQuickUiIsTheDefault() { QCOMPARE(defaultUiMode, UiMode::Quick); }

  void namesRoundTrip() {
    for (const UiMode mode : {UiMode::Quick, UiMode::Widgets})
      QCOMPARE(uiModeFromName(QString(uiModeName(mode))), std::optional(mode));
  }

  void namesMatchTheCommandLineValues() {
    QCOMPARE(uiModeName(UiMode::Quick), QLatin1StringView("quick"));
    QCOMPARE(uiModeName(UiMode::Widgets), QLatin1StringView("widgets"));
  }

  void namesAreCaseInsensitive() {
    QCOMPARE(uiModeFromName(u"Widgets"), std::optional(UiMode::Widgets));
    QCOMPARE(uiModeFromName(u"QUICK"), std::optional(UiMode::Quick));
  }

  void unknownNamesAreRejected() {
    QVERIFY(!uiModeFromName(u"qml").has_value());
    QVERIFY(!uiModeFromName(u"").has_value());
    QVERIFY(!uiModeFromName(u" quick").has_value());
  }
};

int runUiModeTests(int argc, char **argv) {
  UiModeTests tests;
  return QTest::qExec(&tests, argc, argv);
}

#include "tst_uimode.moc"
