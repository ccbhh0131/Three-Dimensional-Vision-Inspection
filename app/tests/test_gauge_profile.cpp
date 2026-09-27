#include "core/device/GaugeProfile.h"

#include <QJsonDocument>
#include <QtTest>

#include <cmath>

namespace {

class GaugeProfileTest final : public QObject
{
    Q_OBJECT

private slots:
    void exactCalibrationPoints();
    void interpolation();
    void counterClockwiseSweep();
    void wrapAcross180();
    void outOfSweepRejected();
    void rangePreserved();
    void invalidProfileRejected();
    void profileJsonRoundTrip();
};

void GaugeProfileTest::exactCalibrationPoints()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    const QVector<double> expectedAngles{-132.5, 173.5, 117.0, 64.5, 11.2, -43.0};
    const QVector<double> expectedValues{0.0, 0.5, 1.0, 1.5, 2.0, 2.5};
    for (qsizetype index = 0; index < expectedValues.size(); ++index) {
        QString error;
        const std::optional<double> angle =
            profile.angleForValue(expectedValues.at(index), &error);
        QVERIFY2(angle.has_value(), qPrintable(error));
        QVERIFY(std::abs(*angle - expectedAngles.at(index)) < 1.0e-6);

        const std::optional<double> value = profile.valueForAngle(expectedAngles.at(index), &error);
        QVERIFY2(value.has_value(), qPrintable(error));
        QVERIFY(std::abs(*value - expectedValues.at(index)) < 1.0e-6);
    }
}

void GaugeProfileTest::interpolation()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    QString error;
    const std::optional<double> angle = profile.angleForValue(1.25, &error);
    QVERIFY2(angle.has_value(), qPrintable(error));
    QVERIFY(std::abs(*angle - 90.75) < 1.0e-6);
    const std::optional<double> value = profile.valueForAngle(90.75, &error);
    QVERIFY2(value.has_value(), qPrintable(error));
    QVERIFY(std::abs(*value - 1.25) < 1.0e-6);
}

void GaugeProfileTest::counterClockwiseSweep()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    QString error;
    const QVector<double> sweep = profile.unwrappedCalibrationAngles(&error);
    QVERIFY2(sweep.size() == 6, qPrintable(error));
    for (qsizetype index = 1; index < sweep.size(); ++index) {
        QVERIFY(sweep.at(index) < sweep.at(index - 1));
    }
    QCOMPARE(profile.direction, vision3d::GaugeDirection::CounterClockwise);
}

void GaugeProfileTest::wrapAcross180()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    QString error;
    const QVector<double> sweep = profile.unwrappedCalibrationAngles(&error);
    QVERIFY2(sweep.size() == 6, qPrintable(error));
    QVERIFY(std::abs(sweep.at(0) - (-132.5)) < 1.0e-6);
    QVERIFY(std::abs(sweep.at(1) - (-186.5)) < 1.0e-6);
    QVERIFY(std::abs(sweep.at(5) - (-403.0)) < 1.0e-6);

    const std::optional<double> wrappedValue = profile.valueForAngle(173.5, &error);
    QVERIFY2(wrappedValue.has_value(), qPrintable(error));
    QVERIFY(std::abs(*wrappedValue - 0.5) < 1.0e-6);
}

void GaugeProfileTest::outOfSweepRejected()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    QString error;
    const std::optional<double> value = profile.valueForAngle(-100.0, &error);
    QVERIFY(!value.has_value());
    QVERIFY(error.contains(QStringLiteral("超出")));
}

void GaugeProfileTest::rangePreserved()
{
    const vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    QCOMPARE(profile.rangeMin, 0.0);
    QCOMPARE(profile.rangeMax, 2.5);
    QCOMPARE(profile.unit, QStringLiteral("MPa"));
    QCOMPARE(profile.id, QStringLiteral("pressure_0_2_5_mpa"));
}

void GaugeProfileTest::invalidProfileRejected()
{
    vision3d::GaugeProfile profile = vision3d::GaugeProfile::pressure025Mpa();
    profile.calibrationPoints.removeLast();
    QString error;
    QVERIFY(!profile.isValid(&error));
    QVERIFY(!error.isEmpty());

    profile = vision3d::GaugeProfile::pressure025Mpa();
    profile.calibrationPoints[2].value = profile.calibrationPoints[1].value;
    QVERIFY(!profile.isValid(&error));
}

void GaugeProfileTest::profileJsonRoundTrip()
{
    const vision3d::GaugeProfile original = vision3d::GaugeProfile::pressure025Mpa();
    QString error;
    const std::optional<vision3d::GaugeProfile> restored =
        vision3d::GaugeProfile::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QCOMPARE(restored->id, original.id);
    QCOMPARE(restored->name, original.name);
    QCOMPARE(restored->unit, original.unit);
    QCOMPARE(restored->rangeMin, original.rangeMin);
    QCOMPARE(restored->rangeMax, original.rangeMax);
    QCOMPARE(restored->direction, original.direction);
    QCOMPARE(restored->calibrationPoints.size(), original.calibrationPoints.size());
    QCOMPARE(QJsonDocument(restored->toJson()).toJson(QJsonDocument::Compact),
             QJsonDocument(original.toJson()).toJson(QJsonDocument::Compact));
}

} // namespace

QTEST_MAIN(GaugeProfileTest)
#include "test_gauge_profile.moc"
