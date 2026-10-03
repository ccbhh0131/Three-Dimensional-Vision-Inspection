#pragma once

#include <QHash>
#include <QString>

// Processing is synchronous and serial; main.cpp owns the polling loop.
class JobProcessor
{
public:
    enum class Outcome { Idle, Completed, Failed };
    JobProcessor(QString inbox, QString outbox);
    Outcome scanOnce();

private:
    Outcome inspectJob(const QString &jobId);
    bool writeStatus(const QString &jobId, const QString &status,
                     const QString &sha256 = {}, const QString &error = {});
    Outcome fail(const QString &jobId, const QString &error);
    void log(const QString &jobId, const QString &message);

    // Replace this one function with the future Blender AI workflow.
    // Write only to outputMesh; the caller verifies and publishes the result.
    static bool processModel(const QString &inputMesh, const QString &outputMesh);
    QString m_inbox;
    QString m_outbox;
    QHash<QString, QString> m_lastMessage;
};
