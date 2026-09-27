#include "core/device/DeviceMarker.h"
#include "core/device/GaugeAsset.h"
#include "core/device/GaugeProfile.h"
#include "core/project/ProjectManager.h"
#include "core/project/ProjectManifest.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <QUuid>

#include <cmath>
#include <limits>

namespace {

struct ProjectFixture
{
    QString root;
    QString manifestPath;
};

ProjectFixture makeFixture(const QString& name)
{
    ProjectFixture fixture;
    fixture.root = QDir(QDir::tempPath())
                       .filePath(QStringLiteral("gauge_asset_%1_%2")
                                     .arg(name, QUuid::createUuid().toString(QUuid::WithoutBraces)));
    fixture.manifestPath = QDir(fixture.root).filePath(QStringLiteral("project.json"));
    return fixture;
}

void removeFixture(const ProjectFixture& fixture)
{
    if (!fixture.root.isEmpty()) {
        QDir(fixture.root).removeRecursively();
    }
}

bool addMarker(vision3d::ProjectManager& manager,
               const QString& name,
               vision3d::DeviceMarker* marker,
               QString* error)
{
    return manager.addDeviceMarker(name,
                                   QVector3D(0.0F, 0.0F, 0.0F),
                                   QStringLiteral("task-1"),
                                   marker,
                                   error);
}

vision3d::GaugeAsset makeValidAsset(const QString& markerId = QStringLiteral("marker-1"))
{
    vision3d::GaugeAsset asset = vision3d::GaugeAsset::create(
        markerId, QStringLiteral("入口压力表"), 0.0, 2.5, QStringLiteral("MPa"));
    return asset;
}

class GaugeAssetTest final : public QObject
{
    Q_OBJECT

private slots:
    void defaultAsset();
    void validAsset();
    void invalidRange();
    void invalidValue();
    void stableId();
    void markerBinding();
    void duplicateMarkerBindingRejected();
    void addUpdateDelete();
    void oldManifestWithoutGaugeAssets();
    void jsonRoundTrip();
    void profileJsonRoundTrip();
    void projectPersistence();
    void projectIsolation();
    void markerDeleteCascade();
    void manualReadingUpdate();
    void outOfRangeReadingPreserved();
    void profileBindingValidation();
};

void GaugeAssetTest::defaultAsset()
{
    const vision3d::GaugeAsset asset;
    QString error;
    QVERIFY(!asset.isValid(&error));
    QVERIFY(error.contains(QStringLiteral("id")));
}

void GaugeAssetTest::validAsset()
{
    const vision3d::GaugeAsset asset = makeValidAsset();
    QString error;
    QVERIFY2(asset.isValid(&error), qPrintable(error));
    QCOMPARE(asset.rangeMin, 0.0);
    QCOMPARE(asset.rangeMax, 2.5);
    QCOMPARE(asset.dataSource, vision3d::GaugeDataSource::None);
}

void GaugeAssetTest::invalidRange()
{
    vision3d::GaugeAsset asset = makeValidAsset();
    asset.rangeMax = asset.rangeMin;
    QString error;
    QVERIFY(!asset.isValid(&error));
    QVERIFY(error.contains(QStringLiteral("rangeMax")));

    asset.rangeMax = std::numeric_limits<double>::infinity();
    QVERIFY(!asset.isValid(&error));
}

void GaugeAssetTest::invalidValue()
{
    vision3d::GaugeAsset asset = makeValidAsset();
    asset.latestValue = std::numeric_limits<double>::quiet_NaN();
    QString error;
    QVERIFY(!asset.isValid(&error));
    QVERIFY(error.contains(QStringLiteral("latestValue")));
}

void GaugeAssetTest::stableId()
{
    const vision3d::GaugeAsset first = makeValidAsset();
    const vision3d::GaugeAsset second = makeValidAsset();
    QVERIFY(!first.id.isEmpty());
    QVERIFY(!second.id.isEmpty());
    QVERIFY(first.id != second.id);
    QCOMPARE(first.id.size(), 36);
}

void GaugeAssetTest::markerBinding()
{
    vision3d::GaugeAssetModel model;
    const vision3d::GaugeAsset asset = makeValidAsset();
    QString error;
    QVERIFY2(model.add(asset, &error), qPrintable(error));
    QCOMPARE(model.findByMarkerId(asset.deviceMarkerId)->id, asset.id);
    QCOMPARE(model.list().size(), 1);
}

void GaugeAssetTest::duplicateMarkerBindingRejected()
{
    vision3d::GaugeAssetModel model;
    QString error;
    QVERIFY2(model.add(makeValidAsset(), &error), qPrintable(error));
    vision3d::GaugeAsset duplicate = makeValidAsset();
    duplicate.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(!model.add(duplicate, &error));
    QVERIFY(error.contains(QStringLiteral("绑定")));
}

void GaugeAssetTest::addUpdateDelete()
{
    vision3d::GaugeAssetModel model;
    vision3d::GaugeAsset asset = makeValidAsset();
    QString error;
    QVERIFY2(model.add(asset, &error), qPrintable(error));
    asset.name = QStringLiteral("主压力表");
    asset.rangeMax = 5.0;
    QVERIFY2(model.update(asset, &error), qPrintable(error));
    QCOMPARE(model.findById(asset.id)->name, QStringLiteral("主压力表"));
    QVERIFY2(model.remove(asset.id, &error), qPrintable(error));
    QVERIFY(!model.findById(asset.id).has_value());
}

void GaugeAssetTest::oldManifestWithoutGaugeAssets()
{
    vision3d::ProjectManifest manifest =
        vision3d::ProjectManifest::createNew(QStringLiteral("旧项目"));
    QJsonObject json = manifest.toJson();
    json.remove(QStringLiteral("gaugeAssets"));
    QString error;
    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::fromJson(json, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(restored->gaugeAssets().isEmpty());
}

void GaugeAssetTest::jsonRoundTrip()
{
    vision3d::GaugeAsset original = makeValidAsset();
    original.latestValue = 1.23;
    original.latestTimestamp = QDateTime::fromString(QStringLiteral("2026-09-26T08:00:00.000Z"),
                                                     Qt::ISODateWithMs);
    original.dataSource = vision3d::GaugeDataSource::Manual;
    QString error;
    const std::optional<vision3d::GaugeAsset> restored =
        vision3d::GaugeAsset::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->id, original.id);
    QCOMPARE(restored->deviceMarkerId, original.deviceMarkerId);
    QCOMPARE(restored->name, original.name);
    QCOMPARE(restored->rangeMin, original.rangeMin);
    QCOMPARE(restored->rangeMax, original.rangeMax);
    QCOMPARE(restored->unit, original.unit);
    QCOMPARE(*restored->latestValue, 1.23);
    QCOMPARE(restored->dataSource, vision3d::GaugeDataSource::Manual);
    QVERIFY(restored->latestTimestamp.has_value());
    QCOMPARE(restored->latestTimestamp->toUTC(), original.latestTimestamp->toUTC());
}

void GaugeAssetTest::profileJsonRoundTrip()
{
    vision3d::GaugeAsset original = makeValidAsset();
    original.gaugeProfileId = QStringLiteral("pressure_0_2_5_mpa");
    const QJsonObject json = original.toJson();
    QCOMPARE(json.value(QStringLiteral("gaugeProfileId")).toString(),
             original.gaugeProfileId);
    QString error;
    const std::optional<vision3d::GaugeAsset> restored =
        vision3d::GaugeAsset::fromJson(json, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->gaugeProfileId, original.gaugeProfileId);
}

void GaugeAssetTest::projectPersistence()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("persistence"));
    QVERIFY(QDir().mkpath(fixture.root));
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(fixture.root, QStringLiteral("Gauge Project"), &error),
             qPrintable(error));
    vision3d::DeviceMarker marker;
    QVERIFY2(addMarker(manager, QStringLiteral("P01"), &marker, &error), qPrintable(error));
    QVERIFY2(manager.addGaugeAsset(marker.id,
                                   QStringLiteral("入口压力表"),
                                   0.0,
                                   2.5,
                                   QStringLiteral("MPa"),
                                   nullptr,
                                   &error),
             qPrintable(error));
    const std::optional<vision3d::GaugeAsset> created =
        manager.gaugeAssetForMarker(marker.id);
    QVERIFY(created.has_value());
    const QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QVERIFY2(manager.updateGaugeReading(created->id,
                                        1.23,
                                        timestamp,
                                        vision3d::GaugeDataSource::Manual,
                                        &error),
             qPrintable(error));

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    const std::optional<vision3d::GaugeAsset> restored =
        reopened.gaugeAssetForMarker(marker.id);
    QVERIFY(restored.has_value());
    QCOMPARE(restored->id, created->id);
    QCOMPARE(restored->deviceMarkerId, marker.id);
    QCOMPARE(restored->name, QStringLiteral("入口压力表"));
    QCOMPARE(*restored->latestValue, 1.23);
    QCOMPARE(restored->dataSource, vision3d::GaugeDataSource::Manual);
    QVERIFY(restored->latestTimestamp.has_value());
    QVERIFY(QFileInfo::exists(QDir(manager.projectDirectory()).filePath(
        QStringLiteral("project.json"))));
    removeFixture(fixture);
}

void GaugeAssetTest::projectIsolation()
{
    const ProjectFixture firstFixture = makeFixture(QStringLiteral("first"));
    const ProjectFixture secondFixture = makeFixture(QStringLiteral("second"));
    QVERIFY(QDir().mkpath(firstFixture.root));
    QVERIFY(QDir().mkpath(secondFixture.root));
    vision3d::ProjectManager first;
    vision3d::ProjectManager second;
    QString error;
    QVERIFY2(first.newProject(firstFixture.root, QStringLiteral("First"), &error),
             qPrintable(error));
    QVERIFY2(second.newProject(secondFixture.root, QStringLiteral("Second"), &error),
             qPrintable(error));
    vision3d::DeviceMarker firstMarker;
    QVERIFY2(addMarker(first, QStringLiteral("P01"), &firstMarker, &error), qPrintable(error));
    QVERIFY2(first.addGaugeAsset(firstMarker.id,
                                 QStringLiteral("Gauge A"),
                                 0.0,
                                 1.0,
                                 QStringLiteral("bar"),
                                 nullptr,
                                 &error),
             qPrintable(error));
    QVERIFY(second.gaugeAssets().isEmpty());
    QVERIFY(!second.gaugeAssetForMarker(firstMarker.id).has_value());
    removeFixture(firstFixture);
    removeFixture(secondFixture);
}

void GaugeAssetTest::markerDeleteCascade()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("cascade"));
    QVERIFY(QDir().mkpath(fixture.root));
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(fixture.root, QStringLiteral("Cascade"), &error),
             qPrintable(error));
    vision3d::DeviceMarker marker;
    QVERIFY2(addMarker(manager, QStringLiteral("P01"), &marker, &error), qPrintable(error));
    QVERIFY2(manager.addGaugeAsset(marker.id,
                                   QStringLiteral("Gauge"),
                                   0.0,
                                   10.0,
                                   QStringLiteral("kPa"),
                                   nullptr,
                                   &error),
             qPrintable(error));
    const QString gaugeId = manager.gaugeAssets().first().id;
    QVERIFY2(manager.removeDeviceMarker(marker.id, &error), qPrintable(error));
    QVERIFY(!manager.deviceMarkerById(marker.id).has_value());
    QVERIFY(!manager.gaugeAssetById(gaugeId).has_value());

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    QVERIFY(reopened.deviceMarkersForReconstruction(QStringLiteral("task-1")).isEmpty());
    QVERIFY(reopened.gaugeAssets().isEmpty());
    removeFixture(fixture);
}

void GaugeAssetTest::manualReadingUpdate()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("reading"));
    QVERIFY(QDir().mkpath(fixture.root));
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(fixture.root, QStringLiteral("Reading"), &error),
             qPrintable(error));
    vision3d::DeviceMarker marker;
    QVERIFY2(addMarker(manager, QStringLiteral("P01"), &marker, &error), qPrintable(error));
    QVERIFY2(manager.addGaugeAsset(marker.id,
                                   QStringLiteral("Gauge"),
                                   0.0,
                                   2.5,
                                   QStringLiteral("MPa"),
                                   nullptr,
                                   &error),
             qPrintable(error));
    const QString gaugeId = manager.gaugeAssets().first().id;
    const QDateTime timestamp = QDateTime::fromString(
        QStringLiteral("2026-09-26T09:10:11.123Z"), Qt::ISODateWithMs);
    QVERIFY2(manager.updateGaugeReading(gaugeId,
                                        1.25,
                                        timestamp,
                                        vision3d::GaugeDataSource::Manual,
                                        &error),
             qPrintable(error));
    const vision3d::GaugeAsset restored = *manager.gaugeAssetById(gaugeId);
    QCOMPARE(*restored.latestValue, 1.25);
    QCOMPARE(restored.dataSource, vision3d::GaugeDataSource::Manual);
    QCOMPARE(restored.latestTimestamp->toUTC(), timestamp.toUTC());
    removeFixture(fixture);
}

void GaugeAssetTest::outOfRangeReadingPreserved()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("out_of_range"));
    QVERIFY(QDir().mkpath(fixture.root));
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(fixture.root, QStringLiteral("Out Of Range"), &error),
             qPrintable(error));
    vision3d::DeviceMarker marker;
    QVERIFY2(addMarker(manager, QStringLiteral("P01"), &marker, &error), qPrintable(error));
    QVERIFY2(manager.addGaugeAsset(marker.id,
                                   QStringLiteral("Gauge"),
                                   0.0,
                                   2.5,
                                   QStringLiteral("MPa"),
                                   nullptr,
                                   &error),
             qPrintable(error));
    const QString gaugeId = manager.gaugeAssets().first().id;
    QVERIFY2(manager.updateGaugeReading(gaugeId,
                                        2.7,
                                        QDateTime::currentDateTimeUtc(),
                                        vision3d::GaugeDataSource::Manual,
                                        &error),
             qPrintable(error));
    QCOMPARE(*manager.gaugeAssetById(gaugeId)->latestValue, 2.7);
    removeFixture(fixture);
}

void GaugeAssetTest::profileBindingValidation()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("profile_binding"));
    QVERIFY(QDir().mkpath(fixture.root));
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(fixture.root, QStringLiteral("Profile Binding"), &error),
             qPrintable(error));
    vision3d::DeviceMarker marker;
    QVERIFY2(addMarker(manager, QStringLiteral("P01"), &marker, &error), qPrintable(error));

    vision3d::GaugeAsset asset = makeValidAsset(marker.id);
    asset.gaugeProfileId = QStringLiteral("pressure_0_2_5_mpa");
    QVERIFY2(manager.addGaugeAsset(asset, &error), qPrintable(error));
    QCOMPARE(manager.gaugeAssetForMarker(marker.id)->gaugeProfileId,
             QStringLiteral("pressure_0_2_5_mpa"));

    vision3d::GaugeAsset mismatch = *manager.gaugeAssetForMarker(marker.id);
    mismatch.unit = QStringLiteral("bar");
    QVERIFY(!manager.updateGaugeAsset(mismatch, &error));
    QVERIFY(error.contains(QStringLiteral("unit/range")));

    mismatch = *manager.gaugeAssetForMarker(marker.id);
    mismatch.gaugeProfileId = QStringLiteral("missing_profile");
    QVERIFY(!manager.updateGaugeAsset(mismatch, &error));
    QVERIFY(error.contains(QStringLiteral("未知 GaugeProfile")));
    removeFixture(fixture);
}

} // namespace

QTEST_APPLESS_MAIN(GaugeAssetTest)
#include "test_gauge_asset.moc"
