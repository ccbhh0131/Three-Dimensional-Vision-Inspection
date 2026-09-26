#include "core/device/DeviceMarker.h"
#include "core/project/ProjectManager.h"
#include "core/project/ProjectManifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>
#include <QUuid>

namespace {

struct Fixture
{
    QString root;
    QString projectPath;
};

Fixture makeProjectPath(const QString& name)
{
    Fixture fixture;
    fixture.root = QDir(QDir::tempPath())
                       .filePath(QStringLiteral("%1_%2")
                                     .arg(name)
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    fixture.projectPath = QDir(fixture.root).filePath(QStringLiteral("project.json"));
    return fixture;
}

void removeFixture(const Fixture& fixture)
{
    if (!fixture.root.isEmpty()) {
        QDir(fixture.root).removeRecursively();
    }
}

bool writeJson(const QString& path, const QJsonObject& json)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray payload = QJsonDocument(json).toJson(QJsonDocument::Indented);
    return file.write(payload) == payload.size();
}

class DeviceMarkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void modelRoundTripAndUniqueId();
    void invalidMarkerDataIsRejectedSafely();
    void oldManifestWithoutMarkersLoadsEmpty();
    void addSaveReloadAndWorldCoordinateRoundTrip();
    void deleteSaveAndMultipleMarkers();
    void projectIsolationAndReconstructionIdentity();
};

void DeviceMarkerTest::modelRoundTripAndUniqueId()
{
    vision3d::DeviceMarker first = vision3d::DeviceMarker::create(
        QStringLiteral("P01"), QVector3D(12.25f, -6.5f, 25.0f), QStringLiteral("task-a"));
    vision3d::DeviceMarker second = vision3d::DeviceMarker::create(
        QStringLiteral("P02"), QVector3D(13.25f, -6.0f, 25.5f), QStringLiteral("task-a"));
    QVERIFY(first.id != second.id);

    vision3d::DeviceMarkerModel model;
    QString error;
    QVERIFY2(model.add(first, &error), qPrintable(error));
    QVERIFY2(model.add(second, &error), qPrintable(error));
    QCOMPARE(model.size(), qsizetype(2));

    const std::optional<vision3d::DeviceMarkerModel> restored =
        vision3d::DeviceMarkerModel::fromJson(model.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->size(), qsizetype(2));
    QCOMPARE(restored->find(first.id)->worldPosition, first.worldPosition);
    QCOMPARE(restored->find(first.id)->reconstructionTaskId, QStringLiteral("task-a"));
}

void DeviceMarkerTest::invalidMarkerDataIsRejectedSafely()
{
    const QJsonArray invalid{
        QJsonObject{
            {QStringLiteral("id"), QStringLiteral("bad")},
            {QStringLiteral("name"), QStringLiteral("P01")},
            {QStringLiteral("position"), QJsonObject{{QStringLiteral("x"), 1.0}}},
            {QStringLiteral("reconstructionTaskId"), QStringLiteral("task-a")},
        },
    };
    QString error;
    const std::optional<vision3d::DeviceMarkerModel> model =
        vision3d::DeviceMarkerModel::fromJson(invalid, &error);
    QVERIFY(!model.has_value());
    QVERIFY(!error.isEmpty());

    vision3d::ProjectManifest manifest = vision3d::ProjectManifest::createNew(
        QStringLiteral("Invalid Marker Manifest"));
    manifest.setDeviceMarkers(invalid);
    QVERIFY2(manifest.validate(&error), qPrintable(error));
    const std::optional<vision3d::ProjectManifest> loaded =
        vision3d::ProjectManifest::fromJson(manifest.toJson(), &error);
    QVERIFY2(loaded.has_value(), qPrintable(error));

    const Fixture fixture = makeProjectPath(QStringLiteral("invalid_marker_manifest"));
    QVERIFY(QDir().mkpath(fixture.root));
    QJsonObject invalidManifest = manifest.toJson();
    invalidManifest.insert(QStringLiteral("deviceMarkers"), invalid);
    QVERIFY(writeJson(fixture.projectPath, invalidManifest));
    vision3d::ProjectManager manager;
    QVERIFY(!manager.openProject(fixture.projectPath, &error));
    QVERIFY(error.contains(QStringLiteral("deviceMarkers")));
    removeFixture(fixture);
}

void DeviceMarkerTest::oldManifestWithoutMarkersLoadsEmpty()
{
    const Fixture fixture = makeProjectPath(QStringLiteral("old_manifest"));
    QVERIFY(QDir().mkpath(fixture.root));
    vision3d::ProjectManifest manifest = vision3d::ProjectManifest::createNew(
        QStringLiteral("Old Manifest"));
    QJsonObject oldJson = manifest.toJson();
    oldJson.remove(QStringLiteral("deviceMarkers"));
    QVERIFY(writeJson(fixture.projectPath, oldJson));

    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.openProject(fixture.projectPath, &error), qPrintable(error));
    QCOMPARE(manager.deviceMarkerModel().size(), qsizetype(0));
    removeFixture(fixture);
}

void DeviceMarkerTest::addSaveReloadAndWorldCoordinateRoundTrip()
{
    QTemporaryDir parentDirectory;
    QVERIFY(parentDirectory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(parentDirectory.path(),
                                 QStringLiteral("round_trip_%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
                                 &error),
             qPrintable(error));
    const QString projectDirectory = manager.projectDirectory();
    vision3d::DeviceMarker created;
    const QVector3D position(12.25f, -6.5f, 25.0f);
    QVERIFY2(manager.addDeviceMarker(QStringLiteral("P01"),
                                     position,
                                     QStringLiteral("task-a"),
                                     &created,
                                     &error),
             qPrintable(error));
    QVERIFY(!created.id.isEmpty());
    QCOMPARE(manager.deviceMarkerModel().size(), qsizetype(1));

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(projectDirectory, &error), qPrintable(error));
    const std::optional<vision3d::DeviceMarker> restored =
        reopened.deviceMarkerById(created.id);
    QVERIFY(restored.has_value());
    QCOMPARE(restored->name, QStringLiteral("P01"));
    QCOMPARE(restored->worldPosition, position);
    QCOMPARE(restored->reconstructionTaskId, QStringLiteral("task-a"));
    QVERIFY(QFileInfo::exists(QDir(projectDirectory).filePath(QStringLiteral("project.json"))));
    reopened.closeProject();
    QDir(projectDirectory).removeRecursively();
}

void DeviceMarkerTest::deleteSaveAndMultipleMarkers()
{
    QTemporaryDir parentDirectory;
    QVERIFY(parentDirectory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(parentDirectory.path(),
                                 QStringLiteral("delete_%1")
                                     .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
                                 &error),
             qPrintable(error));
    vision3d::DeviceMarker first;
    vision3d::DeviceMarker second;
    QVERIFY2(manager.addDeviceMarker(QStringLiteral("P01"),
                                     QVector3D(1.0f, 2.0f, 3.0f),
                                     QStringLiteral("task-a"),
                                     &first,
                                     &error),
             qPrintable(error));
    QVERIFY2(manager.addDeviceMarker(QStringLiteral("P02"),
                                     QVector3D(4.0f, 5.0f, 6.0f),
                                     QStringLiteral("task-a"),
                                     &second,
                                     &error),
             qPrintable(error));
    QCOMPARE(manager.deviceMarkerModel().size(), qsizetype(2));
    QVERIFY2(manager.removeDeviceMarker(first.id, &error), qPrintable(error));
    QCOMPARE(manager.deviceMarkerModel().size(), qsizetype(1));

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    QVERIFY(!reopened.deviceMarkerById(first.id).has_value());
    QVERIFY(reopened.deviceMarkerById(second.id).has_value());
    QDir(manager.projectDirectory()).removeRecursively();
}

void DeviceMarkerTest::projectIsolationAndReconstructionIdentity()
{
    QTemporaryDir firstParentDirectory;
    QTemporaryDir secondParentDirectory;
    QVERIFY(firstParentDirectory.isValid());
    QVERIFY(secondParentDirectory.isValid());
    vision3d::ProjectManager firstManager;
    vision3d::ProjectManager secondManager;
    QString error;
    QVERIFY2(firstManager.newProject(firstParentDirectory.path(),
                                     QStringLiteral("project_a_%1")
                                         .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
                                     &error),
             qPrintable(error));
    vision3d::DeviceMarker marker;
    QVERIFY2(firstManager.addDeviceMarker(QStringLiteral("P01"),
                                          QVector3D(9.0f, 8.0f, 7.0f),
                                          QStringLiteral("task-a"),
                                          &marker,
                                          &error),
             qPrintable(error));
    QCOMPARE(firstManager.deviceMarkersForReconstruction(QStringLiteral("task-a")).size(),
             qsizetype(1));
    QCOMPARE(firstManager.deviceMarkersForReconstruction(QStringLiteral("task-b")).size(),
             qsizetype(0));

    QVERIFY2(secondManager.newProject(secondParentDirectory.path(),
                                      QStringLiteral("project_b_%1")
                                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
                                      &error),
             qPrintable(error));
    QCOMPARE(secondManager.deviceMarkerModel().size(), qsizetype(0));
    QDir(firstManager.projectDirectory()).removeRecursively();
    QDir(secondManager.projectDirectory()).removeRecursively();
}

} // namespace

QTEST_APPLESS_MAIN(DeviceMarkerTest)

#include "test_device_marker.moc"
