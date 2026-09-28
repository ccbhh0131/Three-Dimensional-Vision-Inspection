#include "core/reconstruction/ReconstructionTask.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QVariant>

namespace vision3d {

namespace {

template <typename T>
std::optional<T> invalidValue(const QString& field, QString* error)
{
    if (error != nullptr) {
        *error = QStringLiteral("task.json 字段无效: %1。 ").arg(field).trimmed();
    }
    return std::nullopt;
}

QString dateTimeToString(const QDateTime& value)
{
    return value.isValid() ? value.toUTC().toString(Qt::ISODateWithMs) : QString();
}

QDateTime dateTimeFromJson(const QJsonObject& json,
                           const QString& field,
                           QString* error,
                           bool required)
{
    const QJsonValue value = json.value(field);
    if (!value.isString()) {
        if (required && error != nullptr) {
            *error = QStringLiteral("task.json 缺少有效时间字段: %1。 ").arg(field).trimmed();
        }
        return {};
    }
    const QString text = value.toString();
    if (text.isEmpty()) {
        return {};
    }
    const QDateTime result = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!result.isValid() && error != nullptr) {
        *error = QStringLiteral("task.json 时间字段无法解析: %1。 ").arg(field).trimmed();
    }
    return result;
}

bool readInteger(const QJsonObject& json, const QString& field, int* target, QString* error)
{
    const QJsonValue value = json.value(field);
    if (!value.isDouble()) {
        if (error != nullptr) {
            *error = QStringLiteral("task.json 缺少有效整数: %1。 ").arg(field).trimmed();
        }
        return false;
    }
    const double number = value.toDouble();
    if (number != static_cast<int>(number)) {
        if (error != nullptr) {
            *error = QStringLiteral("task.json 整数值无效: %1。 ").arg(field).trimmed();
        }
        return false;
    }
    *target = static_cast<int>(number);
    return true;
}

} // namespace

QString reconstructionStateToString(ReconstructionState state)
{
    switch (state) {
    case ReconstructionState::Idle:
        return QStringLiteral("idle");
    case ReconstructionState::Preparing:
        return QStringLiteral("preparing");
    case ReconstructionState::Running:
        return QStringLiteral("running");
    case ReconstructionState::Completed:
        return QStringLiteral("completed");
    case ReconstructionState::Failed:
        return QStringLiteral("failed");
    case ReconstructionState::Cancelled:
        return QStringLiteral("cancelled");
    case ReconstructionState::Interrupted:
        return QStringLiteral("interrupted");
    }
    return QStringLiteral("idle");
}

std::optional<ReconstructionState> reconstructionStateFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("idle")) {
        return ReconstructionState::Idle;
    }
    if (normalized == QStringLiteral("preparing")) {
        return ReconstructionState::Preparing;
    }
    if (normalized == QStringLiteral("running")) {
        return ReconstructionState::Running;
    }
    if (normalized == QStringLiteral("completed")) {
        return ReconstructionState::Completed;
    }
    if (normalized == QStringLiteral("failed")) {
        return ReconstructionState::Failed;
    }
    if (normalized == QStringLiteral("cancelled")) {
        return ReconstructionState::Cancelled;
    }
    if (normalized == QStringLiteral("interrupted")) {
        return ReconstructionState::Interrupted;
    }
    return std::nullopt;
}

QString reconstructionStageToString(ReconstructionStage stage)
{
    switch (stage) {
    case ReconstructionStage::None:
        return QStringLiteral("none");
    case ReconstructionStage::PreparingInput:
        return QStringLiteral("preparing_input");
    case ReconstructionStage::FeatureExtraction:
        return QStringLiteral("feature_extraction");
    case ReconstructionStage::FeatureMatching:
        return QStringLiteral("feature_matching");
    case ReconstructionStage::SparseMapping:
        return QStringLiteral("sparse_mapping");
    case ReconstructionStage::ImageUndistortion:
        return QStringLiteral("image_undistortion");
    case ReconstructionStage::DenseStereo:
        return QStringLiteral("dense_stereo");
    case ReconstructionStage::StereoFusion:
        return QStringLiteral("stereo_fusion");
    case ReconstructionStage::Meshing:
        return QStringLiteral("meshing");
    case ReconstructionStage::ModelOptimization:
        return QStringLiteral("model_optimization");
    case ReconstructionStage::Completed:
        return QStringLiteral("completed");
    }
    return QStringLiteral("none");
}

std::optional<ReconstructionStage> reconstructionStageFromString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    const QList<QPair<QString, ReconstructionStage>> values = {
        {QStringLiteral("none"), ReconstructionStage::None},
        {QStringLiteral("preparing_input"), ReconstructionStage::PreparingInput},
        {QStringLiteral("feature_extraction"), ReconstructionStage::FeatureExtraction},
        {QStringLiteral("feature_matching"), ReconstructionStage::FeatureMatching},
        {QStringLiteral("sparse_mapping"), ReconstructionStage::SparseMapping},
        {QStringLiteral("image_undistortion"), ReconstructionStage::ImageUndistortion},
        {QStringLiteral("dense_stereo"), ReconstructionStage::DenseStereo},
        {QStringLiteral("stereo_fusion"), ReconstructionStage::StereoFusion},
        {QStringLiteral("meshing"), ReconstructionStage::Meshing},
        {QStringLiteral("model_optimization"), ReconstructionStage::ModelOptimization},
        {QStringLiteral("completed"), ReconstructionStage::Completed},
    };
    for (const auto& pair : values) {
        if (pair.first == normalized) {
            return pair.second;
        }
    }
    return std::nullopt;
}

QJsonObject ReconstructionTask::toJson() const
{
    return QJsonObject{
        {QStringLiteral("taskId"), taskId},
        {QStringLiteral("state"), reconstructionStateToString(state)},
        {QStringLiteral("stage"), reconstructionStageToString(stage)},
        {QStringLiteral("backend"),
         QJsonObject{{QStringLiteral("name"), backendName},
                     {QStringLiteral("version"), backendVersion}}},
        {QStringLiteral("workspace"), workspaceRelativePath},
        {QStringLiteral("timestamps"),
         QJsonObject{{QStringLiteral("createdAt"), dateTimeToString(createdAt)},
                     {QStringLiteral("startedAt"), dateTimeToString(startedAt)},
                     {QStringLiteral("finishedAt"), dateTimeToString(finishedAt)}}},
        {QStringLiteral("errorMessage"), errorMessage},
        {QStringLiteral("failedStage"), failedStage},
        {QStringLiteral("exitCode"), exitCode},
        {QStringLiteral("input"), QJsonObject{{QStringLiteral("imageCount"), inputImageCount}}},
        {QStringLiteral("sparse"),
         QJsonObject{{QStringLiteral("primaryModel"), primaryModelRelativePath},
                     {QStringLiteral("registeredImages"), registeredImageCount},
                     {QStringLiteral("registrationRatio"), registrationRatio}}},
        {QStringLiteral("outputs"),
         QJsonObject{{QStringLiteral("fusedPointCloud"), fusedPointCloudRelativePath},
                     {QStringLiteral("mesh"), meshRelativePath}}},
        {QStringLiteral("totalElapsedMilliseconds"), totalElapsedMilliseconds},
    };
}

std::optional<ReconstructionTask> ReconstructionTask::fromJson(const QJsonObject& json,
                                                               QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<ReconstructionTask> {
        if (error != nullptr) {
            *error = message;
        }
        return std::nullopt;
    };

    if (!json.value(QStringLiteral("taskId")).isString()
        || json.value(QStringLiteral("taskId")).toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("task.json 缺少 taskId。"));
    }
    if (!json.value(QStringLiteral("state")).isString()
        || !json.value(QStringLiteral("stage")).isString()) {
        return fail(QStringLiteral("task.json 缺少 state 或 stage。"));
    }

    const std::optional<ReconstructionState> state =
        reconstructionStateFromString(json.value(QStringLiteral("state")).toString());
    const std::optional<ReconstructionStage> stage =
        reconstructionStageFromString(json.value(QStringLiteral("stage")).toString());
    if (!state.has_value()) {
        return fail(QStringLiteral("task.json state 无效。"));
    }
    if (!stage.has_value()) {
        return fail(QStringLiteral("task.json stage 无效。"));
    }

    ReconstructionTask task;
    task.taskId = json.value(QStringLiteral("taskId")).toString().trimmed();
    task.state = *state;
    task.stage = *stage;
    task.workspaceRelativePath = json.value(QStringLiteral("workspace")).toString();
    task.errorMessage = json.value(QStringLiteral("errorMessage")).toString();
    task.failedStage = json.value(QStringLiteral("failedStage")).toString();

    const QJsonObject backend = json.value(QStringLiteral("backend")).toObject();
    task.backendName = backend.value(QStringLiteral("name")).toString();
    task.backendVersion = backend.value(QStringLiteral("version")).toString();

    const QJsonObject timestamps = json.value(QStringLiteral("timestamps")).toObject();
    QString dateError;
    task.createdAt = dateTimeFromJson(timestamps, QStringLiteral("createdAt"), &dateError, false);
    task.startedAt = dateTimeFromJson(timestamps, QStringLiteral("startedAt"), &dateError, false);
    task.finishedAt = dateTimeFromJson(timestamps, QStringLiteral("finishedAt"), &dateError, false);
    if (!dateError.isEmpty()) {
        return fail(dateError);
    }

    const QJsonValue exitCode = json.value(QStringLiteral("exitCode"));
    if (exitCode.isDouble()) {
        const double number = exitCode.toDouble();
        if (number != static_cast<int>(number)) {
            return fail(QStringLiteral("task.json exitCode 无效。"));
        }
        task.exitCode = static_cast<int>(number);
    }

    const QJsonObject input = json.value(QStringLiteral("input")).toObject();
    if (input.contains(QStringLiteral("imageCount"))
        && !readInteger(input, QStringLiteral("imageCount"), &task.inputImageCount, error)) {
        return std::nullopt;
    }

    const QJsonObject sparse = json.value(QStringLiteral("sparse")).toObject();
    task.primaryModelRelativePath = sparse.value(QStringLiteral("primaryModel")).toString();
    if (sparse.contains(QStringLiteral("registeredImages"))
        && !readInteger(sparse,
                        QStringLiteral("registeredImages"),
                        &task.registeredImageCount,
                        error)) {
        return std::nullopt;
    }
    if (sparse.value(QStringLiteral("registrationRatio")).isDouble()) {
        task.registrationRatio = sparse.value(QStringLiteral("registrationRatio")).toDouble();
    }

    const QJsonObject outputs = json.value(QStringLiteral("outputs")).toObject();
    task.fusedPointCloudRelativePath = outputs.value(QStringLiteral("fusedPointCloud")).toString();
    task.meshRelativePath = outputs.value(QStringLiteral("mesh")).toString();
    if (json.value(QStringLiteral("totalElapsedMilliseconds")).isDouble()) {
        task.totalElapsedMilliseconds = static_cast<qint64>(
            json.value(QStringLiteral("totalElapsedMilliseconds")).toDouble());
    }
    return task;
}

bool ReconstructionTask::save(const QString& filePath, QString* error) const
{
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法写入 task.json: %1").arg(file.errorString());
        }
        return false;
    }
    const QByteArray payload = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("保存 task.json 失败: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

std::optional<ReconstructionTask> ReconstructionTask::load(const QString& filePath,
                                                           QString* error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("无法读取 task.json: %1").arg(file.errorString());
        }
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("task.json JSON 解析失败: %1").arg(parseError.errorString());
        }
        return std::nullopt;
    }
    return fromJson(document.object(), error);
}

} // namespace vision3d
