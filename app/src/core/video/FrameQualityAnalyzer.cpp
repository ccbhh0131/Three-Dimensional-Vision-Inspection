#include "core/video/FrameQualityAnalyzer.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFont>
#include <QVector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace vision3d {

namespace {

struct FrameRecord
{
    QString path;
    QString fileName;
    double timestamp = 0.0;
    double blurScore = 0.0;
    double similarityScore = 0.0;
    int hashDistance = -1;
    bool decoded = false;
    bool accepted = false;
    bool recovered = false;
    bool blurFlagged = false;
    bool duplicateFlagged = false;
    QString rejectionReason;
    cv::Mat gray;
    cv::Mat smallGray;
    quint64 hash = 0;
};

double percentile(const QVector<double>& sortedValues, double percent)
{
    if (sortedValues.isEmpty()) {
        return 0.0;
    }
    const double position = qBound(0.0, percent, 1.0)
                            * static_cast<double>(sortedValues.size() - 1);
    const qsizetype lower = static_cast<qsizetype>(std::floor(position));
    const qsizetype upper = static_cast<qsizetype>(std::ceil(position));
    const double weight = position - static_cast<double>(lower);
    return sortedValues.at(lower) * (1.0 - weight) + sortedValues.at(upper) * weight;
}

quint64 perceptualHash(const cv::Mat& gray)
{
    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(32, 32), 0.0, 0.0, cv::INTER_AREA);
    cv::Mat floating;
    resized.convertTo(floating, CV_32F);
    cv::Mat transformed;
    cv::dct(floating, transformed);

    QVector<float> coefficients;
    coefficients.reserve(63);
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            if (row != 0 || column != 0) {
                coefficients.append(transformed.at<float>(row, column));
            }
        }
    }
    std::sort(coefficients.begin(), coefficients.end());
    const float median = coefficients.at(coefficients.size() / 2);
    quint64 hash = 0;
    int bit = 0;
    for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 8; ++column) {
            if (row == 0 && column == 0) {
                continue;
            }
            if (transformed.at<float>(row, column) > median) {
                hash |= (quint64{1} << bit);
            }
            ++bit;
        }
    }
    return hash;
}

int hashDistance(quint64 lhs, quint64 rhs)
{
    return qPopulationCount(lhs ^ rhs);
}

double normalizedDifference(const cv::Mat& lhs, const cv::Mat& rhs)
{
    if (lhs.empty() || rhs.empty() || lhs.size() != rhs.size()) {
        return 1.0;
    }
    cv::Mat difference;
    cv::absdiff(lhs, rhs, difference);
    return cv::mean(difference)[0] / 255.0;
}

bool writeJson(const QString& path, const QJsonObject& object, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法写入帧清单：%1").arg(file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("无法提交帧清单：%1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool createContactSheet(const QVector<FrameRecord>& records,
                        const QString& contactSheetPath,
                        QString* error)
{
    QVector<int> acceptedIndexes;
    for (qsizetype index = 0; index < records.size(); ++index) {
        if (records.at(index).accepted) {
            acceptedIndexes.append(static_cast<int>(index));
        }
    }
    if (acceptedIndexes.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("没有有效帧可生成预览图。");
        }
        return false;
    }

    const int previewCount = qMin(40, acceptedIndexes.size());
    QVector<int> selected;
    selected.reserve(previewCount);
    for (int i = 0; i < previewCount; ++i) {
        const int slot = previewCount == 1
                             ? 0
                             : qRound(static_cast<double>(i)
                                      * static_cast<double>(acceptedIndexes.size() - 1)
                                      / static_cast<double>(previewCount - 1));
        selected.append(acceptedIndexes.at(slot));
    }

    constexpr int columns = 4;
    constexpr int cellWidth = 320;
    constexpr int cellHeight = 260;
    const int rows = (selected.size() + columns - 1) / columns;
    QImage sheet(columns * cellWidth, rows * cellHeight, QImage::Format_RGB32);
    sheet.fill(QColor(30, 36, 45));
    QPainter painter(&sheet);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setPen(QColor(235, 239, 244));
    QFont font(QStringLiteral("Microsoft YaHei"), 10);
    painter.setFont(font);
    for (int index = 0; index < selected.size(); ++index) {
        const FrameRecord& record = records.at(selected.at(index));
        QImage frame(record.path);
        if (frame.isNull()) {
            continue;
        }
        const int column = index % columns;
        const int row = index / columns;
        const QRect cell(column * cellWidth, row * cellHeight, cellWidth, cellHeight);
        const QRect imageArea(cell.left() + 10, cell.top() + 8, cellWidth - 20, 220);
        const QImage thumb = frame.scaled(imageArea.size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
        const QPoint topLeft(imageArea.center().x() - thumb.width() / 2,
                             imageArea.center().y() - thumb.height() / 2);
        painter.drawImage(topLeft, thumb);
        painter.drawText(QRect(cell.left() + 10, cell.top() + 232, cellWidth - 20, 22),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("%1  %2 秒")
                             .arg(record.fileName)
                             .arg(record.timestamp, 0, 'f', 2));
    }
    painter.end();
    if (!sheet.save(contactSheetPath, "JPG", 90)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法保存抽帧预览图：%1").arg(contactSheetPath);
        }
        return false;
    }
    return true;
}

} // namespace

bool FrameQualityAnalyzer::analyze(const QStringList& rawFramePaths,
                                   const QString& sourceVideo,
                                   double durationSeconds,
                                   double sampleFps,
                                   const QString& jobDirectory,
                                   FrameQualitySummary* summary,
                                   const ProgressCallback& progress,
                                   const std::atomic_bool* cancel,
                                   QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    if (summary == nullptr) {
        return fail(QStringLiteral("帧质量统计输出参数为空。"));
    }
    if (rawFramePaths.isEmpty() || durationSeconds <= 0.0 || sampleFps <= 0.0) {
        return fail(QStringLiteral("没有可分析的抽帧文件或视频时间信息无效。"));
    }

    const QString rawDirectory = QDir(jobDirectory).filePath(QStringLiteral("raw_frames"));
    const QString acceptedDirectory = QDir(jobDirectory).filePath(QStringLiteral("accepted_frames"));
    const QString rejectedBlurDirectory = QDir(jobDirectory).filePath(
        QStringLiteral("rejected_frames/blur"));
    const QString rejectedDuplicateDirectory = QDir(jobDirectory).filePath(
        QStringLiteral("rejected_frames/duplicate"));
    const QString manifestsDirectory = QDir(jobDirectory).filePath(QStringLiteral("manifests"));
    if (!QDir().mkpath(rawDirectory) || !QDir().mkpath(acceptedDirectory)
        || !QDir().mkpath(rejectedBlurDirectory)
        || !QDir().mkpath(rejectedDuplicateDirectory)
        || !QDir().mkpath(manifestsDirectory)) {
        return fail(QStringLiteral("无法创建视频帧工作目录。"));
    }

    QVector<FrameRecord> records;
    QVector<double> blurScores;
    records.reserve(rawFramePaths.size());
    for (qsizetype index = 0; index < rawFramePaths.size(); ++index) {
        if (cancel != nullptr && cancel->load()) {
            return fail(QStringLiteral("帧质量检查已取消。"));
        }
        FrameRecord record;
        record.path = QFileInfo(rawFramePaths.at(index)).absoluteFilePath();
        record.fileName = QFileInfo(record.path).fileName();
        record.timestamp = qMin(durationSeconds,
                                static_cast<double>(index) / sampleFps);
        QImage image(record.path);
        if (!image.isNull()) {
            image = image.convertToFormat(QImage::Format_Grayscale8);
            const cv::Mat decoded(image.height(),
                                  image.width(),
                                  CV_8UC1,
                                  const_cast<uchar*>(image.constBits()),
                                  static_cast<size_t>(image.bytesPerLine()));
            record.gray = decoded.clone();
            cv::Mat laplacian;
            cv::Laplacian(record.gray, laplacian, CV_64F);
            cv::Scalar mean;
            cv::Scalar standardDeviation;
            cv::meanStdDev(laplacian, mean, standardDeviation);
            record.blurScore = standardDeviation[0] * standardDeviation[0];
            cv::resize(record.gray,
                       record.smallGray,
                       cv::Size(32, 32),
                       0.0,
                       0.0,
                       cv::INTER_AREA);
            record.hash = perceptualHash(record.gray);
            record.decoded = true;
            blurScores.append(record.blurScore);
        } else {
            record.rejectionReason = QStringLiteral("decode_error");
        }
        if (!records.isEmpty() && record.decoded && records.last().decoded) {
            record.hashDistance = hashDistance(records.last().hash, record.hash);
            record.similarityScore = 1.0
                                     - normalizedDifference(records.last().smallGray,
                                                           record.smallGray);
        }
        records.append(std::move(record));
        if (progress) {
            progress(static_cast<int>(index + 1), static_cast<int>(rawFramePaths.size()));
        }
    }
    if (blurScores.isEmpty()) {
        return fail(QStringLiteral("所有抽帧均无法解码，未生成有效帧。"));
    }

    std::sort(blurScores.begin(), blurScores.end());
    const double p10 = percentile(blurScores, 0.10);
    const double p25 = percentile(blurScores, 0.25);
    const double median = percentile(blurScores, 0.50);
    const double p75 = percentile(blurScores, 0.75);
    const double relativeTailLimit = median * 0.35;
    for (FrameRecord& record : records) {
        if (!record.decoded) {
            continue;
        }
        record.accepted = true;
        if (record.blurScore <= p10 && median > 0.0
            && record.blurScore < relativeTailLimit) {
            record.accepted = false;
            record.blurFlagged = true;
            record.rejectionReason = QStringLiteral("blur_relative_tail");
        }
    }

    qsizetype groupStart = 0;
    while (groupStart < records.size()) {
        if (!records.at(groupStart).decoded) {
            ++groupStart;
            continue;
        }
        qsizetype groupEnd = groupStart;
        while (groupEnd + 1 < records.size()) {
            const FrameRecord& next = records.at(groupEnd + 1);
            if (!next.decoded || next.hashDistance < 0 || next.hashDistance > 1
                || (1.0 - next.similarityScore) > 0.015) {
                break;
            }
            ++groupEnd;
        }
        if (groupEnd > groupStart) {
            qsizetype best = groupStart;
            for (qsizetype index = groupStart + 1; index <= groupEnd; ++index) {
                if (records.at(index).blurScore > records.at(best).blurScore) {
                    best = index;
                }
            }
            for (qsizetype index = groupStart; index <= groupEnd; ++index) {
                if (index == best) {
                    continue;
                }
                if (records.at(index).rejectionReason != QStringLiteral("decode_error")) {
                    records[index].accepted = false;
                    records[index].duplicateFlagged = true;
                    records[index].rejectionReason = QStringLiteral("near_duplicate");
                }
            }
        }
        groupStart = groupEnd + 1;
    }

    int coverageRecovered = 0;
    constexpr double coverageWindowSeconds = 2.5;
    for (double windowStart = 0.0; windowStart < durationSeconds;
         windowStart += coverageWindowSeconds) {
        const double windowEnd = qMin(durationSeconds, windowStart + coverageWindowSeconds);
        bool hasAccepted = false;
        for (const FrameRecord& record : records) {
            if (record.accepted && record.timestamp >= windowStart
                && (record.timestamp < windowEnd
                    || qFuzzyCompare(windowEnd + 1.0, durationSeconds + 1.0))) {
                hasAccepted = true;
                break;
            }
        }
        if (hasAccepted) {
            continue;
        }
        qsizetype sharpest = -1;
        for (qsizetype index = 0; index < records.size(); ++index) {
            const FrameRecord& record = records.at(index);
            if (record.decoded && !record.accepted && record.timestamp >= windowStart
                && record.timestamp < windowEnd
                && (sharpest < 0 || record.blurScore > records.at(sharpest).blurScore)) {
                sharpest = index;
            }
        }
        if (sharpest >= 0) {
            records[sharpest].accepted = true;
            records[sharpest].recovered = true;
            ++coverageRecovered;
        }
    }

    FrameQualitySummary result;
    result.rawFrameCount = rawFramePaths.size();
    result.blurP10 = p10;
    result.blurP25 = p25;
    result.blurMedian = median;
    result.blurP75 = p75;
    for (const FrameRecord& record : records) {
        result.blurRejectedCount += record.blurFlagged ? 1 : 0;
        result.duplicateRejectedCount += record.duplicateFlagged ? 1 : 0;
    }
    result.coverageRecoveredCount = coverageRecovered;
    result.frameManifestPath = QDir(manifestsDirectory).filePath(
        QStringLiteral("frames-manifest.json"));
    result.acceptedManifestPath = QDir(manifestsDirectory).filePath(
        QStringLiteral("accepted-frames-manifest.json"));
    result.contactSheetPath = QDir(jobDirectory).filePath(QStringLiteral("frame-contact-sheet.jpg"));

    QJsonArray frameArray;
    QJsonArray acceptedArray;
    for (const FrameRecord& record : records) {
        const QString relativePath = QDir(jobDirectory).relativeFilePath(record.path);
        QJsonObject frame{{QStringLiteral("filename"), record.fileName},
                          {QStringLiteral("timestamp"), record.timestamp},
                          {QStringLiteral("sourceVideo"), sourceVideo},
                          {QStringLiteral("accepted"), record.accepted},
                          {QStringLiteral("blurScore"), record.blurScore},
                          {QStringLiteral("blurFlagged"), record.blurFlagged},
                          {QStringLiteral("duplicateFlagged"), record.duplicateFlagged},
                          {QStringLiteral("similarityScore"), record.similarityScore},
                          {QStringLiteral("pHashDistance"), record.hashDistance},
                          {QStringLiteral("rejectionReason"), record.rejectionReason},
                          {QStringLiteral("recoveredByCoverageGuard"), record.recovered}};
        frameArray.append(frame);
        if (record.accepted) {
            const QString destination = QDir(acceptedDirectory).filePath(record.fileName);
            if (!QFileInfo::exists(destination) && !QFile::copy(record.path, destination)) {
                return fail(QStringLiteral("无法复制有效帧：%1").arg(record.fileName));
            }
            result.acceptedFramePaths.append(QFileInfo(destination).absoluteFilePath());
            acceptedArray.append(QJsonObject{{QStringLiteral("filename"), record.fileName},
                                             {QStringLiteral("timestamp"), record.timestamp},
                                             {QStringLiteral("sourceRawFrame"), relativePath},
                                             {QStringLiteral("acceptedPath"),
                                              QDir(jobDirectory).relativeFilePath(destination)},
                                             {QStringLiteral("recoveredByCoverageGuard"),
                                              record.recovered}});
        } else if (record.rejectionReason == QStringLiteral("near_duplicate")) {
            const QString destination = QDir(rejectedDuplicateDirectory).filePath(record.fileName);
            if (!QFileInfo::exists(destination) && !QFile::copy(record.path, destination)) {
                return fail(QStringLiteral("无法归档重复帧：%1").arg(record.fileName));
            }
        } else if (record.rejectionReason == QStringLiteral("blur_relative_tail")) {
            const QString destination = QDir(rejectedBlurDirectory).filePath(record.fileName);
            if (!QFileInfo::exists(destination) && !QFile::copy(record.path, destination)) {
                return fail(QStringLiteral("无法归档模糊帧：%1").arg(record.fileName));
            }
        }
    }
    result.acceptedFrameCount = result.acceptedFramePaths.size();
    if (result.acceptedFrameCount == 0) {
        return fail(QStringLiteral("质量筛选后没有有效帧。"));
    }

    const QJsonObject metrics{{QStringLiteral("p10"), p10},
                              {QStringLiteral("p25"), p25},
                              {QStringLiteral("median"), median},
                              {QStringLiteral("p75"), p75},
                              {QStringLiteral("relativeTailLimit"), relativeTailLimit}};
    const QJsonObject manifest{{QStringLiteral("schemaVersion"), 1},
                               {QStringLiteral("sourceVideo"), sourceVideo},
                               {QStringLiteral("durationSeconds"), durationSeconds},
                               {QStringLiteral("sampleFps"), sampleFps},
                               {QStringLiteral("rawFrameCount"), result.rawFrameCount},
                               {QStringLiteral("acceptedFrameCount"), result.acceptedFrameCount},
                               {QStringLiteral("blurRejectedCount"), result.blurRejectedCount},
                               {QStringLiteral("duplicateRejectedCount"),
                                result.duplicateRejectedCount},
                               {QStringLiteral("coverageRecoveredCount"), coverageRecovered},
                               {QStringLiteral("blurDistribution"), metrics},
                               {QStringLiteral("duplicateRule"),
                                QStringLiteral("pHash Hamming <= 1 and normalized difference <= 0.015")},
                               {QStringLiteral("coverageWindowSeconds"), coverageWindowSeconds},
                               {QStringLiteral("frames"), frameArray}};
    const QJsonObject acceptedManifest{{QStringLiteral("schemaVersion"), 1},
                                       {QStringLiteral("sourceVideo"), sourceVideo},
                                       {QStringLiteral("rawExtracted"), result.rawFrameCount},
                                       {QStringLiteral("blurRejected"), result.blurRejectedCount},
                                       {QStringLiteral("duplicateRejected"),
                                        result.duplicateRejectedCount},
                                       {QStringLiteral("coverageRecovered"), coverageRecovered},
                                       {QStringLiteral("finalAccepted"), result.acceptedFrameCount},
                                       {QStringLiteral("frames"), acceptedArray}};
    if (!writeJson(result.frameManifestPath, manifest, error)
        || !writeJson(result.acceptedManifestPath, acceptedManifest, error)
        || !createContactSheet(records, result.contactSheetPath, error)) {
        return false;
    }
    *summary = result;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace vision3d
