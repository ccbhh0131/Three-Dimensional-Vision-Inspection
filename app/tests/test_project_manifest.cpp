#include "core/project/ProjectManifest.h"

#include <QFile>
#include <QJsonObject>
#include <QQuaternion>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class ProjectManifestTest final : public QObject
{
    Q_OBJECT

private slots:
    void serializeDeserializeRoundTrip();
    void sceneAlignmentRoundTripAndOldCompatibility();
    void invalidSceneAlignmentIsRejected();
    void invalidJsonIsRejected();
    void unsupportedSchemaVersionIsRejected();
    void qSaveFileProjectSaveCanBeReadAgain();
};

void ProjectManifestTest::serializeDeserializeRoundTrip()
{
    vision3d::ProjectManifest original = vision3d::ProjectManifest::createNew(QStringLiteral("Temple Demo"));
    original.setBackendVersion(QStringLiteral("3.11.1"));
    original.setReconstructionState(QStringLiteral("idle"));

    QString error;
    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->schemaVersion(), original.schemaVersion());
    QCOMPARE(restored->projectId(), original.projectId());
    QCOMPARE(restored->name(), original.name());
    QCOMPARE(restored->backend(), original.backend());
    QCOMPARE(restored->backendVersion(), original.backendVersion());
    QCOMPARE(restored->reconstructionState(), original.reconstructionState());
}

void ProjectManifestTest::sceneAlignmentRoundTripAndOldCompatibility()
{
    vision3d::ProjectManifest original = vision3d::ProjectManifest::createNew(
        QStringLiteral("Aligned Temple"));
    vision3d::SceneAlignmentTransform alignment;
    alignment.rotateByAxis(QStringLiteral("Y"), 90.0f);
    original.setSceneAlignment(alignment);

    QString error;
    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    const QQuaternion restoredRotation = restored->sceneAlignment().rotation();
    QVERIFY(qFuzzyCompare(restoredRotation.scalar(), alignment.rotation().scalar()));
    QVERIFY(qFuzzyCompare(restoredRotation.x(), alignment.rotation().x()));
    QVERIFY(qFuzzyCompare(restoredRotation.y(), alignment.rotation().y()));
    QVERIFY(qFuzzyCompare(restoredRotation.z(), alignment.rotation().z()));

    QJsonObject oldProject = original.toJson();
    oldProject.remove(QStringLiteral("sceneAlignment"));
    const std::optional<vision3d::ProjectManifest> oldRestored =
        vision3d::ProjectManifest::fromJson(oldProject, &error);
    QVERIFY2(oldRestored.has_value(), qPrintable(error));
    const QQuaternion identity = oldRestored->sceneAlignment().rotation();
    QCOMPARE(identity, QQuaternion(1.0f, 0.0f, 0.0f, 0.0f));
}

void ProjectManifestTest::invalidSceneAlignmentIsRejected()
{
    vision3d::ProjectManifest original = vision3d::ProjectManifest::createNew(
        QStringLiteral("Invalid Alignment"));
    QJsonObject json = original.toJson();
    json.insert(QStringLiteral("sceneAlignment"),
                QJsonObject{{QStringLiteral("rotation"),
                             QJsonObject{{QStringLiteral("w"), 0.0},
                                          {QStringLiteral("x"), 0.0},
                                          {QStringLiteral("y"), 0.0},
                                          {QStringLiteral("z"), 0.0}}}});

    QString error;
    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::fromJson(json, &error);
    QVERIFY(!restored.has_value());
    QVERIFY(error.contains(QStringLiteral("有效四元数")));
}

void ProjectManifestTest::invalidJsonIsRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("invalid.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write("{invalid json") > 0);
    file.close();

    QString error;
    const std::optional<vision3d::ProjectManifest> manifest =
        vision3d::ProjectManifest::load(path, &error);
    QVERIFY(!manifest.has_value());
    QVERIFY(error.contains(QStringLiteral("JSON 解析失败")));
}

void ProjectManifestTest::unsupportedSchemaVersionIsRejected()
{
    const QJsonObject json{
        {QStringLiteral("schemaVersion"), 99},
        {QStringLiteral("projectId"), QStringLiteral("project-id")},
        {QStringLiteral("name"), QStringLiteral("Unsupported")},
    };

    QString error;
    const std::optional<vision3d::ProjectManifest> manifest =
        vision3d::ProjectManifest::fromJson(json, &error);
    QVERIFY(!manifest.has_value());
    QVERIFY(error.contains(QStringLiteral("不支持的 schemaVersion")));
}

void ProjectManifestTest::qSaveFileProjectSaveCanBeReadAgain()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("project.json"));
    const vision3d::ProjectManifest original =
        vision3d::ProjectManifest::createNew(QStringLiteral("Atomic Save"));

    QString error;
    QVERIFY2(original.save(path, &error), qPrintable(error));
    QVERIFY(QFile::exists(path));

    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::load(path, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->projectId(), original.projectId());
    QCOMPARE(restored->name(), original.name());
}

} // namespace

QTEST_MAIN(ProjectManifestTest)
#include "test_project_manifest.moc"
