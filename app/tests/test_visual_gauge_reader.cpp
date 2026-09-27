#include "core/device/GaugeProfile.h"
#include "core/vision/VisualGaugeReader.h"

#include <QImage>
#include <QImageReader>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QProcessEnvironment>
#include <QtTest>

#include <cmath>

namespace {

QImage syntheticDial(double angleDegrees)
{
    QImage image(320, 320, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF center(160.0, 160.0);
    painter.setPen(QPen(Qt::black, 5.0));
    painter.drawEllipse(center, 116.0, 116.0);
    const double radians = angleDegrees * 3.14159265358979323846 / 180.0;
    const QPointF tip(center.x() + std::cos(radians) * 94.0,
                      center.y() - std::sin(radians) * 94.0);
    painter.setPen(QPen(QColor(30, 30, 30), 8.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, tip);
    painter.setBrush(Qt::black);
    painter.drawEllipse(center, 10.0, 10.0);
    painter.end();
    return image;
}

class VisualGaugeReaderTest final : public QObject
{
    Q_OBJECT

private slots:
    void horizontalReading();
    void verticalReading();
    void diagonalReading();
    void wrapNearBoundaryReading();
    void outOfSweepFails();
    void optionalRealImageReading();
};

bool verifyReading(double angleDegrees, double expectedValue)
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const vision3d::VisualGaugeReadingResult result =
        vision3d::VisualGaugeReader::read(syntheticDial(angleDegrees),
                                          profile,
                                          QRect(0, 0, 320, 320));
    if (!result.success) {
        qWarning().noquote() << result.failureReason;
        return false;
    }
    return !result.diagnosticOverlay.isNull()
        && (result.centerSource == vision3d::GaugeCenterSource::Circle
            || result.centerSource == vision3d::GaugeCenterSource::RoiCenterFallback)
        && std::hypot(result.center.x() - 160.0, result.center.y() - 160.0) < 10.0
        && std::abs(result.needleAngleDegrees - angleDegrees) < 8.0
        && std::abs(result.value - expectedValue) < 0.12
        && result.confidence > 0.0;
}

void VisualGaugeReaderTest::horizontalReading()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const std::optional<double> expected = profile.valueForAngle(0.0);
    QVERIFY(expected.has_value());
    QVERIFY(verifyReading(0.0, *expected));
}

void VisualGaugeReaderTest::verticalReading()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const std::optional<double> expected = profile.valueForAngle(90.0);
    QVERIFY(expected.has_value());
    QVERIFY(verifyReading(90.0, *expected));
}

void VisualGaugeReaderTest::diagonalReading()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const std::optional<double> expected = profile.valueForAngle(135.0);
    QVERIFY(expected.has_value());
    QVERIFY(verifyReading(135.0, *expected));
}

void VisualGaugeReaderTest::wrapNearBoundaryReading()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const std::optional<double> expected = profile.valueForAngle(173.5);
    QVERIFY(expected.has_value());
    QVERIFY(verifyReading(173.5, *expected));
}

void VisualGaugeReaderTest::outOfSweepFails()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const vision3d::VisualGaugeReadingResult result =
        vision3d::VisualGaugeReader::read(syntheticDial(-100.0),
                                          profile,
                                          QRect(0, 0, 320, 320));
    QVERIFY(!result.success);
    QVERIFY(result.failureReason.contains(QStringLiteral("超出")));
    QVERIFY(!result.diagnosticOverlay.isNull());
}

void VisualGaugeReaderTest::optionalRealImageReading()
{
    const QString imagePath = qEnvironmentVariable(
        "VISION3DINSPECTOR_STAGE4D_REAL_IMAGE").trimmed();
    if (imagePath.isEmpty()) {
        QSKIP("VISION3DINSPECTOR_STAGE4D_REAL_IMAGE is not set");
    }
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));

    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    QRect roi(QPoint(0, 0), image.size());
    const QStringList roiParts = qEnvironmentVariable(
        "VISION3DINSPECTOR_STAGE4D_REAL_ROI").trimmed().split(QLatin1Char(','));
    if (roiParts.size() == 4) {
        bool okX = false;
        bool okY = false;
        bool okWidth = false;
        bool okHeight = false;
        const int x = roiParts.at(0).toInt(&okX);
        const int y = roiParts.at(1).toInt(&okY);
        const int width = roiParts.at(2).toInt(&okWidth);
        const int height = roiParts.at(3).toInt(&okHeight);
        if (okX && okY && okWidth && okHeight && width > 0 && height > 0) {
            roi = QRect(x, y, width, height);
        }
    }
    const vision3d::VisualGaugeReadingResult result =
        vision3d::VisualGaugeReader::read(image,
                                          profile,
                                          roi);
    const QString captureDirectory = qEnvironmentVariable(
        "VISION3DINSPECTOR_STAGE4D_REAL_CAPTURE_DIR").trimmed();
    if (!captureDirectory.isEmpty() && !result.diagnosticOverlay.isNull()) {
        QDir().mkpath(captureDirectory);
        result.diagnosticOverlay.save(
            QDir(captureDirectory).filePath(QFileInfo(imagePath).completeBaseName()
                                            + QStringLiteral("_visual_reader_overlay.png")));
    }
    QVERIFY2(result.success, qPrintable(result.failureReason));
    QVERIFY(!result.diagnosticOverlay.isNull());
    qInfo().noquote() << QStringLiteral("real_image=%1 roi=%2,%3,%4,%5 size=%6x%7 angle=%8 value=%9 confidence=%10")
                             .arg(imagePath)
                             .arg(roi.x())
                             .arg(roi.y())
                             .arg(roi.width())
                             .arg(roi.height())
                             .arg(image.width())
                             .arg(image.height())
                             .arg(result.needleAngleDegrees, 0, 'f', 3)
                             .arg(result.value, 0, 'f', 6)
                             .arg(result.confidence, 0, 'f', 3);
}

} // namespace

QTEST_MAIN(VisualGaugeReaderTest)
#include "test_visual_gauge_reader.moc"
