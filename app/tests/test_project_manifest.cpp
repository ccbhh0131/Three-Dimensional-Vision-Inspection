#include "core/project/ProjectManifest.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class ProjectManifestTest final : public QObject
{
    Q_OBJECT

private slots:
    void serializeDeserializeRoundTrip();
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
