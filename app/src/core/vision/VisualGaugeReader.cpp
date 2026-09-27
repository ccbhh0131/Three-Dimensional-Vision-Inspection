#include "core/vision/VisualGaugeReader.h"

#include <QImageReader>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace vision3d {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct LineCandidate
{
    cv::Point2f nearPoint;
    cv::Point2f farPoint;
    double length = 0.0;
    double nearDistance = 0.0;
    double farDistance = 0.0;
    double lineDistance = 0.0;
    double angleDegrees = 0.0;
    double score = 0.0;
    double uniqueness = 1.0;
    double radialSupport = 0.0;
};

struct SearchParameters
{
    int pass = 1;
    double cannyLow = 50.0;
    double cannyHigh = 150.0;
    double houghThresholdRatio = 0.07;
    double minimumLineLengthRatio = 0.16;
    double maximumLineGapRatio = 0.04;
    double nearDistanceRatio = 0.34;
    double farDistanceRatio = 0.32;
    double minimumCandidateLengthRatio = 0.12;
};

double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double distance(const cv::Point2f& left, const cv::Point2f& right)
{
    return std::hypot(static_cast<double>(left.x - right.x),
                      static_cast<double>(left.y - right.y));
}

double radialNeedleSupport(const cv::Mat& gray,
                           const cv::Point2f& center,
                           const cv::Point2f& farPoint,
                           double radius)
{
    if (gray.empty() || radius <= 0.0) {
        return 0.0;
    }
    const cv::Point2f vector = farPoint - center;
    const double vectorLength = std::hypot(static_cast<double>(vector.x),
                                           static_cast<double>(vector.y));
    if (vectorLength <= 1.0) {
        return 0.0;
    }
    const cv::Point2f direction(static_cast<float>(vector.x / vectorLength),
                                static_cast<float>(vector.y / vectorLength));
    const cv::Point2f perpendicular(-direction.y, direction.x);
    const double meanIntensity = cv::mean(gray)[0];
    const double darkThreshold = std::clamp(meanIntensity * 0.72, 70.0, 160.0);
    const double halfWidth = std::clamp(radius * 0.018, 2.0, 14.0);
    double weightedSupport = 0.0;
    double totalWeight = 0.0;
    int currentRun = 0;
    int longestRun = 0;
    constexpr int kSamples = 32;
    for (int index = 0; index < kSamples; ++index) {
        const double fraction = static_cast<double>(index) / static_cast<double>(kSamples - 1);
        const double radialDistance = radius * (0.20 + 0.62 * fraction);
        const cv::Point2f sample = center + direction * static_cast<float>(radialDistance);
        const cv::Point2f left = sample - perpendicular * static_cast<float>(halfWidth);
        const cv::Point2f right = sample + perpendicular * static_cast<float>(halfWidth);
        const cv::Point2f middle = sample;
        const auto sampleIntensity = [&gray](const cv::Point2f& point) {
            const int x = std::clamp(static_cast<int>(std::lround(point.x)), 0, gray.cols - 1);
            const int y = std::clamp(static_cast<int>(std::lround(point.y)), 0, gray.rows - 1);
            return static_cast<double>(gray.at<uchar>(y, x));
        };
        const double leftIntensity = sampleIntensity(left);
        const double middleIntensity = sampleIntensity(middle);
        const double rightIntensity = sampleIntensity(right);
        const double darkest = std::min({leftIntensity, middleIntensity, rightIntensity});
        const double sideAverage = (leftIntensity + rightIntensity) * 0.5;
        const bool supported = darkest <= darkThreshold
            || sideAverage - darkest >= std::max(10.0, meanIntensity * 0.08);
        const double weight = 0.5 + fraction;
        weightedSupport += supported ? weight : 0.0;
        totalWeight += weight;
        currentRun = supported ? currentRun + 1 : 0;
        longestRun = std::max(longestRun, currentRun);
    }
    if (totalWeight <= 0.0) {
        return 0.0;
    }
    const double weightedFraction = weightedSupport / totalWeight;
    const double continuityFraction = static_cast<double>(longestRun)
        / static_cast<double>(kSamples);
    return 0.55 * weightedFraction + 0.45 * continuityFraction;
}

std::optional<LineCandidate> bestRadialScanCandidate(const cv::Mat& gray,
                                                     const cv::Point2f& center,
                                                     double radius,
                                                     const GaugeProfile& profile)
{
    double bestSupport = 0.0;
    double bestAngle = 0.0;
    for (int angle = -180; angle < 180; ++angle) {
        const double angleDegrees = static_cast<double>(angle);
        if (!profile.valueForAngle(angleDegrees, nullptr).has_value()) {
            continue;
        }
        const double radians = angleDegrees * kPi / 180.0;
        const cv::Point2f farPoint(
            center.x + static_cast<float>(std::cos(radians) * radius * 0.90),
            center.y - static_cast<float>(std::sin(radians) * radius * 0.90));
        const double support = radialNeedleSupport(gray, center, farPoint, radius);
        if (support > bestSupport) {
            bestSupport = support;
            bestAngle = angleDegrees;
        }
    }
    if (bestSupport < 0.55) {
        return std::nullopt;
    }
    const double radians = bestAngle * kPi / 180.0;
    LineCandidate candidate;
    candidate.nearPoint = cv::Point2f(
        center.x + static_cast<float>(std::cos(radians) * radius * 0.18),
        center.y - static_cast<float>(std::sin(radians) * radius * 0.18));
    candidate.farPoint = cv::Point2f(
        center.x + static_cast<float>(std::cos(radians) * radius * 0.90),
        center.y - static_cast<float>(std::sin(radians) * radius * 0.90));
    candidate.length = radius * 0.72;
    candidate.nearDistance = radius * 0.18;
    candidate.farDistance = radius * 0.90;
    candidate.lineDistance = 0.0;
    candidate.angleDegrees = bestAngle;
    candidate.radialSupport = bestSupport;
    return candidate;
}

double angleDifferenceDegrees(double left, double right)
{
    double difference = std::fmod(std::abs(left - right), 360.0);
    if (difference > 180.0) {
        difference = 360.0 - difference;
    }
    return difference;
}

cv::Mat qImageToBgr(const QImage& image)
{
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat rgbaMat(rgba.height(),
                    rgba.width(),
                    CV_8UC4,
                    const_cast<uchar*>(rgba.constBits()),
                    static_cast<size_t>(rgba.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(rgbaMat, bgr, cv::COLOR_RGBA2BGR);
    return bgr.clone();
}

QImage bgrToQImage(const cv::Mat& bgr)
{
    if (bgr.empty()) {
        return {};
    }
    cv::Mat rgba;
    cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);
    const QImage view(rgba.data,
                      rgba.cols,
                      rgba.rows,
                      static_cast<int>(rgba.step),
                      QImage::Format_RGBA8888);
    return view.copy();
}

VisualGaugeReadingResult failureResult(const QImage& image, const QString& reason)
{
    VisualGaugeReadingResult result;
    result.failureReason = reason;
    result.diagnosticOverlay = image;
    return result;
}

void drawOverlay(cv::Mat* image,
                 const QRect& roi,
                 const cv::Point2f* center,
                 const cv::Point2f* needleTip,
                 double circleRadius,
                 GaugeCenterSource centerSource,
                 double angleDegrees,
                 const std::optional<double>& value,
                 double confidence,
                 int searchPass)
{
    if (image == nullptr || image->empty()) {
        return;
    }
    cv::rectangle(*image,
                  cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
                  cv::Scalar(0, 220, 255),
                  2,
                  cv::LINE_AA);
    if (center != nullptr) {
        if (circleRadius > 0.0) {
            cv::circle(*image,
                       *center,
                       static_cast<int>(std::round(circleRadius)),
                       cv::Scalar(255, 180, 0),
                       2,
                       cv::LINE_AA);
        }
        cv::drawMarker(*image,
                       *center,
                       cv::Scalar(0, 255, 0),
                       cv::MARKER_CROSS,
                       18,
                       2,
                       cv::LINE_AA);
    }
    if (center != nullptr && needleTip != nullptr) {
        cv::line(*image, *center, *needleTip, cv::Scalar(0, 0, 255), 3, cv::LINE_AA);
        cv::circle(*image, *needleTip, 6, cv::Scalar(255, 0, 255), cv::FILLED, cv::LINE_AA);
    }

    const QString source = gaugeCenterSourceToString(centerSource);
    const QString valueText = value.has_value()
        ? QStringLiteral("value=%1").arg(*value, 0, 'f', 3)
        : QStringLiteral("value=FAILED");
    const QString angleText = QStringLiteral("angle=%1 deg")
                                  .arg(angleDegrees, 0, 'f', 1);
    const QString confidenceText = QStringLiteral("confidence=%1")
                                       .arg(confidence, 0, 'f', 2);
    const QString searchPassText = QStringLiteral("searchPass=%1").arg(searchPass);
    const std::vector<QString> labels{
        QStringLiteral("Visual Gauge Reader"),
        QStringLiteral("center=%1").arg(source),
        angleText,
        valueText,
        confidenceText,
        searchPassText,
    };
    int y = std::max(24, roi.y() + 24);
    for (const QString& label : labels) {
        cv::putText(*image,
                    label.toStdString(),
                    cv::Point(std::max(4, roi.x() + 8), y),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.55,
                    cv::Scalar(30, 30, 30),
                    3,
                    cv::LINE_AA);
        cv::putText(*image,
                    label.toStdString(),
                    cv::Point(std::max(4, roi.x() + 8), y),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.55,
                    cv::Scalar(255, 255, 255),
                    1,
                    cv::LINE_AA);
        y += 22;
    }
}

} // namespace

QString gaugeCenterSourceToString(GaugeCenterSource source)
{
    switch (source) {
    case GaugeCenterSource::None:
        return QStringLiteral("None");
    case GaugeCenterSource::Circle:
        return QStringLiteral("Circle");
    case GaugeCenterSource::RoiCenterFallback:
        return QStringLiteral("RoiCenterFallback");
    }
    return QStringLiteral("None");
}

QString gaugeCenterSourceDisplayName(GaugeCenterSource source)
{
    switch (source) {
    case GaugeCenterSource::None:
        return QStringLiteral("未检测");
    case GaugeCenterSource::Circle:
        return QStringLiteral("Hough 圆");
    case GaugeCenterSource::RoiCenterFallback:
        return QStringLiteral("ROI 中心回退");
    }
    return QStringLiteral("未检测");
}

VisualGaugeReadingResult VisualGaugeReader::read(const QImage& image,
                                                 const GaugeProfile& profile,
                                                 const QRect& roi)
{
    QString profileError;
    if (!profile.isValid(&profileError)) {
        return failureResult(image,
                             QStringLiteral("GaugeProfile invalid: %1").arg(profileError));
    }
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return failureResult(image, QStringLiteral("输入图像为空。"));
    }

    const QRect imageBounds(0, 0, image.width(), image.height());
    const QRect effectiveRoi = roi.isValid() ? roi.intersected(imageBounds) : imageBounds;
    if (!effectiveRoi.isValid() || effectiveRoi.width() < 32 || effectiveRoi.height() < 32) {
        return failureResult(image, QStringLiteral("ROI 太小，至少需要 32x32 像素。"));
    }

    const cv::Mat bgr = qImageToBgr(image);
    const cv::Rect cvRoi(effectiveRoi.x(),
                         effectiveRoi.y(),
                         effectiveRoi.width(),
                         effectiveRoi.height());
    const cv::Mat crop = bgr(cvRoi).clone();
    cv::Mat gray;
    cv::cvtColor(crop, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 1.2, 1.2);

    const int minimumDimension = std::min(crop.cols, crop.rows);
    const double roiCenterX = crop.cols * 0.5;
    const double roiCenterY = crop.rows * 0.5;
    const cv::Point2f roiCenter(static_cast<float>(roiCenterX),
                                static_cast<float>(roiCenterY));
    double radius = minimumDimension * 0.42;
    cv::Point2f center = roiCenter;
    GaugeCenterSource centerSource = GaugeCenterSource::RoiCenterFallback;
    double centerQuality = 0.45;

    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(gray,
                     circles,
                     cv::HOUGH_GRADIENT,
                     1.2,
                     std::max(12.0, minimumDimension * 0.25),
                     110.0,
                     std::max(18.0, minimumDimension * 0.04),
                     std::max(8, static_cast<int>(minimumDimension * 0.25)),
                     std::max(10, static_cast<int>(minimumDimension * 0.60)));
    double bestCircleScore = -std::numeric_limits<double>::max();
    for (const cv::Vec3f& circle : circles) {
        const cv::Point2f candidate(circle[0], circle[1]);
        const double candidateRadius = circle[2];
        if (candidateRadius <= 0.0
            || candidate.x < 0.0 || candidate.y < 0.0
            || candidate.x >= crop.cols || candidate.y >= crop.rows) {
            continue;
        }
        const double centerDistance = distance(candidate, roiCenter);
        const double score = clamp01(1.0 - centerDistance / (minimumDimension * 0.35))
            + clamp01(candidateRadius / (minimumDimension * 0.5));
        if (score > bestCircleScore) {
            bestCircleScore = score;
            center = candidate;
            radius = candidateRadius;
        }
    }
    if (bestCircleScore > 0.7) {
        centerSource = GaugeCenterSource::Circle;
        centerQuality = clamp01(bestCircleScore / 2.0);
    }

    cv::Mat edges;
    std::vector<LineCandidate> candidates;
    int searchPass = 0;
    const std::vector<SearchParameters> searchModes{
        SearchParameters{},
        SearchParameters{
            2,
            30.0,
            100.0,
            0.035,
            0.08,
            0.08,
            0.40,
            0.28,
            0.09,
        },
    };
    std::vector<LineCandidate> fallbackCandidates;
    cv::Mat fallbackEdges;
    int fallbackSearchPass = 0;
    for (const SearchParameters& parameters : searchModes) {
        cv::Mat passEdges;
        cv::Canny(gray, passEdges, parameters.cannyLow, parameters.cannyHigh);
        std::vector<cv::Vec4i> lines;
        cv::HoughLinesP(passEdges,
                        lines,
                        1.0,
                        kPi / 180.0,
                        std::max(10, static_cast<int>(minimumDimension
                                                      * parameters.houghThresholdRatio)),
                        std::max(8.0, minimumDimension * parameters.minimumLineLengthRatio),
                        std::max(3.0, minimumDimension * parameters.maximumLineGapRatio));

        std::vector<LineCandidate> passCandidates;
        passCandidates.reserve(lines.size());
        for (const cv::Vec4i& line : lines) {
            const cv::Point2f first(static_cast<float>(line[0]), static_cast<float>(line[1]));
            const cv::Point2f second(static_cast<float>(line[2]), static_cast<float>(line[3]));
            const double firstDistance = distance(first, center);
            const double secondDistance = distance(second, center);
            const cv::Point2f nearPoint = firstDistance <= secondDistance ? first : second;
            const cv::Point2f farPoint = firstDistance <= secondDistance ? second : first;
            const double nearDistance = std::min(firstDistance, secondDistance);
            const double farDistance = std::max(firstDistance, secondDistance);
            const double length = distance(first, second);
            const double lineDistance = length > 1.0
                ? std::abs((second.x - first.x) * (center.y - first.y)
                           - (second.y - first.y) * (center.x - first.x)) / length
                : std::numeric_limits<double>::max();
            if (nearDistance > radius * parameters.nearDistanceRatio
                || farDistance < radius * parameters.farDistanceRatio
                || length < minimumDimension * parameters.minimumCandidateLengthRatio
                || lineDistance > radius * 0.24) {
                continue;
            }
            const double angleDegrees = std::atan2(
                                          -(static_cast<double>(farPoint.y) - center.y),
                                          static_cast<double>(farPoint.x) - center.x)
                * 180.0 / kPi;
            LineCandidate candidate;
            candidate.nearPoint = nearPoint;
            candidate.farPoint = farPoint;
            candidate.length = length;
            candidate.nearDistance = nearDistance;
            candidate.farDistance = farDistance;
            candidate.lineDistance = lineDistance;
            candidate.angleDegrees = angleDegrees;
            candidate.radialSupport = radialNeedleSupport(gray, center, farPoint, radius);
            passCandidates.push_back(candidate);
        }
        std::vector<LineCandidate> inProfileCandidates;
        inProfileCandidates.reserve(passCandidates.size());
        for (const LineCandidate& candidate : passCandidates) {
            if (profile.valueForAngle(candidate.angleDegrees, nullptr).has_value()) {
                inProfileCandidates.push_back(candidate);
            }
        }
        if (!passCandidates.empty() && inProfileCandidates.empty()
            && fallbackCandidates.empty()) {
            fallbackCandidates = passCandidates;
            fallbackEdges = passEdges;
            fallbackSearchPass = parameters.pass;
        }
        if (!inProfileCandidates.empty()) {
            double maximumRadialSupport = 0.0;
            for (const LineCandidate& candidate : inProfileCandidates) {
                maximumRadialSupport = std::max(maximumRadialSupport, candidate.radialSupport);
            }
            if (parameters.pass == 2) {
                const std::optional<LineCandidate> scanCandidate =
                    bestRadialScanCandidate(gray, center, radius, profile);
                if (scanCandidate.has_value()
                    && (scanCandidate->radialSupport >= 0.60
                        || scanCandidate->radialSupport > maximumRadialSupport + 0.06)) {
                    inProfileCandidates.push_back(*scanCandidate);
                }
            }
            // A first-pass Hough line with weak radial image support is not
            // sufficient evidence to stop the deterministic search. Let the
            // relaxed pass inspect the same ROI before selecting a needle.
            if (parameters.pass == 1 && maximumRadialSupport < 0.70) {
                continue;
            }
            edges = passEdges;
            candidates = std::move(inProfileCandidates);
            searchPass = parameters.pass;
            break;
        }
    }
    if (candidates.empty() && !fallbackCandidates.empty()) {
        candidates = std::move(fallbackCandidates);
        edges = fallbackEdges;
        searchPass = fallbackSearchPass;
    }

    if (candidates.empty()) {
        cv::Mat overlay = bgr.clone();
        drawOverlay(&overlay,
                    effectiveRoi,
                    nullptr,
                    nullptr,
                    0.0,
                    centerSource,
                    0.0,
                    std::nullopt,
                    0.0,
                    searchPass);
        VisualGaugeReadingResult result = failureResult(
            image, QStringLiteral("未找到经过中心的有效 HoughLinesP 指针候选。"));
        result.center = QPointF(effectiveRoi.x() + center.x, effectiveRoi.y() + center.y);
        result.searchPass = searchPass == 0 ? 2 : searchPass;
        result.centerSource = centerSource;
        result.diagnosticOverlay = bgrToQImage(overlay);
        return result;
    }

    for (qsizetype index = 0; index < static_cast<qsizetype>(candidates.size()); ++index) {
        double nearestOtherAngle = 180.0;
        for (qsizetype otherIndex = 0;
             otherIndex < static_cast<qsizetype>(candidates.size());
             ++otherIndex) {
            if (index == otherIndex) {
                continue;
            }
            nearestOtherAngle = std::min(
                nearestOtherAngle,
                angleDifferenceDegrees(candidates.at(index).angleDegrees,
                                        candidates.at(otherIndex).angleDegrees));
        }
        candidates.at(index).uniqueness = clamp01(nearestOtherAngle / 45.0);
        const double centerScore = clamp01(
            1.0 - candidates.at(index).lineDistance / std::max(1.0, radius * 0.24));
        const double lengthScore = clamp01(candidates.at(index).length / std::max(1.0, radius));
        const double tipScore = clamp01(
            (candidates.at(index).farDistance - radius * 0.30) / std::max(1.0, radius * 0.70));
        const double lineLength = std::max(1.0, candidates.at(index).length);
        const cv::Point2f outward(
            static_cast<float>((candidates.at(index).farPoint.x - candidates.at(index).nearPoint.x)
                               / lineLength),
            static_cast<float>((candidates.at(index).farPoint.y - candidates.at(index).nearPoint.y)
                               / lineLength));
        const double farVectorLength = std::max(1.0, candidates.at(index).farDistance);
        const cv::Point2f radial(
            static_cast<float>((candidates.at(index).farPoint.x - center.x) / farVectorLength),
            static_cast<float>((candidates.at(index).farPoint.y - center.y) / farVectorLength));
        const double radialConsistency = clamp01((outward.dot(radial) - 0.50) / 0.50);
        const double insideScore = candidates.at(index).farDistance <= radius
            ? 1.0
            : clamp01(1.0 - (candidates.at(index).farDistance - radius)
                             / std::max(1.0, radius * 0.15));
        candidates.at(index).score = 0.23 * centerScore
            + 0.12 * lengthScore
            + 0.12 * tipScore
            + 0.08 * candidates.at(index).uniqueness
            + 0.15 * radialConsistency
            + 0.25 * candidates.at(index).radialSupport
            + 0.05 * insideScore;
    }

    const auto bestIterator = std::max_element(
        candidates.cbegin(), candidates.cend(), [](const LineCandidate& left, const LineCandidate& right) {
            return left.score < right.score;
        });
    const LineCandidate& best = *bestIterator;
    const cv::Point2f fullCenter(center.x + effectiveRoi.x(), center.y + effectiveRoi.y());
    const cv::Point2f fullTip(best.farPoint.x + effectiveRoi.x(), best.farPoint.y + effectiveRoi.y());
    const double angleDegrees = std::atan2(
                                  -(static_cast<double>(fullTip.y) - fullCenter.y),
                                  static_cast<double>(fullTip.x) - fullCenter.x)
        * 180.0 / kPi;
    const double confidence = clamp01(0.65 * centerQuality
                                      + 0.25 * best.score
                                      + 0.10 * best.uniqueness);
    QString conversionError;
    const std::optional<double> value = profile.valueForAngle(angleDegrees, &conversionError);

    cv::Mat overlay = bgr.clone();
    drawOverlay(&overlay,
                effectiveRoi,
                &fullCenter,
                &fullTip,
                centerSource == GaugeCenterSource::Circle ? radius : 0.0,
                centerSource,
                angleDegrees,
                value,
                confidence,
                searchPass);

    VisualGaugeReadingResult result;
    result.value = value.value_or(0.0);
    result.needleAngleDegrees = angleDegrees;
    result.center = QPointF(fullCenter.x, fullCenter.y);
    result.needleTip = QPointF(fullTip.x, fullTip.y);
    result.confidence = confidence;
    result.searchPass = searchPass;
    result.centerSource = centerSource;
    result.diagnosticOverlay = bgrToQImage(overlay);
    if (!value.has_value()) {
        result.failureReason = conversionError.isEmpty()
            ? QStringLiteral("指针角度超出 GaugeProfile 有效 sweep。")
            : conversionError;
        return result;
    }
    result.success = true;
    return result;
}

VisualGaugeReadingResult VisualGaugeReader::readFromFile(const QString& imagePath,
                                                         const GaugeProfile& profile,
                                                         const QRect& roi)
{
    QImageReader reader(imagePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return failureResult({}, QStringLiteral("无法读取仪表图像: %1").arg(reader.errorString()));
    }
    return read(image, profile, roi);
}

} // namespace vision3d
