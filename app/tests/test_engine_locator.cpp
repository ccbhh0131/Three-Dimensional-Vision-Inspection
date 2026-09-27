#include "backend/ReconstructionEngineLocator.h"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class EngineLocatorTest final : public QObject
{
    Q_OBJECT

private slots:
    void internalRootIsApplicationRelative();
    void savedDevelopmentFallbackRoundtrip();
    void preferredRootUsesInternalBeforeFallback();
};

void EngineLocatorTest::internalRootIsApplicationRelative()
{
    QTemporaryDir applicationDirectory;
    QVERIFY(applicationDirectory.isValid());
    const QString root = vision3d::ReconstructionEngineLocator::internalEngineRoot(
        applicationDirectory.path());
    const QString expected = QDir(applicationDirectory.path()).filePath(
        QStringLiteral("runtime/reconstruction/engine"));
    QCOMPARE(QDir::fromNativeSeparators(root), QDir::fromNativeSeparators(expected));
}

void EngineLocatorTest::savedDevelopmentFallbackRoundtrip()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    const QString fallback = QStringLiteral("developer/reconstruction-engine");
    vision3d::ReconstructionEngineLocator::saveDevelopmentBackendRoot(fallback);
    QCOMPARE(vision3d::ReconstructionEngineLocator::savedDevelopmentBackendRoot(), fallback);

    QSettings settings;
    QCOMPARE(settings.value(QStringLiteral("colmap/backendRoot")).toString(), fallback);
    QCOMPARE(settings.value(QStringLiteral("reconstruction/developmentBackendRoot")).toString(),
             fallback);
}

void EngineLocatorTest::preferredRootUsesInternalBeforeFallback()
{
    QTemporaryDir applicationDirectory;
    QVERIFY(applicationDirectory.isValid());
    const QString fallback = QStringLiteral("developer/reconstruction-engine");

    QCOMPARE(vision3d::ReconstructionEngineLocator::preferredRoot(applicationDirectory.path(),
                                                                    fallback),
             fallback);

    const QString internal =
        vision3d::ReconstructionEngineLocator::internalEngineRoot(applicationDirectory.path());
    QVERIFY(QDir().mkpath(internal));
    QCOMPARE(vision3d::ReconstructionEngineLocator::preferredRoot(applicationDirectory.path(),
                                                                    fallback),
             internal);
    QVERIFY(vision3d::ReconstructionEngineLocator::isInternalRoot(internal,
                                                                   applicationDirectory.path()));
}

} // namespace

QTEST_MAIN(EngineLocatorTest)
#include "test_engine_locator.moc"
