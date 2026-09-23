#pragma once

#include "backend/ColmapBackend.h"
#include "core/process/ProcessRunner.h"
#include "core/project/ProjectManager.h"
#include "core/reconstruction/ReconstructionArtifactValidator.h"
#include "core/reconstruction/ReconstructionTask.h"
#include "core/reconstruction/ReconstructionWorkspace.h"

#include <QElapsedTimer>
#include <QFile>
#include <QObject>

namespace vision3d {

class ReconstructionController final : public QObject
{
    Q_OBJECT

public:
    explicit ReconstructionController(ProjectManager* projectManager,
                                      ColmapBackend* backend,
                                      ProcessRunner* processRunner = nullptr,
                                      QObject* parent = nullptr);

    bool start(const QString& backendRoot, QString* error = nullptr);
    void cancel();
    bool isRunning() const;
    const ReconstructionTask& currentTask() const;
    const ReconstructionJobPaths& jobPaths() const;
    qint64 activeProcessId() const;
    static int completedStageCount(ReconstructionStage stage);

    struct StageDefinition
    {
        ReconstructionStage stage;
        QString logName;
    };

signals:
    void taskChanged();
    void logMessage(const QString& text, bool isError);
    void finished(bool success);

private slots:
    void onProcessStarted();
    void onProcessStdout(const QString& text);
    void onProcessStderr(const QString& text);
    void onProcessFailed(const QString& message);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class ProcessPurpose
    {
        None,
        Probe,
        Stage,
    };

    bool validateStartPreconditions(const QString& backendRoot,
                                   QList<AssetRecord>* assets,
                                   QString* error) const;
    void beginProbe();
    void beginStage(int index);
    ProcessCommand commandForStage(ReconstructionStage stage) const;
    void openLog(const QString& fileName, const ProcessCommand& command);
    void closeLog();
    void appendLog(const QString& text);
    void setTaskChanged();
    bool persistTask(QString* error = nullptr);
    bool validateStageArtifact(ReconstructionStage stage, QString* error);
    bool selectPrimaryModel(QString* error);
    void failTask(const QString& message, int exitCode);
    void cancelTask();
    void completeTask();
    static QString stageName(ReconstructionStage stage);

    ProjectManager* m_projectManager;
    ColmapBackend* m_backend;
    ProcessRunner* m_processRunner;
    ProcessRunner* m_ownedProcessRunner = nullptr;
    ReconstructionTask m_task;
    ReconstructionJobPaths m_jobPaths;
    QList<AssetRecord> m_assets;
    QString m_backendRoot;
    ReconstructionConfig m_config;
    ProcessPurpose m_processPurpose = ProcessPurpose::None;
    int m_stageIndex = -1;
    bool m_cancelRequested = false;
    QString m_processError;
    QString m_probeStdout;
    QString m_probeStderr;
    QFile m_stageLog;
    QElapsedTimer m_totalTimer;
    QElapsedTimer m_stageTimer;
};

} // namespace vision3d
