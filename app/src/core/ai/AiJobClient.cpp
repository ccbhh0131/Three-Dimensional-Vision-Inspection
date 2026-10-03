#include "AiJobClient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <cmath>

namespace {
bool within(const QString &path, const QString &root)
{
    const QString prefix = root.endsWith('/') ? root : root + '/';
    return path.compare(root, Qt::CaseInsensitive) == 0
        || path.startsWith(prefix, Qt::CaseInsensitive);
}

// All callers supply validated single path components. Resolve junctions too.
bool safeEntry(const QString &parent, const QString &name)
{
    const QString expected = QDir(parent).filePath(name);
    const QFileInfo info(expected);
    return !info.isSymLink() && (!info.exists()
        || info.canonicalFilePath().compare(expected, Qt::CaseInsensitive) == 0);
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError && doc.isObject() ? doc.object() : QJsonObject{};
}

bool publish(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path); // Temporary file stays alongside its final destination.
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

QString hashFile(const QString &path)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

qint64 positiveSize(const QJsonValue &value)
{
    const double size = value.toDouble(-1);
    return std::isfinite(size) && size > 0 && size <= 9007199254740991.0
        && std::floor(size) == size ? static_cast<qint64>(size) : -1;
}

AiJobClient::Result state(AiJobClient::State value, const QString &message = {})
{
    AiJobClient::Result result;
    result.state = value;
    result.message = message;
    return result;
}
}

AiJobClient::AiJobClient(const QString &clientToAi, const QString &aiToClient)
{
    const QFileInfo send(clientToAi), receive(aiToClient);
    if (!send.isAbsolute() || !receive.isAbsolute() || !send.isDir() || !receive.isDir()) {
        m_configurationError = "Supply two existing absolute transport directories";
        return;
    }
    m_sendRoot = send.canonicalFilePath();
    m_receiveRoot = receive.canonicalFilePath();
    if (m_sendRoot.isEmpty() || m_receiveRoot.isEmpty()
        || within(m_sendRoot, m_receiveRoot) || within(m_receiveRoot, m_sendRoot))
        m_configurationError = "Transport directories must be disjoint";
}

AiJobClient::Submission AiJobClient::submit(const QString &inputModel, QString &error) const
{
    error.clear();
    if (!m_configurationError.isEmpty()) {
        error = m_configurationError;
        return {};
    }
    QFile input(inputModel);
    if (!QFileInfo(inputModel).isFile() || !input.open(QIODevice::ReadOnly) || input.size() <= 0) {
        error = "Cannot read a non-empty input model";
        return {};
    }
    const QString jobId = "AIR-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir(m_sendRoot).mkdir(jobId)) {
        error = "Cannot create unique job directory";
        return {};
    }
    const QString jobDir = QDir(m_sendRoot).filePath(jobId);
    const QString mesh = QDir(jobDir).filePath("final-mesh.ply");
    QSaveFile output(mesh);
    if (!output.open(QIODevice::WriteOnly)) {
        error = "Cannot stage input model";
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 size = 0;
    while (!input.atEnd()) {
        const auto block = input.read(1024 * 1024);
        if (input.error() != QFileDevice::NoError || block.isEmpty() || output.write(block) != block.size()) {
            error = "Input model copy failed";
            return {};
        }
        hash.addData(block);
        size += block.size();
    }
    const QString sha256 = QString::fromLatin1(hash.result().toHex());
    if (size <= 0 || size != input.size() || !output.commit()
        || QFileInfo(mesh).size() != size || hashFile(mesh) != sha256) {
        error = "Staged model verification failed";
        return {};
    }
    const QJsonObject request{
        {"protocolVersion", 1}, {"jobId", jobId}, {"type", "mesh-copy"},
        {"meshFile", "final-mesh.ply"}, {"meshSize", size}, {"sha256", sha256}
    };
    if (!publish(QDir(jobDir).filePath("request.json"), QJsonDocument(request).toJson())
        || !publish(QDir(jobDir).filePath("READY"), {})) {
        error = "Cannot publish request and READY";
        return {};
    }
    return {jobId, size, sha256}; // Locally published, NOT remotely delivered.
}

AiJobClient::Result AiJobClient::checkResult(const QString &jobId) const
{
    if (!m_configurationError.isEmpty())
        return state(State::Failed, m_configurationError);
    static const QRegularExpression idPattern("^[A-Za-z0-9][A-Za-z0-9_-]{0,63}$");
    static const QRegularExpression filePattern("^[A-Za-z0-9][A-Za-z0-9_-]*\\.ply$");
    static const QRegularExpression hashPattern("^[0-9a-f]{64}$");
    if (!idPattern.match(jobId).hasMatch() || !safeEntry(m_sendRoot, jobId)
        || !safeEntry(m_receiveRoot, jobId))
        return state(State::Failed, "Invalid job path");
    const QString sentDir = QDir(m_sendRoot).filePath(jobId);
    const QString receivedDir = QDir(m_receiveRoot).filePath(jobId);
    if (!safeEntry(sentDir, "request.json") || !safeEntry(receivedDir, "response.json"))
        return state(State::Failed, "Invalid manifest path");
    const auto request = readJson(QDir(sentDir).filePath("request.json"));
    if (request.value("jobId").toString() != jobId || request.value("protocolVersion").toDouble() != 1)
        return state(State::Failed, "Unknown local request");
    const auto response = readJson(QDir(receivedDir).filePath("response.json"));
    if (response.isEmpty())
        return state(State::Waiting, "Waiting for complete response");
    if (response.value("protocolVersion").toDouble() != 1 || response.value("jobId").toString() != jobId)
        return state(State::Failed, "Response protocol or job ID mismatch");
    const QString status = response.value("status").toString();
    if (status == "queued")
        return state(State::Queued);
    if (status == "processing")
        return state(State::Processing);
    if (status == "failed")
        return state(State::Failed, response.value("error").toString().left(256));
    if (status != "completed")
        return state(State::Failed, "Unsupported response status");

    const QString name = response.value("resultFile").toString();
    const QString expectedHash = response.value("sha256").toString().toLower();
    if (!filePattern.match(name).hasMatch() || !hashPattern.match(expectedHash).hasMatch()
        || !safeEntry(receivedDir, name))
        return state(State::Failed, "Invalid result manifest or path");
    // Stage 7-1B v1 responses have no size field. A mesh-copy result preserves
    // request size; future non-copy processing must supply response.resultSize.
    const qint64 expectedSize = response.contains("resultSize")
        ? positiveSize(response.value("resultSize"))
        : request.value("type").toString() == "mesh-copy"
            ? positiveSize(request.value("meshSize")) : -1;
    if (expectedSize <= 0)
        return state(State::Failed, "Response needs a valid resultSize");
    const QString path = QDir(receivedDir).filePath(name);
    const QFileInfo file(path);
    if (!file.isFile() || file.size() != expectedSize)
        return state(State::Waiting, "Waiting for complete result model");
    const QString actualHash = hashFile(path);
    // Deliberately compare with the response, never the original input hash.
    if (actualHash != expectedHash)
        return state(State::Waiting, "Waiting for result matching response SHA-256");
    Result result;
    result.state = State::Completed;
    result.filePath = path;
    result.fileSize = file.size();
    result.sha256 = actualHash;
    return result;
}
