#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

namespace vision3d {

struct ProcessResult
{
    bool started = false;
    bool timedOut = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    QString standardOutput;
    QString standardError;
    QString processError;
};

class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit ProcessRunner(QObject* parent = nullptr);
    ~ProcessRunner() override;

    virtual bool start(const QString& program,
                       const QStringList& arguments,
                       const QString& workingDirectory,
                       QString* error = nullptr);
    virtual bool isRunning() const;
    virtual void requestCancel(int terminateWaitMilliseconds = 1500);
    virtual qint64 processId() const;

    ProcessResult runBlocking(const QString& program,
                              const QStringList& arguments,
                              const QString& workingDirectory,
                              int timeoutMilliseconds = 15000) const;

signals:
    void started();
    void stdoutReady(const QString& text);
    void stderrReady(const QString& text);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void failed(const QString& message);

private:
    QProcess m_process;
};

} // namespace vision3d
