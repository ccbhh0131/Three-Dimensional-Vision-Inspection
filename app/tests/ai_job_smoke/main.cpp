#include "core/ai/AiJobClient.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QTextStream>
#include <stdexcept>

namespace {
void require(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

QString sha256(const QString &path)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    require(file.open(QIODevice::ReadOnly) && hash.addData(&file), "Cannot hash file");
    return QString::fromLatin1(hash.result().toHex());
}

void write(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit(), "Cannot publish smoke artifact");
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 4) {
        QTextStream(stderr) << "Usage: Vision3DAiJobSmoke WORKER_EXE EXISTING_MODEL FRESH_WORKSPACE" << Qt::endl;
        return 1;
    }
    try {
        const QString workerPath = QFileInfo(args[1]).absoluteFilePath();
        const QString modelPath = QFileInfo(args[2]).absoluteFilePath();
        const QString workspace = QDir::fromNativeSeparators(args[3]);
        require(QDir::isAbsolutePath(workspace) && !QFileInfo::exists(workspace), "Use a fresh absolute workspace");
        const QString sendRoot = workspace + "/client_to_ai";
        const QString receiveRoot = workspace + "/ai_to_client";
        const QString hostOut = workspace + "/host/ai_to_client";
        require(QDir().mkpath(sendRoot) && QDir().mkpath(receiveRoot) && QDir().mkpath(hostOut), "Cannot create workspace");
        const QString inputHash = sha256(modelPath);
        const qint64 inputSize = QFileInfo(modelPath).size();
        AiJobClient client(sendRoot, receiveRoot);
        QString error;
        const auto submission = client.submit(modelPath, error);
        require(!submission.jobId.isEmpty(), "Client submission failed");
        const QString jobId = submission.jobId;
        require(submission.sha256 == inputHash && submission.meshSize == inputSize, "Submission metadata mismatch");
        require(QFileInfo::exists(sendRoot + '/' + jobId + "/READY"), "READY not published");
        require(sha256(sendRoot + '/' + jobId + "/final-mesh.ply") == inputHash, "Published model differs");
        require(client.checkResult(jobId).state == AiJobClient::State::Waiting, "Publication was mistaken for completion");

        QProcess worker;
        worker.setProgram(workerPath);
        worker.setArguments({"--inbox", sendRoot, "--outbox", hostOut, "--once"});
        worker.setWorkingDirectory(workspace);
        worker.start();
        require(worker.waitForStarted(10000), "Existing Worker failed to start");
        if (!worker.waitForFinished(30000)) {
            worker.kill();
            worker.waitForFinished();
            throw std::runtime_error("Worker timeout");
        }
        write(workspace + "/worker.log", worker.readAllStandardOutput() + worker.readAllStandardError());
        require(worker.exitStatus() == QProcess::NormalExit && worker.exitCode() == 0, "Worker did not complete the Job");

        // Local delivery simulation, not a network test: completed arrives
        // before its PLY, then a partial PLY, then the whole verified result.
        const QString receivedJob = receiveRoot + '/' + jobId;
        const QString hostJob = hostOut + '/' + jobId;
        require(QDir().mkpath(receivedJob), "Cannot create receive Job directory");
        require(QFile::copy(hostJob + "/response.json", receivedJob + "/response.json"), "Cannot deliver response");
        auto result = client.checkResult(jobId);
        require(result.state == AiJobClient::State::Waiting && result.filePath.isEmpty(), "Accepted response without model");
        QFile hostResult(hostJob + "/refined-mesh.ply");
        require(hostResult.open(QIODevice::ReadOnly), "Cannot read Worker output");
        write(receivedJob + "/refined-mesh.ply", hostResult.read(4096));
        result = client.checkResult(jobId);
        require(result.state == AiJobClient::State::Waiting && result.filePath.isEmpty(), "Accepted incomplete model");
        require(hostResult.seek(0), "Cannot rewind output");
        write(receivedJob + "/refined-mesh.ply", hostResult.readAll());
        result = client.checkResult(jobId);
        require(result.state == AiJobClient::State::Completed && !result.filePath.isEmpty(), "Client did not verify complete result");
        require(result.fileSize == inputSize && result.sha256 == inputHash, "Copy-mode end-to-end mismatch");
        require(sha256(modelPath) == inputHash, "Original model changed");

        const QJsonObject report{
            {"result", "PASS"}, {"jobId", jobId}, {"protocolVersion", 1},
            {"inputSize", inputSize}, {"outputSize", result.fileSize},
            {"inputSha256", inputHash}, {"outputSha256", result.sha256},
            {"submissionNotCompletion", true}, {"responseBeforeModelWait", true},
            {"partialResultWait", true}, {"clientVerifiedResult", true},
            {"workerExitCode", worker.exitCode()}, {"realNetworkTest", false}
        };
        write(workspace + "/smoke-result.json", QJsonDocument(report).toJson());
        QTextStream(stdout) << QJsonDocument(report).toJson() << Qt::endl;
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "FAIL: " << error.what() << Qt::endl;
        return 1;
    }
}
