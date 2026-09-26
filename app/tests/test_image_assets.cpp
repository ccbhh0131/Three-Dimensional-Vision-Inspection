#include "backend/ColmapBackend.h"
#include "core/assets/ThumbnailCache.h"
#include "core/project/ProjectManager.h"
#include "widgets/ImageAssetModel.h"
#include "widgets/ImageBrowserPanel.h"
#include "widgets/ImagePreviewWidget.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QColor>
#include <QListView>
#include <QPixmap>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QString writeImage(const QString& directory,
                   const QString& fileName,
                   const QSize& size = QSize(640, 480),
                   const QColor& color = QColor(QStringLiteral("#2f80ed")))
{
    QDir().mkpath(directory);
    const QString path = QDir(directory).filePath(fileName);
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    return image.save(path, "PNG") ? path : QString();
}

vision3d::AssetRecord makeRecord(const QString& id = QStringLiteral("asset-1"))
{
    vision3d::AssetRecord record;
    record.id = id;
    record.type = QStringLiteral("image");
    record.relativePath = QStringLiteral("images/%1.png").arg(id);
    record.originalFileName = QStringLiteral("source.png");
    record.fileSize = 1234;
    record.width = 640;
    record.height = 480;
    record.sha256 = QString(64, QLatin1Char('a'));
    return record;
}

class ImageAssetsTest final : public QObject
{
    Q_OBJECT

private slots:
    void assetRecordSerializeDeserialize();
    void manifestWithImageAssetsRoundTrip();
    void validPngImportUsingRuntimeGeneratedImage();
    void duplicateSha256IsNotDuplicated();
    void sameFilenameDifferentContentIsAllowed();
    void unsupportedOrBrokenImageIsRejected();
    void thumbnailIsBoundedAndCached();
    void reopenRestoresAssets();
    void missingAssetDoesNotBlockOpen();
    void removeUpdatesManifestAndRemovesFilesAndCache();
    void modelRowCountEqualsManifest();
    void modelRolesAreCorrect();
    void templeRingSmokeWhenConfigured();
    void templeRingPreviewClicksWhenConfigured();
};

void ImageAssetsTest::assetRecordSerializeDeserialize()
{
    const vision3d::AssetRecord original = makeRecord();
    QString error;
    const std::optional<vision3d::AssetRecord> restored =
        vision3d::AssetRecord::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->id, original.id);
    QCOMPARE(restored->type, original.type);
    QCOMPARE(restored->relativePath, original.relativePath);
    QCOMPARE(restored->originalFileName, original.originalFileName);
    QCOMPARE(restored->fileSize, original.fileSize);
    QCOMPARE(restored->width, original.width);
    QCOMPARE(restored->height, original.height);
    QCOMPARE(restored->sha256, original.sha256);
}

void ImageAssetsTest::manifestWithImageAssetsRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManifest original = vision3d::ProjectManifest::createNew(
        QStringLiteral("Assets"));
    original.setImageAssetRecords({makeRecord()});

    const QString path = directory.filePath(QStringLiteral("project.json"));
    QString error;
    QVERIFY2(original.save(path, &error), qPrintable(error));
    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::load(path, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    const QList<vision3d::AssetRecord> records = restored->imageAssetRecords(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().toJson(), makeRecord().toJson());
}

void ImageAssetsTest::validPngImportUsingRuntimeGeneratedImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(directory.path(), QStringLiteral("import"), &error),
             qPrintable(error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("sample.png"));
    QVERIFY(!source.isEmpty());

    const QList<vision3d::AssetImportResult> results = manager.importImages({source}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().status, vision3d::AssetImportStatus::Imported);
    QCOMPARE(manager.imageAssetRecords().size(), 1);
    QVERIFY(QFileInfo(manager.absoluteAssetPath(results.first().asset)).isFile());
}

void ImageAssetsTest::duplicateSha256IsNotDuplicated()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("duplicate"), &error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("first.png"));
    QVERIFY(!source.isEmpty());

    QCOMPARE(manager.importImages({source}, &error).first().status,
             vision3d::AssetImportStatus::Imported);
    const QList<vision3d::AssetImportResult> second = manager.importImages({source}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(second.first().status, vision3d::AssetImportStatus::Duplicate);
    QCOMPARE(manager.imageAssetRecords().size(), 1);
}

void ImageAssetsTest::sameFilenameDifferentContentIsAllowed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("same-name"), &error));
    const QString first = writeImage(directory.filePath(QStringLiteral("source-a")),
                                     QStringLiteral("same.png"),
                                     QSize(640, 480),
                                     QColor(QStringLiteral("#2f80ed")));
    const QString second = writeImage(directory.filePath(QStringLiteral("source-b")),
                                      QStringLiteral("same.png"),
                                      QSize(640, 480),
                                      QColor(QStringLiteral("#eb5757")));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());

    const QList<vision3d::AssetImportResult> results = manager.importImages({first, second}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(results.size(), 2);
    QCOMPARE(results.at(0).status, vision3d::AssetImportStatus::Imported);
    QCOMPARE(results.at(1).status, vision3d::AssetImportStatus::Imported);
    const QList<vision3d::AssetRecord> records = manager.imageAssetRecords();
    QCOMPARE(records.size(), 2);
    QVERIFY(records.at(0).sha256 != records.at(1).sha256);
    QCOMPARE(records.at(0).originalFileName, records.at(1).originalFileName);
}

void ImageAssetsTest::unsupportedOrBrokenImageIsRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("invalid"), &error));

    const QString textPath = directory.filePath(QStringLiteral("not-image.txt"));
    QFile text(textPath);
    QVERIFY(text.open(QIODevice::WriteOnly));
    QVERIFY(text.write("not an image") > 0);
    text.close();

    const QString brokenPath = directory.filePath(QStringLiteral("broken.png"));
    QFile broken(brokenPath);
    QVERIFY(broken.open(QIODevice::WriteOnly));
    QVERIFY(broken.write("broken png") > 0);
    broken.close();

    const QList<vision3d::AssetImportResult> results =
        manager.importImages({textPath, brokenPath}, &error);
    QCOMPARE(results.size(), 2);
    QCOMPARE(results.at(0).status, vision3d::AssetImportStatus::Failed);
    QCOMPARE(results.at(1).status, vision3d::AssetImportStatus::Failed);
    QCOMPARE(manager.imageAssetRecords().size(), 0);
}

void ImageAssetsTest::thumbnailIsBoundedAndCached()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("thumb"), &error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("large.png"),
                                      QSize(1920, 1080));
    QVERIFY(!source.isEmpty());
    const vision3d::AssetImportResult result = manager.importImages({source}, &error).first();
    QCOMPARE(result.status, vision3d::AssetImportStatus::Imported);

    const QImage thumbnail = vision3d::ThumbnailCache::loadOrCreate(
        result.asset, manager.projectDirectory());
    QVERIFY(!thumbnail.isNull());
    QVERIFY(thumbnail.width() <= vision3d::ThumbnailCache::MaxThumbnailEdge);
    QVERIFY(thumbnail.height() <= vision3d::ThumbnailCache::MaxThumbnailEdge);
    QVERIFY(QFileInfo(vision3d::ThumbnailCache::cachePath(
                         manager.projectDirectory(), result.asset.id))
                .isFile());
}

void ImageAssetsTest::reopenRestoresAssets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("reopen"), &error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("restore.png"));
    QVERIFY(!source.isEmpty());
    QCOMPARE(manager.importImages({source}, &error).first().status,
             vision3d::AssetImportStatus::Imported);

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    QCOMPARE(reopened.imageAssetRecords().size(), 1);
    QCOMPARE(reopened.imageAssetRecords().first().originalFileName,
             QStringLiteral("restore.png"));
}

void ImageAssetsTest::missingAssetDoesNotBlockOpen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("missing"), &error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("missing.png"));
    QVERIFY(!source.isEmpty());
    const vision3d::AssetImportResult result = manager.importImages({source}, &error).first();
    QCOMPARE(result.status, vision3d::AssetImportStatus::Imported);
    QVERIFY(QFile::remove(manager.absoluteAssetPath(result.asset)));

    vision3d::ProjectManager reopened;
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    QCOMPARE(reopened.imageAssetRecords().size(), 1);

    vision3d::ImageAssetModel model;
    model.setProject(*reopened.currentManifest(), reopened.projectDirectory());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), vision3d::ImageAssetModel::ExistsRole).toBool(),
             false);
}

void ImageAssetsTest::removeUpdatesManifestAndRemovesFilesAndCache()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("remove"), &error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("remove.png"));
    QVERIFY(!source.isEmpty());
    const vision3d::AssetImportResult result = manager.importImages({source}, &error).first();
    QCOMPARE(result.status, vision3d::AssetImportStatus::Imported);
    const QString internalPath = manager.absoluteAssetPath(result.asset);
    const QString cachePath = vision3d::ThumbnailCache::cachePath(
        manager.projectDirectory(), result.asset.id);
    QVERIFY(!vision3d::ThumbnailCache::loadOrCreate(result.asset, manager.projectDirectory()).isNull());
    QVERIFY(QFileInfo(cachePath).isFile());

    QVERIFY2(manager.removeAsset(result.asset.id, &error), qPrintable(error));
    QCOMPARE(manager.imageAssetRecords().size(), 0);
    QVERIFY(!QFileInfo::exists(internalPath));
    QVERIFY(!QFileInfo::exists(cachePath));

    const std::optional<vision3d::ProjectManifest> restored =
        vision3d::ProjectManifest::load(QDir(manager.projectDirectory()).filePath(
                                             QStringLiteral("project.json")),
                                         &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->imageAssetRecords().size(), 0);
}

void ImageAssetsTest::modelRowCountEqualsManifest()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("model"), &error));
    const QString sourceA = writeImage(directory.filePath(QStringLiteral("sources")),
                                       QStringLiteral("a.png"));
    const QString sourceB = writeImage(directory.filePath(QStringLiteral("sources")),
                                       QStringLiteral("b.png"),
                                       QSize(320, 240),
                                       QColor(QStringLiteral("#27ae60")));
    QVERIFY(!sourceA.isEmpty());
    QVERIFY(!sourceB.isEmpty());
    manager.importImages({sourceA, sourceB}, &error);

    vision3d::ImageAssetModel model;
    model.setProject(*manager.currentManifest(), manager.projectDirectory());
    QCOMPARE(model.rowCount(), manager.imageAssetRecords().size());
}

void ImageAssetsTest::modelRolesAreCorrect()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    vision3d::ProjectManager manager;
    QString error;
    QVERIFY(manager.newProject(directory.path(), QStringLiteral("roles"), &error));
    const QString source = writeImage(directory.filePath(QStringLiteral("sources")),
                                      QStringLiteral("roles.png"),
                                      QSize(320, 240));
    QVERIFY(!source.isEmpty());
    const vision3d::AssetImportResult result = manager.importImages({source}, &error).first();
    QCOMPARE(result.status, vision3d::AssetImportStatus::Imported);

    vision3d::ImageAssetModel model;
    model.setProject(*manager.currentManifest(), manager.projectDirectory());
    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, Qt::DisplayRole).toString(), QStringLiteral("roles.png"));
    QCOMPARE(model.data(index, vision3d::ImageAssetModel::AssetIdRole).toString(), result.asset.id);
    QCOMPARE(model.data(index, vision3d::ImageAssetModel::RelativePathRole).toString(),
             result.asset.relativePath);
    QCOMPARE(model.data(index, vision3d::ImageAssetModel::WidthRole).toInt(), 320);
    QCOMPARE(model.data(index, vision3d::ImageAssetModel::HeightRole).toInt(), 240);
    QCOMPARE(model.data(index, vision3d::ImageAssetModel::ExistsRole).toBool(), true);
    QVERIFY(!model.data(index, Qt::DecorationRole).value<QPixmap>().isNull());
}

void ImageAssetsTest::templeRingSmokeWhenConfigured()
{
    const QString inputDirectory = qEnvironmentVariable("VISION3D_STAGE2_TEMPLE_DIR");
    if (inputDirectory.isEmpty()) {
        QSKIP("TempleRing smoke is opt-in; set VISION3D_STAGE2_TEMPLE_DIR to run it.");
    }

    const QString workspaceParent = qEnvironmentVariable("VISION3D_STAGE2_WORKSPACE_PARENT",
                                                          QDir(QDir::tempPath()).filePath(
                                                              QStringLiteral("vision3d_stage2_workspace")));
    const QString projectName = QStringLiteral("TempleImageDemo");
    const QString projectDirectory = QDir(workspaceParent).filePath(projectName);
    if (QFileInfo::exists(QDir(projectDirectory).filePath(QStringLiteral("project.json")))) {
        QSKIP("TempleRing smoke workspace already exists; refusing to overwrite it.");
    }
    QVERIFY(QDir().mkpath(workspaceParent));

    const QFileInfoList imageInfos = QDir(inputDirectory).entryInfoList(
        QStringList() << QStringLiteral("*.png") << QStringLiteral("*.PNG"),
        QDir::Files,
        QDir::Name);
    QCOMPARE(imageInfos.size(), 47);
    QStringList sourceFiles;
    for (const QFileInfo& info : imageInfos) {
        sourceFiles.append(info.absoluteFilePath());
    }

    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.newProject(workspaceParent, projectName, &error), qPrintable(error));

    QElapsedTimer importTimer;
    importTimer.start();
    const QList<vision3d::AssetImportResult> importResults =
        manager.importImages(sourceFiles, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    int imported = 0;
    int duplicates = 0;
    int failed = 0;
    for (const vision3d::AssetImportResult& result : importResults) {
        imported += result.status == vision3d::AssetImportStatus::Imported ? 1 : 0;
        duplicates += result.status == vision3d::AssetImportStatus::Duplicate ? 1 : 0;
        failed += result.status == vision3d::AssetImportStatus::Failed ? 1 : 0;
    }
    QCOMPARE(imported, 47);
    QCOMPARE(duplicates, 0);
    QCOMPARE(failed, 0);
    const qint64 importElapsed = importTimer.elapsed();
    QCOMPARE(manager.imageAssetRecords().size(), 47);

    vision3d::ImageBrowserPanel browser;
    vision3d::ImagePreviewWidget preview;
    QObject::connect(&browser,
                     &vision3d::ImageBrowserPanel::assetSelected,
                     &preview,
                     [&manager, &preview](const QString& assetId) {
                         const std::optional<vision3d::AssetRecord> asset =
                             manager.assetById(assetId);
                         if (asset.has_value()) {
                             preview.showAsset(*asset, manager.projectDirectory());
                         }
                     });
    browser.setProject(*manager.currentManifest(), manager.projectDirectory());
    QCOMPARE(browser.model()->rowCount(), 47);

    for (int row = 0; row < 5; ++row) {
        browser.view()->setCurrentIndex(browser.model()->index(row, 0));
        QApplication::processEvents();
        const std::optional<vision3d::AssetRecord> asset = browser.model()->assetAt(
            browser.model()->index(row, 0));
        QVERIFY(asset.has_value());
        QCOMPARE(preview.currentAssetId(), asset->id);
        QVERIFY(preview.hasLoadedImage());
    }

    qint64 cacheBytes = 0;
    int cacheFiles = 0;
    for (const vision3d::AssetRecord& asset : manager.imageAssetRecords()) {
        const QImage thumbnail = vision3d::ThumbnailCache::loadOrCreate(
            asset, manager.projectDirectory());
        QVERIFY(!thumbnail.isNull());
        QVERIFY(thumbnail.width() <= vision3d::ThumbnailCache::MaxThumbnailEdge);
        QVERIFY(thumbnail.height() <= vision3d::ThumbnailCache::MaxThumbnailEdge);
        const QFileInfo cacheInfo(vision3d::ThumbnailCache::cachePath(
            manager.projectDirectory(), asset.id));
        if (cacheInfo.isFile()) {
            ++cacheFiles;
            cacheBytes += cacheInfo.size();
        }
    }
    QCOMPARE(cacheFiles, 47);

    const qint64 projectJsonBytes = QFileInfo(
        QDir(manager.projectDirectory()).filePath(QStringLiteral("project.json"))).size();
    vision3d::ProjectManager reopened;
    QElapsedTimer reopenTimer;
    reopenTimer.start();
    QVERIFY2(reopened.openProject(manager.projectDirectory(), &error), qPrintable(error));
    const qint64 reopenElapsed = reopenTimer.elapsed();
    QCOMPARE(reopened.imageAssetRecords().size(), 47);

    const vision3d::AssetRecord removedAsset = reopened.imageAssetRecords().first();
    const QString removedSource = sourceFiles.first();
    QVERIFY2(reopened.removeAsset(removedAsset.id, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(removedSource));
    const QList<vision3d::AssetImportResult> reimportResults =
        reopened.importImages({removedSource}, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(reimportResults.size(), 1);
    QCOMPARE(reimportResults.first().status, vision3d::AssetImportStatus::Imported);
    QCOMPARE(reopened.imageAssetRecords().size(), 47);

    const QString colmapRoot = qEnvironmentVariable("VISION3D_COLMAP_ROOT");
    if (colmapRoot.isEmpty()) {
        QSKIP("Set VISION3D_COLMAP_ROOT to run the configured COLMAP portion of this opt-in smoke.");
    }
    vision3d::ColmapBackend backend;
    const vision3d::BackendProbeResult backendResult = backend.probe(colmapRoot);
    QVERIFY2(backendResult.available, qPrintable(backendResult.message));
    QCOMPARE(backendResult.version, QStringLiteral("3.11.1"));

    qInfo().noquote() << QStringLiteral("TEMPLE_SMOKE imported=47 duplicates=0 failed=0 "
                                       "import_elapsed_ms=%1")
                             .arg(importElapsed);
    qInfo().noquote() << QStringLiteral("TEMPLE_SMOKE reopen_elapsed_ms=%1 "
                                       "cache_files=%2 cache_bytes=%3 project_json_bytes=%4")
                             .arg(reopenElapsed)
                             .arg(cacheFiles)
                             .arg(cacheBytes)
                             .arg(projectJsonBytes);
}

void ImageAssetsTest::templeRingPreviewClicksWhenConfigured()
{
    const QString inputDirectory = qEnvironmentVariable("VISION3D_STAGE2_TEMPLE_DIR");
    if (inputDirectory.isEmpty()) {
        QSKIP("TempleRing preview click smoke is opt-in.");
    }

    const QString workspaceParent = qEnvironmentVariable("VISION3D_STAGE2_WORKSPACE_PARENT",
                                                          QDir(QDir::tempPath()).filePath(
                                                              QStringLiteral("vision3d_stage2_workspace")));
    const QString projectDirectory = QDir(workspaceParent).filePath(
        QStringLiteral("TempleImageDemo"));
    if (!QFileInfo::exists(QDir(projectDirectory).filePath(QStringLiteral("project.json")))) {
        QSKIP("Run the TempleRing import smoke first.");
    }

    vision3d::ProjectManager manager;
    QString error;
    QVERIFY2(manager.openProject(projectDirectory, &error), qPrintable(error));
    QCOMPARE(manager.imageAssetRecords().size(), 47);

    vision3d::ImageBrowserPanel browser;
    vision3d::ImagePreviewWidget preview;
    browser.resize(700, 420);
    preview.resize(700, 420);
    browser.show();
    preview.show();
    browser.setProject(*manager.currentManifest(), manager.projectDirectory());
    QObject::connect(&browser,
                     &vision3d::ImageBrowserPanel::assetSelected,
                     &preview,
                     [&manager, &preview](const QString& assetId) {
                         const std::optional<vision3d::AssetRecord> asset =
                             manager.assetById(assetId);
                         if (asset.has_value()) {
                             preview.showAsset(*asset, manager.projectDirectory());
                         }
                     });
    QApplication::processEvents();

    int clicked = 0;
    for (int row = 0; row < 5; ++row) {
        const QModelIndex index = browser.model()->index(row, 0);
        const QRect itemRect = browser.view()->visualRect(index);
        QVERIFY(itemRect.isValid());
        // visualRect() is expressed in viewport coordinates, so send the click to the viewport.
        QTest::mouseClick(browser.view()->viewport(),
                          Qt::LeftButton,
                          Qt::NoModifier,
                          itemRect.center());
        QApplication::processEvents();
        const std::optional<vision3d::AssetRecord> asset = browser.model()->assetAt(index);
        QVERIFY(asset.has_value());
        QCOMPARE(browser.view()->currentIndex().row(), row);
        QCOMPARE(browser.selectedAssetId(), asset->id);
        QCOMPARE(preview.currentAssetId(), asset->id);
        QVERIFY(preview.hasLoadedImage());
        ++clicked;
    }
    QCOMPARE(clicked, 5);
    browser.close();
    preview.close();
}

} // namespace

QTEST_MAIN(ImageAssetsTest)
#include "test_image_assets.moc"
