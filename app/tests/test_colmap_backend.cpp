#include "backend/ColmapBackend.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class ColmapBackendTest final : public QObject
{
    Q_OBJECT

private slots:
    void missingDirectoryIsUnavailable();
    void missingBatchFileIsUnavailable();
    void versionParserFindsExpectedVersion();
};

void ColmapBackendTest::missingDirectoryIsUnavailable()
{
    vision3d::ColmapBackend backend;
    const vision3d::BackendProbeResult result =
        backend.probe(QStringLiteral("Z:/definitely-not-a-colmap-directory"));
    QVERIFY(!result.available);
    QVERIFY(result.message.contains(QStringLiteral("根目录不存在")));
}

void ColmapBackendTest::missingBatchFileIsUnavailable()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(QDir().mkpath(QDir(directory.path()).filePath(QStringLiteral("bin"))));
    QFile executable(QDir(directory.path()).filePath(QStringLiteral("bin/colmap.exe")));
    QVERIFY(executable.open(QIODevice::WriteOnly));
    executable.close();

    vision3d::ColmapBackend backend;
    const vision3d::BackendProbeResult result = backend.probe(directory.path());
    QVERIFY(!result.available);
    QVERIFY(result.message.contains(QStringLiteral("COLMAP.bat")));
}

void ColmapBackendTest::versionParserFindsExpectedVersion()
{
    QCOMPARE(vision3d::ColmapBackend::parseVersion(
                  QStringLiteral("COLMAP 3.11.1\nUsage: colmap command [options]")),
              QStringLiteral("3.11.1"));
    QCOMPARE(vision3d::ColmapBackend::parseVersion(QStringLiteral("no version here")), QString());
}

} // namespace

QTEST_MAIN(ColmapBackendTest)
#include "test_colmap_backend.moc"
