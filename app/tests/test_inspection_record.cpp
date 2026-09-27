#include "core/device/DeviceMarker.h"
#include "core/device/GaugeAsset.h"
#include "core/inspection/InspectionRecord.h"
#include "core/project/ProjectManager.h"
#include "core/project/ProjectManifest.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <QUuid>
#include <QVector3D>

#include <cmath>
#include <limits>
#include <optional>

namespace {

struct ProjectFixture
{
    QString root;
};

ProjectFixture makeFixture(const QString& suffix)
{
    ProjectFixture fixture;
    fixture.root = QDir(QDir::tempPath()).filePath(
        QStringLiteral("stage4e_history_%1_%2")
            .arg(suffix, QUuid::createUuid().toString(QUuid::WithoutBraces)));
    return fixture;
}

void removeFixture(const ProjectFixture& fixture)
{
    if (!fixture.root.isEmpty()) {
        QDir(fixture.root).removeRecursively();
    }
}

QDateTime timeAt(int seconds)
{
    return QDateTime::fromString(
        QStringLiteral("2026-09-26T12:%1:%2.000Z")
            .arg(seconds / 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0')),
        Qt::ISODateWithMs);
}

bool createGaugeProject(const ProjectFixture& fixture,
                        vision3d::ProjectManager* manager,
                        vision3d::DeviceMarker* marker,
                        QString* gaugeId,
                        QString* error)
{
    if (!QDir().mkpath(fixture.root)
        || !manager->newProject(fixture.root, QStringLiteral("History Project"), error)) {
        return false;
    }
    if (!manager->addDeviceMarker(QStringLiteral("P01"),
                                  QVector3D(0.0F, 0.0F, 0.0F),
                                  QStringLiteral("task-1"),
                                  marker,
                                  error)) {
        return false;
    }
    if (!manager->addGaugeAsset(marker->id,
                                QStringLiteral("入口压力表"),
                                0.0,
                                2.5,
                                QStringLiteral("MPa"),
                                nullptr,
                                error)) {
        return false;
    }
    *gaugeId = manager->gaugeAssets().first().id;
    return true;
}

class InspectionRecordTest final : public QObject
{
    Q_OBJECT

private slots:
    void validRecord();
    void invalidGaugeId();
    void invalidValue();
    void invalidTimestamp();
    void manualRecord();
    void visualRecord();
    void stableId();
    void projectJsonRoundTrip();
    void oldManifestWithoutRecords();
    void multipleRecordsOneGauge();
    void sortByTimestamp();
    void latestStateUsesNewestTimestamp();
    void pastRecordDoesNotReplaceLatest();
    void gaugeDeleteCascade();
    void markerDeleteCascade();
    void projectIsolation();
    void visualImageAssetBinding();
};

void InspectionRecordTest::validRecord()
{
    const vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"),
        1.2,
        timeAt(10),
        vision3d::GaugeDataSource::Manual);
    QString error;
    QVERIFY2(record.isValid(&error), qPrintable(error));
    QCOMPARE(record.gaugeAssetId, QStringLiteral("gauge-1"));
    QCOMPARE(record.dataSource, vision3d::GaugeDataSource::Manual);
}

void InspectionRecordTest::invalidGaugeId()
{
    vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QString(), 1.0, timeAt(10), vision3d::GaugeDataSource::Manual);
    QString error;
    QVERIFY(!record.isValid(&error));
    QVERIFY(error.contains(QStringLiteral("gaugeAssetId")));

    const ProjectFixture fixture = makeFixture(QStringLiteral("invalid_gauge"));
    vision3d::ProjectManager manager;
    vision3d::DeviceMarker marker;
    QString gaugeId;
    QVERIFY(createGaugeProject(fixture, &manager, &marker, &gaugeId, &error));
    QVERIFY(!manager.recordGaugeReading(QStringLiteral("missing-gauge"),
                                        1.0,
                                        timeAt(11),
                                        vision3d::GaugeDataSource::Manual,
                                        &error));
    QVERIFY(error.contains(QStringLiteral("未找到 GaugeAsset")));
    removeFixture(fixture);
}

void InspectionRecordTest::invalidValue()
{
    vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"),
        std::numeric_limits<double>::quiet_NaN(),
        timeAt(10),
        vision3d::GaugeDataSource::Manual);
    QString error;
    QVERIFY(!record.isValid(&error));
    QVERIFY(error.contains(QStringLiteral("value")));
}

void InspectionRecordTest::invalidTimestamp()
{
    vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.0, QDateTime(), vision3d::GaugeDataSource::Manual);
    QString error;
    QVERIFY(!record.isValid(&error));
    QVERIFY(error.contains(QStringLiteral("timestamp")));
}

void InspectionRecordTest::manualRecord()
{
    const vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.2, timeAt(10), vision3d::GaugeDataSource::Manual);
    QVERIFY(record.imageAssetId.isEmpty());
    QVERIFY(record.sourceImagePath.isEmpty());
    QVERIFY(record.isValid());
}

void InspectionRecordTest::visualRecord()
{
    const vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"),
        1.24,
        timeAt(20),
        vision3d::GaugeDataSource::Visual,
        QStringLiteral("image-1"),
        QStringLiteral("images/image-1.jpg"));
    QVERIFY(record.isValid());
    const QJsonObject json = record.toJson();
    QCOMPARE(json.value(QStringLiteral("dataSource")).toString(), QStringLiteral("visual"));
    QCOMPARE(json.value(QStringLiteral("imageAssetId")).toString(), QStringLiteral("image-1"));
    QString error;
    const std::optional<vision3d::InspectionRecord> restored =
        vision3d::InspectionRecord::fromJson(json, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->value, 1.24);
    QCOMPARE(restored->sourceImagePath, QStringLiteral("images/image-1.jpg"));
}

void InspectionRecordTest::stableId()
{
    const vision3d::InspectionRecord first = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.0, timeAt(1), vision3d::GaugeDataSource::Manual);
    const vision3d::InspectionRecord second = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.0, timeAt(1), vision3d::GaugeDataSource::Manual);
    QCOMPARE(first.id.size(), 36);
    QVERIFY(first.id != second.id);
}

void InspectionRecordTest::projectJsonRoundTrip()
{
    vision3d::InspectionRecordModel model;
    const vision3d::InspectionRecord record = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.24, timeAt(30), vision3d::GaugeDataSource::Visual);
    QString error;
    QVERIFY2(model.add(record, &error), qPrintable(error));
    const std::optional<vision3d::InspectionRecordModel> restored =
        vision3d::InspectionRecordModel::fromJson(model.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->size(), 1);
    QCOMPARE(restored->findById(record.id)->value, record.value);
}

void InspectionRecordTest::oldManifestWithoutRecords()
{
    vision3d::ProjectManifest manifest =
        vision3d::ProjectManifest::createNew(QStringLiteral("旧项目"));
    QJsonObject json = manifest.toJson();
    json.remove(QStringLiteral("inspectionRecords"));
    QString error;
    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::fromJson(json, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(restored->inspectionRecords().isEmpty());
}

void InspectionRecordTest::multipleRecordsOneGauge()
{
    vision3d::InspectionRecordModel model;
    QString error;
    QVERIFY(model.add(vision3d::InspectionRecord::create(
                         QStringLiteral("gauge-1"),
                         1.2,
                         timeAt(10),
                         vision3d::GaugeDataSource::Manual),
                     &error));
    QVERIFY(model.add(vision3d::InspectionRecord::create(
                         QStringLiteral("gauge-1"),
                         1.24,
                         timeAt(20),
                         vision3d::GaugeDataSource::Visual),
                     &error));
    QCOMPARE(model.recordsForGauge(QStringLiteral("gauge-1")).size(), 2);
    QCOMPARE(model.size(), 2);
}

void InspectionRecordTest::sortByTimestamp()
{
    vision3d::InspectionRecordModel model;
    QString error;
    const vision3d::InspectionRecord oldRecord = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.1, timeAt(10), vision3d::GaugeDataSource::Manual);
    const vision3d::InspectionRecord newRecord = vision3d::InspectionRecord::create(
        QStringLiteral("gauge-1"), 1.3, timeAt(30), vision3d::GaugeDataSource::Visual);
    QVERIFY(model.add(oldRecord, &error));
    QVERIFY(model.add(newRecord, &error));
    const QList<vision3d::InspectionRecord> sorted =
        model.recordsForGauge(QStringLiteral("gauge-1"));
    QCOMPARE(sorted.first().id, newRecord.id);
    QCOMPARE(sorted.last().id, oldRecord.id);
}

void InspectionRecordTest::latestStateUsesNewestTimestamp()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("latest"));
    vision3d::ProjectManager manager;
    vision3d::DeviceMarker marker;
    QString gaugeId;
    QString error;
    QVERIFY(createGaugeProject(fixture, &manager, &marker, &gaugeId, &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.20,
                                       timeAt(10),
                                       vision3d::GaugeDataSource::Manual,
                                       &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.24,
                                       timeAt(30),
                                       vision3d::GaugeDataSource::Visual,
                                       &error));
    const vision3d::GaugeAsset gauge = *manager.gaugeAssetById(gaugeId);
    QCOMPARE(*gauge.latestValue, 1.24);
    QCOMPARE(gauge.dataSource, vision3d::GaugeDataSource::Visual);
    QCOMPARE(gauge.latestTimestamp->toUTC(), timeAt(30));
    removeFixture(fixture);
}

void InspectionRecordTest::pastRecordDoesNotReplaceLatest()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("past"));
    vision3d::ProjectManager manager;
    vision3d::DeviceMarker marker;
    QString gaugeId;
    QString error;
    QVERIFY(createGaugeProject(fixture, &manager, &marker, &gaugeId, &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.24,
                                       timeAt(30),
                                       vision3d::GaugeDataSource::Visual,
                                       &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.20,
                                       timeAt(10),
                                       vision3d::GaugeDataSource::Manual,
                                       &error));
    const vision3d::GaugeAsset gauge = *manager.gaugeAssetById(gaugeId);
    QCOMPARE(*gauge.latestValue, 1.24);
    QCOMPARE(gauge.dataSource, vision3d::GaugeDataSource::Visual);
    QCOMPARE(manager.inspectionRecordsForGauge(gaugeId).size(), 2);
    removeFixture(fixture);
}

void InspectionRecordTest::gaugeDeleteCascade()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("gauge_delete"));
    vision3d::ProjectManager manager;
    vision3d::DeviceMarker marker;
    QString gaugeId;
    QString error;
    QVERIFY(createGaugeProject(fixture, &manager, &marker, &gaugeId, &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.2,
                                       timeAt(10),
                                       vision3d::GaugeDataSource::Manual,
                                       &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.3,
                                       timeAt(20),
                                       vision3d::GaugeDataSource::Manual,
                                       &error));
    QVERIFY(manager.removeGaugeAsset(gaugeId, &error));
    QVERIFY(manager.inspectionRecords().isEmpty());
    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    QVERIFY(reopened.inspectionRecords().isEmpty());
    removeFixture(fixture);
}

void InspectionRecordTest::markerDeleteCascade()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("marker_delete"));
    vision3d::ProjectManager manager;
    vision3d::DeviceMarker marker;
    QString gaugeId;
    QString error;
    QVERIFY(createGaugeProject(fixture, &manager, &marker, &gaugeId, &error));
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.2,
                                       timeAt(10),
                                       vision3d::GaugeDataSource::Manual,
                                       &error));
    QVERIFY(manager.removeDeviceMarker(marker.id, &error));
    QVERIFY(manager.gaugeAssets().isEmpty());
    QVERIFY(manager.inspectionRecords().isEmpty());
    removeFixture(fixture);
}

void InspectionRecordTest::projectIsolation()
{
    const ProjectFixture firstFixture = makeFixture(QStringLiteral("first"));
    const ProjectFixture secondFixture = makeFixture(QStringLiteral("second"));
    vision3d::ProjectManager first;
    vision3d::ProjectManager second;
    vision3d::DeviceMarker firstMarker;
    vision3d::DeviceMarker secondMarker;
    QString firstGauge;
    QString secondGauge;
    QString error;
    QVERIFY(createGaugeProject(firstFixture, &first, &firstMarker, &firstGauge, &error));
    QVERIFY(createGaugeProject(secondFixture, &second, &secondMarker, &secondGauge, &error));
    QVERIFY(first.recordGaugeReading(firstGauge,
                                     1.2,
                                     timeAt(10),
                                     vision3d::GaugeDataSource::Manual,
                                     &error));
    QCOMPARE(first.inspectionRecordsForGauge(firstGauge).size(), 1);
    QVERIFY(second.inspectionRecords().isEmpty());
    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(first.projectDirectory(), &error), qPrintable(error));
    QCOMPARE(reopened.inspectionRecordsForGauge(firstGauge).size(), 1);
    QVERIFY(!reopened.inspectionRecordsForGauge(secondGauge).size());
    removeFixture(firstFixture);
    removeFixture(secondFixture);
}

void InspectionRecordTest::visualImageAssetBinding()
{
    const ProjectFixture fixture = makeFixture(QStringLiteral("image_binding"));
    vision3d::ProjectManager manager;
    vision3d::DeviceMarker marker;
    QString gaugeId;
    QString error;
    QVERIFY(createGaugeProject(fixture, &manager, &marker, &gaugeId, &error));

    const QString sourcePath = QDir(fixture.root).filePath(QStringLiteral("source.png"));
    QImage source(24, 18, QImage::Format_RGB32);
    source.fill(Qt::green);
    QVERIFY(source.save(sourcePath));
    const QList<vision3d::AssetImportResult> results = manager.importImages({sourcePath}, &error);
    QCOMPARE(results.size(), 1);
    QVERIFY2(results.first().status == vision3d::AssetImportStatus::Imported,
             qPrintable(results.first().message));
    const QString imageAssetId = results.first().asset.id;
    QVERIFY(manager.recordGaugeReading(gaugeId,
                                       1.24,
                                       timeAt(40),
                                       vision3d::GaugeDataSource::Visual,
                                       imageAssetId,
                                       &error));
    const vision3d::InspectionRecord record =
        manager.inspectionRecordsForGauge(gaugeId).first();
    QCOMPARE(record.imageAssetId, imageAssetId);
    QCOMPARE(record.sourceImagePath, results.first().asset.relativePath);
    QVERIFY(!manager.removeAsset(imageAssetId, &error));
    QVERIFY(error.contains(QStringLiteral("巡检历史引用")));

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    const vision3d::InspectionRecord restored =
        reopened.inspectionRecordsForGauge(gaugeId).first();
    QCOMPARE(restored.imageAssetId, imageAssetId);
    QVERIFY(QFileInfo(reopened.absoluteAssetPath(*reopened.assetById(imageAssetId))).isFile());
    removeFixture(fixture);
}

} // namespace

QTEST_APPLESS_MAIN(InspectionRecordTest)
#include "test_inspection_record.moc"
