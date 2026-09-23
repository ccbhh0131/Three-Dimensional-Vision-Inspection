#include "core/project/ProjectManager.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class ProjectManagerTest final : public QObject
{
    Q_OBJECT

private slots:
    void newAndOpenProjectWorkspace();
    void existingManifestIsNotOverwritten();
};

void ProjectManagerTest::newAndOpenProjectWorkspace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(directory.path(), QStringLiteral("零件检测01"), &error),
             qPrintable(error));

    const QString workspace = manager.projectDirectory();
    QVERIFY(QFileInfo::exists(QDir(workspace).filePath(QStringLiteral("project.json"))));
    QVERIFY(QDir(QDir(workspace).filePath(QStringLiteral("images"))).exists());
    QVERIFY(QDir(QDir(workspace).filePath(QStringLiteral("cache"))).exists());
    QVERIFY(QDir(QDir(workspace).filePath(QStringLiteral("reconstruction"))).exists());
    QVERIFY(QDir(QDir(workspace).filePath(QStringLiteral("logs"))).exists());

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(workspace, &error), qPrintable(error));
    QVERIFY(reopened.hasProject());
    QCOMPARE(reopened.currentManifest()->name(), QStringLiteral("零件检测01"));
    QCOMPARE(reopened.projectDirectory(), workspace);
}

void ProjectManagerTest::existingManifestIsNotOverwritten()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("NoOverwrite"), &error));
    QVERIFY(!manager.newProject(directory.path(), QStringLiteral("NoOverwrite"), &error));
    QVERIFY(error.contains(QStringLiteral("未执行覆盖")));
}

} // namespace

QTEST_MAIN(ProjectManagerTest)
#include "test_project_manager.moc"
