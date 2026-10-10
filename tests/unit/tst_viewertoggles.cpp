#include <QSignalSpy>
#include <QTest>

#include "components/viewertoggles/viewertoggles.h"
#include "testsuites.h"

namespace {
using namespace Qt::StringLiterals;

class FakeViewerToggleStore final : public IViewerToggleStore {
public:
    bool useUpscayl() override { return upscayl; }
    void setUseUpscayl(bool enabled) override { upscayl = enabled; }
    bool hdrToneMappingEnabled() override { return hdr; }
    void setHdrToneMappingEnabled(bool enabled) override { hdr = enabled; }
    QStringList availableUpscaylModels() override { return models; }
    QString upscaylModel() override { return model; }
    void setUpscaylModel(const QString &value) override { model = value; }
    void publishChanges() override { ++published; }

    bool upscayl = false;
    bool hdr = false;
    QStringList models;
    QString model;
    int published = 0;
};

NotificationRequest notificationAt(const QSignalSpy &spy, qsizetype index) {
    return spy.at(index).constFirst().value<NotificationRequest>();
}
} // namespace

class ViewerTogglesTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NotificationRequest>(); }

    void togglingUpscaylOnPublishesAndConfirms() {
        FakeViewerToggleStore store;
        ViewerToggles toggles(store);
        QSignalSpy messages(&toggles, &ViewerToggles::notificationRequested);
        QSignalSpy hides(&toggles, &ViewerToggles::upscaledCropHideRequested);

        toggles.toggleUpscayl();

        QVERIFY(store.upscayl);
        QCOMPARE(store.published, 1);
        QCOMPARE(messages.size(), 1);
        const NotificationRequest message = notificationAt(messages, 0);
        QCOMPARE(message.text, u"Use Upscayl: ON"_s);
        QCOMPARE(message.kind, NotificationKind::AiUpscale);
        QCOMPARE(message.durationMs, std::optional(ViewerToggles::kToggleNotificationMs));
        QCOMPARE(hides.size(), 0);
    }

    void togglingUpscaylOffHidesTheUpscaledCrop() {
        FakeViewerToggleStore store;
        store.upscayl = true;
        ViewerToggles toggles(store);
        QSignalSpy messages(&toggles, &ViewerToggles::notificationRequested);
        QSignalSpy hides(&toggles, &ViewerToggles::upscaledCropHideRequested);

        toggles.toggleUpscayl();

        QVERIFY(!store.upscayl);
        QCOMPARE(notificationAt(messages, 0).text, u"Use Upscayl: OFF"_s);
        QCOMPARE(hides.size(), 1);
    }

    void togglingHdrToneMappingPublishesAndConfirms() {
        FakeViewerToggleStore store;
        ViewerToggles toggles(store);
        QSignalSpy messages(&toggles, &ViewerToggles::notificationRequested);

        toggles.toggleHdrToneMapping();
        QVERIFY(store.hdr);
        QCOMPARE(notificationAt(messages, 0).text, u"HDR Tone-Mapping: ON"_s);
        QCOMPARE(notificationAt(messages, 0).kind, NotificationKind::Info);

        toggles.toggleHdrToneMapping();
        QVERIFY(!store.hdr);
        QCOMPARE(notificationAt(messages, 1).text, u"HDR Tone-Mapping: OFF"_s);
        QCOMPARE(store.published, 2);
    }

    void cyclingStepsThroughModelsAndPublishesOnceSettled() {
        FakeViewerToggleStore store;
        store.models = {u"a"_s, u"b"_s, u"c"_s};
        store.model = u"b"_s;
        ViewerToggles toggles(store);
        QSignalSpy messages(&toggles, &ViewerToggles::notificationRequested);

        toggles.cycleUpscaylModel();
        toggles.cycleUpscaylModel();

        // Stored at once, wrapping around; announced only after the delay.
        QCOMPARE(store.model, u"a"_s);
        QCOMPARE(store.published, 0);
        QCOMPARE(messages.size(), 2);
        QCOMPARE(notificationAt(messages, 0).text, u"Model: c"_s);
        QCOMPARE(notificationAt(messages, 1).text, u"Model: a"_s);
        QCOMPARE(notificationAt(messages, 1).kind, NotificationKind::AiUpscale);
        QTRY_COMPARE_WITH_TIMEOUT(store.published, 1, 2 * ViewerToggles::kModelSwitchDelayMs);
    }

    void cyclingContinuesFromThePendingModel() {
        FakeViewerToggleStore store;
        store.models = {u"a"_s, u"b"_s, u"c"_s};
        store.model = u"a"_s;
        ViewerToggles toggles(store);

        toggles.cycleUpscaylModel();
        // Something else rewrites the stored model before the announcement.
        store.model = u"a"_s;
        toggles.cycleUpscaylModel();

        QCOMPARE(store.model, u"c"_s);
    }

    void cyclingFromAnUnknownModelStartsAtTheFirst() {
        FakeViewerToggleStore store;
        store.models = {u"a"_s, u"b"_s};
        store.model = u"removed"_s;
        ViewerToggles toggles(store);

        toggles.cycleUpscaylModel();

        QCOMPARE(store.model, u"a"_s);
    }

    void cyclingWithoutModelsDoesNothing() {
        FakeViewerToggleStore store;
        store.model = u"a"_s;
        ViewerToggles toggles(store);
        QSignalSpy messages(&toggles, &ViewerToggles::notificationRequested);

        toggles.cycleUpscaylModel();

        QCOMPARE(store.model, u"a"_s);
        QCOMPARE(messages.size(), 0);
        QCOMPARE(store.published, 0);
    }
};

int runViewerTogglesTests(int argc, char **argv) {
    ViewerTogglesTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "tst_viewertoggles.moc"
