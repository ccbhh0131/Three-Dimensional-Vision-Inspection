#include "JobProcessor.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <cmath>
#include <utility>
#include <windows.h>

namespace {
bool samePath(const QString &a, const QString &b)
{
    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
}

// Names are single components. Canonical checks also reject escaping symlinks
// and Windows directory junctions, rather than relying only on '..' filtering.
bool safeEntry(const QString &parent, const QString &name)
{
    const QString path = QDir(parent).filePath(name);
    const QFileInfo info(path);
    if (info.isSymLink())
        return false;
    return !info.exists() || samePath(info.canonicalFilePath(), path);
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && doc.isObject()
        ? doc.object() : QJsonObject{};
}

QString hashFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}
}

JobProcessor::JobProcessor(QString inbox, QString outbox)
    : m_inbox(std::move(inbox)), m_outbox(std::move(outbox))
{
}

void JobProcessor::log(const QString &jobId, const QString &message)
{
    if (m_lastMessage.value(jobId) == message)
        return;
    m_lastMessage.insert(jobId, message);
    QTextStream(stdout) << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                        << ' ' << jobId << ' ' << message << Qt::endl;
}

bool JobProcessor::writeStatus(const QString &jobId, const QString &status,
                               const QString &sha256, const QString &error)
{
    const QString dir = QDir(m_outbox).filePath(jobId);
    if (!safeEntry(m_outbox, jobId) || !QDir().mkpath(dir)
        || !safeEntry(dir, "response.json"))
        return false;
    const auto old = readObject(QDir(dir).filePath("response.json"));
    if (old.value("jobId").toString() == jobId
        && old.value("status").toString() == status)
        return true;
    QJsonObject response{
        {"protocolVersion", 1}, {"jobId", jobId}, {"status", status},
        {"resultFile", status == "completed" ? "refined-mesh.ply" : ""},
        {"sha256", sha256}, {"error", error},
        {"updatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    };
    QSaveFile file(QDir(dir).filePath("response.json"));
    // Default direct-write fallback is disabled: publish by rename only.
    const auto bytes = QJsonDocument(response).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()
        && file.commit();
}

JobProcessor::Outcome JobProcessor::fail(const QString &jobId, const QString &error)
{
    if (!writeStatus(jobId, "failed", {}, error))
        log(jobId, "failed: cannot publish failure status");
    else
        log(jobId, "failed: " + error);
    return Outcome::Failed;
}

bool JobProcessor::processModel(const QString &inputMesh, const QString &outputMesh)
{
    QFile input(inputMesh);
    QSaveFile output(outputMesh);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        return false;
    while (!input.atEnd()) {
        const QByteArray block = input.read(1024 * 1024);
        if (input.error() != QFileDevice::NoError || block.isEmpty()
            || output.write(block) != block.size())
            return false;
    }
    return output.commit();
}

JobProcessor::Outcome JobProcessor::inspectJob(const QString &jobId)
{
    const QString inputDir = QDir(m_inbox).filePath(jobId);
    const QString outputDir = QDir(m_outbox).filePath(jobId);
    if (!safeEntry(m_inbox, jobId) || !safeEntry(m_outbox, jobId)) {
        log(jobId, "failed: job directory escapes its root");
        return Outcome::Failed;
    }
    if (!safeEntry(outputDir, "response.json")) {
        log(jobId, "failed: unsafe response path");
        return Outcome::Failed;
    }
    const auto previous = readObject(QDir(outputDir).filePath("response.json"));
    const QString previousStatus = previous.value("status").toString();
    if (previous.value("jobId").toString() == jobId
        && (previousStatus == "completed" || previousStatus == "failed"))
        return Outcome::Idle; // Persistent idempotency, including after restart.

    if (!m_lastMessage.contains(jobId))
        log(jobId, "received Job");
    if (!safeEntry(inputDir, "request.json") || !safeEntry(inputDir, "READY"))
        return fail(jobId, "unsafe request or READY path");
    const auto request = readObject(QDir(inputDir).filePath("request.json"));
    if (request.isEmpty()) {
        log(jobId, "waiting for complete request JSON");
        return Outcome::Idle;
    }
    if (!QFileInfo(QDir(inputDir).filePath("READY")).isFile()) {
        if (!writeStatus(jobId, "queued"))
            return fail(jobId, "cannot publish queued status");
        log(jobId, "waiting for READY");
        return Outcome::Idle;
    }

    const QString meshName = request.value("meshFile").toString();
    const QString expectedHash = request.value("sha256").toString().toLower();
    const double size = request.value("meshSize").toDouble(-1);
    static const QRegularExpression filePattern("^[A-Za-z0-9][A-Za-z0-9_-]*\\.ply$");
    static const QRegularExpression hashPattern("^[0-9a-f]{64}$");
    if (request.value("protocolVersion").toDouble(-1) != 1
        || request.value("jobId").toString() != jobId
        || request.value("type").toString() != "mesh-copy"
        || !filePattern.match(meshName).hasMatch()
        || !hashPattern.match(expectedHash).hasMatch()
        || !std::isfinite(size) || size <= 0 || size > 9007199254740991.0
        || std::floor(size) != size)
        return fail(jobId, "invalid or unsupported request fields");
    if (!safeEntry(inputDir, meshName))
        return fail(jobId, "mesh path escapes job directory");
    if (!writeStatus(jobId, "queued"))
        return fail(jobId, "cannot publish queued status");
    const QString inputMesh = QDir(inputDir).filePath(meshName);
    const QFileInfo mesh(inputMesh);
    if (!mesh.isFile() || mesh.size() != static_cast<qint64>(size)) {
        log(jobId, "waiting for model synchronization (missing or size mismatch)");
        return Outcome::Idle;
    }
    if (hashFile(inputMesh) != expectedHash) {
        log(jobId, "waiting for model synchronization (SHA-256 mismatch or unreadable)");
        return Outcome::Idle;
    }

    const QString staging = QDir(outputDir).filePath("refined-mesh.ply.part");
    const QString result = QDir(outputDir).filePath("refined-mesh.ply");
    if (!safeEntry(outputDir, "refined-mesh.ply.part")
        || !safeEntry(outputDir, "refined-mesh.ply"))
        return fail(jobId, "unsafe result path");
    if (!writeStatus(jobId, "processing"))
        return fail(jobId, "cannot publish processing status");
    log(jobId, "processing");
    // A crash after publishing the model but before completed can be recovered
    // without copying again; never replace an unrelated existing result.
    if (QFileInfo::exists(result)) {
        if (QFileInfo(result).size() != static_cast<qint64>(size)
            || hashFile(result) != expectedHash)
            return fail(jobId, "existing result does not match request");
    } else {
        if (!processModel(inputMesh, staging))
            return fail(jobId, "model processing failed");
        if (QFileInfo(staging).size() != static_cast<qint64>(size)
            || hashFile(staging) != expectedHash)
            return fail(jobId, "copied model verification failed");
        // No COPY_ALLOWED / REPLACE_EXISTING: a same-directory atomic rename,
        // with no QFile::rename copy-and-delete fallback on failure.
        if (!MoveFileExW(reinterpret_cast<LPCWSTR>(staging.utf16()),
                         reinterpret_cast<LPCWSTR>(result.utf16()), MOVEFILE_WRITE_THROUGH))
            return fail(jobId, "atomic result publication failed");
    }
    if (!writeStatus(jobId, "completed", expectedHash))
        return fail(jobId, "cannot publish completed status");
    log(jobId, "completed");
    return Outcome::Completed;
}

JobProcessor::Outcome JobProcessor::scanOnce()
{
    static const QRegularExpression idPattern("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$");
    const auto jobs = QDir(m_inbox).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &jobId : jobs) {
        if (!idPattern.match(jobId).hasMatch())
            continue;
        const auto outcome = inspectJob(jobId);
        if (outcome != Outcome::Idle)
            return outcome; // At most one eligible task per scan.
    }
    return Outcome::Idle;
}
