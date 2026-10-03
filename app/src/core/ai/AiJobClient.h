#pragma once

#include <QString>

// Filesystem transport only. The caller owns polling and any UI integration.
class AiJobClient
{
public:
    struct Submission {
        QString jobId; // Empty means submission failed; READY was not published.
        qint64 meshSize = 0;
        QString sha256;
    };
    enum class State { Waiting, Queued, Processing, Completed, Failed };
    struct Result {
        State state = State::Waiting;
        QString filePath; // Only set after completed AND local verification.
        qint64 fileSize = 0;
        QString sha256;
        QString message;
    };

    // Both roots must be existing, absolute and disjoint directories.
    AiJobClient(const QString &clientToAi, const QString &aiToClient);
    Submission submit(const QString &inputModel, QString &error) const;
    Result checkResult(const QString &jobId) const;

private:
    QString m_sendRoot;
    QString m_receiveRoot;
    QString m_configurationError;
};
