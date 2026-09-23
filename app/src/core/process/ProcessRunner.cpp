#include "core/process/ProcessRunner.h"

#include <QTimer>

namespace vision3d {

ProcessRunner::ProcessRunner(QObject* parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::started, this, &ProcessRunner::started);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        emit stdoutReady(QString::fromUtf8(m_process.readAllStandardOutput()));
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        emit stderrReady(QString::fromUtf8(m_process.readAllStandardError()));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit failed(m_process.errorString());
    });
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                const QByteArray output = m_process.readAllStandardOutput();
                const QByteArray error = m_process.readAllStandardError();
                if (!output.isEmpty()) {
                    emit stdoutReady(QString::fromUtf8(output));
                }
                if (!error.isEmpty()) {
                    emit stderrReady(QString::fromUtf8(error));
                }
                emit finished(exitCode, exitStatus);
            });
}

ProcessRunner::~ProcessRunner()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(5000);
    }
}

bool ProcessRunner::start(const QString& program,
                          const QStringList& arguments,
                          const QString& workingDirectory,
                          QString* error)
{
    if (m_process.state() != QProcess::NotRunning) {
        if (error != nullptr) {
            *error = QStringLiteral("已有进程正在运行。");
        }
        return false;
    }

    m_process.setWorkingDirectory(workingDirectory);
    m_process.setProgram(program);
    m_process.setArguments(arguments);
    m_process.start();
    return true;
}

bool ProcessRunner::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

void ProcessRunner::requestCancel(int terminateWaitMilliseconds)
{
    if (!isRunning()) {
        return;
    }

    m_process.terminate();
    const int boundedWait = qBound(0, terminateWaitMilliseconds, 10000);
    QTimer::singleShot(boundedWait, this, [this] {
        if (isRunning()) {
            m_process.kill();
        }
    });
}

qint64 ProcessRunner::processId() const
{
    return m_process.processId();
}

ProcessResult ProcessRunner::runBlocking(const QString& program,
                                         const QStringList& arguments,
                                         const QString& workingDirectory,
                                         int timeoutMilliseconds) const
{
    ProcessResult result;
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setWorkingDirectory(workingDirectory);
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();

    if (!process.waitForStarted(5000)) {
        result.processError = process.errorString();
        return result;
    }
    result.started = true;

    if (!process.waitForFinished(timeoutMilliseconds)) {
        result.timedOut = true;
        result.processError = QStringLiteral("进程在 %1 ms 内未完成。").arg(timeoutMilliseconds);
        process.kill();
        process.waitForFinished(5000);
    }

    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.standardOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.standardError = QString::fromUtf8(process.readAllStandardError());
    if (result.processError.isEmpty() && process.error() != QProcess::UnknownError
        && result.exitStatus == QProcess::CrashExit) {
        result.processError = process.errorString();
    }
    return result;
}

} // namespace vision3d
