#include "app/MainWindow.h"

#include "backend/ReconstructionEngineLocator.h"
#include "widgets/BackendPanel.h"

#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

namespace {

class MainWindowTest final : public QObject
{
    Q_OBJECT

private slots:
    void windowStartsAndProbesEngine();
};

void MainWindowTest::windowStartsAndProbesEngine()
{
    QTemporaryDir settingsDirectory;
    QVERIFY(settingsDirectory.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());

    const QString developmentRoot = qEnvironmentVariable("VISION3D_COLMAP_ROOT");
    if (developmentRoot.isEmpty()) {
        QSKIP("External COLMAP probe is opt-in; set VISION3D_COLMAP_ROOT to run it.");
    }
    vision3d::ReconstructionEngineLocator::saveDevelopmentBackendRoot(developmentRoot);

    vision3d::MainWindow window;
    window.show();
    QVERIFY(window.isVisible());

    QPushButton* probeButton = nullptr;
    const QList<QPushButton*> buttons = window.findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button->text() == QStringLiteral("检测重建引擎")) {
            probeButton = button;
            break;
        }
    }
    QVERIFY(probeButton != nullptr);
    vision3d::BackendPanel* backendPanel = window.findChild<vision3d::BackendPanel*>();
    QVERIFY(backendPanel != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(backendPanel->isVerified(), 30000);
    QVERIFY(backendPanel->isGpuAvailable());
    const vision3d::BackendProbeResult& technical = backendPanel->lastResult();
    QCOMPARE(technical.backendName, QStringLiteral("COLMAP"));
    QCOMPARE(technical.version, QStringLiteral("3.11.1"));
    QCOMPARE(QDir::fromNativeSeparators(technical.rootPath),
             QDir::fromNativeSeparators(developmentRoot));
    QVERIFY(QDir::fromNativeSeparators(technical.executablePath)
                .endsWith(QStringLiteral("/bin/colmap.exe"), Qt::CaseInsensitive));

    bool hasReadyStatus = false;
    for (QLabel* label : window.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("就绪")) {
            hasReadyStatus = true;
        }
        QVERIFY(!label->text().contains(QStringLiteral("COLMAP")));
        QVERIFY(!label->text().contains(QStringLiteral("3.11.1")));
        QVERIFY(!label->text().contains(developmentRoot));
    }
    QVERIFY(hasReadyStatus);
    QVERIFY(window.findChild<QLineEdit*>() == nullptr);
    QVERIFY(probeButton->text() == QStringLiteral("检测重建引擎"));
}

} // namespace

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
