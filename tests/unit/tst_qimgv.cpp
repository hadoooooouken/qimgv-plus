#include <QTest>

// Qt Test suite for UI-independent qimgv components. Test cases are added
// together with the components they cover, starting with the view-transform
// model of migration stage S0.4 (docs/QML_MIGRATION_PLAN.md).
class QimgvTests : public QObject {
  Q_OBJECT
};

QTEST_GUILESS_MAIN(QimgvTests)

#include "tst_qimgv.moc"
