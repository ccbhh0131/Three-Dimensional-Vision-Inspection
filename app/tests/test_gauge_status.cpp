#include "core/device/GaugeAsset.h"
#include "core/device/GaugeStatus.h"

#include <QJsonObject>
#include <QtTest>

namespace {

vision3d::GaugeStatusRule fullRule()
{
    vision3d::GaugeStatusRule rule;
    rule.alarmLow = 0.2;
    rule.warningLow = 0.4;
    rule.warningHigh = 2.0;
    rule.alarmHigh = 2.3;
    return rule;
}

} // namespace

class GaugeStatusTest final : public QObject
{
    Q_OBJECT

private slots:
    void noRule();
    void noReading();
    void normal();
    void lowWarning();
    void highWarning();
    void lowAlarm();
    void highAlarm();
    void exactWarningBoundary();
    void exactAlarmBoundary();
    void partialHighOnlyRule();
    void partialLowOnlyRule();
    void invalidThresholdOrder();
    void outOfGaugeRangeValueNotClamped();
    void ruleJsonRoundTrip();
    void oldAssetWithoutRuleIsCompatible();
};

void GaugeStatusTest::noRule()
{
    const std::optional<double> value = 1.2;
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(value, std::nullopt),
             vision3d::GaugeStatus::Unknown);
}

void GaugeStatusTest::noReading()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(std::nullopt, rule),
             vision3d::GaugeStatus::Unknown);
}

void GaugeStatusTest::normal()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(1.2, *rule),
             vision3d::GaugeStatus::Normal);
}

void GaugeStatusTest::lowWarning()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.3, *rule),
             vision3d::GaugeStatus::Warning);
}

void GaugeStatusTest::highWarning()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.1, *rule),
             vision3d::GaugeStatus::Warning);
}

void GaugeStatusTest::lowAlarm()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.1, *rule),
             vision3d::GaugeStatus::Alarm);
}

void GaugeStatusTest::highAlarm()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.4, *rule),
             vision3d::GaugeStatus::Alarm);
}

void GaugeStatusTest::exactWarningBoundary()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.4, *rule),
             vision3d::GaugeStatus::Normal);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.0, *rule),
             vision3d::GaugeStatus::Normal);
}

void GaugeStatusTest::exactAlarmBoundary()
{
    const std::optional<vision3d::GaugeStatusRule> rule = fullRule();
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.2, *rule),
             vision3d::GaugeStatus::Warning);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.3, *rule),
             vision3d::GaugeStatus::Warning);
}

void GaugeStatusTest::partialHighOnlyRule()
{
    vision3d::GaugeStatusRule rule;
    rule.warningHigh = 2.0;
    rule.alarmHigh = 2.3;
    QVERIFY(rule.isValid(0.0, 2.5));
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(1.0, rule),
             vision3d::GaugeStatus::Normal);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.1, rule),
             vision3d::GaugeStatus::Warning);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(2.4, rule),
             vision3d::GaugeStatus::Alarm);
}

void GaugeStatusTest::partialLowOnlyRule()
{
    vision3d::GaugeStatusRule rule;
    rule.alarmLow = 0.2;
    rule.warningLow = 0.4;
    QVERIFY(rule.isValid(0.0, 2.5));
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.5, rule),
             vision3d::GaugeStatus::Normal);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.3, rule),
             vision3d::GaugeStatus::Warning);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.1, rule),
             vision3d::GaugeStatus::Alarm);
}

void GaugeStatusTest::invalidThresholdOrder()
{
    vision3d::GaugeStatusRule rule;
    rule.alarmLow = 0.5;
    rule.warningLow = 0.4;
    QString error;
    QVERIFY(!rule.isValid(0.0, 2.5, &error));
    QVERIFY(error.contains(QStringLiteral("阈值顺序无效")));
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(0.45, rule),
             vision3d::GaugeStatus::Unknown);
}

void GaugeStatusTest::outOfGaugeRangeValueNotClamped()
{
    const vision3d::GaugeStatusRule rule = fullRule();
    const double value = 2.7;
    QCOMPARE(value, 2.7);
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(value, rule),
             vision3d::GaugeStatus::Alarm);
}

void GaugeStatusTest::ruleJsonRoundTrip()
{
    const vision3d::GaugeStatusRule original = fullRule();
    QString error;
    const std::optional<vision3d::GaugeStatusRule> restored =
        vision3d::GaugeStatusRule::fromJson(original.toJson(), &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(restored->alarmLow.has_value());
    QVERIFY(restored->warningLow.has_value());
    QVERIFY(restored->warningHigh.has_value());
    QVERIFY(restored->alarmHigh.has_value());
    QCOMPARE(*restored->alarmLow, *original.alarmLow);
    QCOMPARE(*restored->warningLow, *original.warningLow);
    QCOMPARE(*restored->warningHigh, *original.warningHigh);
    QCOMPARE(*restored->alarmHigh, *original.alarmHigh);

    vision3d::GaugeAsset asset = vision3d::GaugeAsset::create(
        QStringLiteral("marker-1"), QStringLiteral("压力表"), 0.0, 2.5, QStringLiteral("MPa"));
    asset.statusRule = original;
    const std::optional<vision3d::GaugeAsset> restoredAsset =
        vision3d::GaugeAsset::fromJson(asset.toJson(), &error);
    QVERIFY2(restoredAsset.has_value(), qPrintable(error));
    QVERIFY(restoredAsset->statusRule.has_value());
    QVERIFY(restoredAsset->statusRule->warningHigh.has_value());
    QCOMPARE(*restoredAsset->statusRule->warningHigh, *original.warningHigh);
}

void GaugeStatusTest::oldAssetWithoutRuleIsCompatible()
{
    vision3d::GaugeAsset asset = vision3d::GaugeAsset::create(
        QStringLiteral("marker-legacy"), QStringLiteral("旧表"), 0.0, 2.5, QStringLiteral("MPa"));
    QJsonObject legacy = asset.toJson();
    legacy.remove(QStringLiteral("statusRule"));
    QString error;
    const std::optional<vision3d::GaugeAsset> restored =
        vision3d::GaugeAsset::fromJson(legacy, &error);
    QVERIFY2(restored.has_value(), qPrintable(error));
    QVERIFY(!restored->statusRule.has_value());
    QCOMPARE(vision3d::GaugeStatusEvaluator::evaluate(restored->latestValue,
                                                       restored->statusRule),
             vision3d::GaugeStatus::Unknown);
}

QTEST_MAIN(GaugeStatusTest)
#include "test_gauge_status.moc"
