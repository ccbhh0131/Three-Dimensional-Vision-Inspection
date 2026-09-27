#pragma once

#include <QObject>
#include <QVariantList>

class QString;

namespace vision3d {

class ProjectManager;
namespace realtime {
class RealtimeMonitoringController;
}

// Presentation-only adapter for the Stage 5A QML shell.  Domain operations
// remain owned by MainWindow and the existing project/realtime controllers.
class AppShellViewModel final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QString pageTitle READ pageTitle NOTIFY dataChanged)
    Q_PROPERTY(bool hasProject READ hasProject NOTIFY dataChanged)
    Q_PROPERTY(QString projectName READ projectName NOTIFY dataChanged)
    Q_PROPERTY(QString projectSummary READ projectSummary NOTIFY dataChanged)
    Q_PROPERTY(int imageCount READ imageCount NOTIFY dataChanged)
    Q_PROPERTY(int markerCount READ markerCount NOTIFY dataChanged)
    Q_PROPERTY(int gaugeCount READ gaugeCount NOTIFY dataChanged)
    Q_PROPERTY(int inspectionCount READ inspectionCount NOTIFY dataChanged)
    Q_PROPERTY(int alarmCount READ alarmCount NOTIFY dataChanged)
    Q_PROPERTY(QString reconstructionText READ reconstructionText NOTIFY dataChanged)
    Q_PROPERTY(QString reconstructionStageText READ reconstructionStageText NOTIFY dataChanged)
    Q_PROPERTY(int reconstructionProgress READ reconstructionProgress NOTIFY dataChanged)
    Q_PROPERTY(QString reconstructionArtifactText READ reconstructionArtifactText NOTIFY dataChanged)
    Q_PROPERTY(QString engineText READ engineText NOTIFY dataChanged)
    Q_PROPERTY(QString viewerText READ viewerText NOTIFY dataChanged)

    Q_PROPERTY(QString deviceName READ deviceName NOTIFY dataChanged)
    Q_PROPERTY(QString gaugeName READ gaugeName NOTIFY dataChanged)
    Q_PROPERTY(QString gaugeId READ gaugeId NOTIFY dataChanged)
    Q_PROPERTY(QString currentReadingText READ currentReadingText NOTIFY dataChanged)
    Q_PROPERTY(QString readingUnit READ readingUnit NOTIFY dataChanged)
    Q_PROPERTY(QString readingSourceText READ readingSourceText NOTIFY dataChanged)
    Q_PROPERTY(QString gaugeStatusText READ gaugeStatusText NOTIFY dataChanged)
    Q_PROPERTY(QString gaugeStatusCode READ gaugeStatusCode NOTIFY dataChanged)
    Q_PROPERTY(QString rangeText READ rangeText NOTIFY dataChanged)
    Q_PROPERTY(QString profileText READ profileText NOTIFY dataChanged)
    Q_PROPERTY(QString markerPositionText READ markerPositionText NOTIFY dataChanged)
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY dataChanged)
    Q_PROPERTY(QString historyCountText READ historyCountText NOTIFY dataChanged)

    Q_PROPERTY(QString realtimeStateText READ realtimeStateText NOTIFY dataChanged)
    Q_PROPERTY(QString realtimeValueText READ realtimeValueText NOTIFY dataChanged)
    Q_PROPERTY(QString realtimeSourceText READ realtimeSourceText NOTIFY dataChanged)
    Q_PROPERTY(QString realtimeConnectionText READ realtimeConnectionText NOTIFY dataChanged)
    Q_PROPERTY(QString realtimeHostText READ realtimeHostText NOTIFY dataChanged)
    Q_PROPERTY(QString realtimeRegisterText READ realtimeRegisterText NOTIFY dataChanged)
    Q_PROPERTY(QString realtimePollingText READ realtimePollingText NOTIFY dataChanged)
    Q_PROPERTY(bool realtimeRunning READ realtimeRunning NOTIFY dataChanged)

    Q_PROPERTY(QVariantList historyRows READ historyRows NOTIFY dataChanged)
    Q_PROPERTY(QString visualImageSource READ visualImageSource NOTIFY dataChanged)
    Q_PROPERTY(QString statusBarText READ statusBarText NOTIFY dataChanged)

public:
    explicit AppShellViewModel(ProjectManager* projectManager,
                               realtime::RealtimeMonitoringController* realtimeController,
                               QObject* parent = nullptr);

    QString currentPage() const;
    QString pageTitle() const;
    bool hasProject() const;
    QString projectName() const;
    QString projectSummary() const;
    int imageCount() const;
    int markerCount() const;
    int gaugeCount() const;
    int inspectionCount() const;
    int alarmCount() const;
    QString reconstructionText() const;
    QString reconstructionStageText() const;
    int reconstructionProgress() const;
    QString reconstructionArtifactText() const;
    QString engineText() const;
    QString viewerText() const;

    QString deviceName() const;
    QString gaugeName() const;
    QString gaugeId() const;
    QString currentReadingText() const;
    QString readingUnit() const;
    QString readingSourceText() const;
    QString gaugeStatusText() const;
    QString gaugeStatusCode() const;
    QString rangeText() const;
    QString profileText() const;
    QString markerPositionText() const;
    QString updatedText() const;
    QString historyCountText() const;

    QString realtimeStateText() const;
    QString realtimeValueText() const;
    QString realtimeSourceText() const;
    QString realtimeConnectionText() const;
    QString realtimeHostText() const;
    QString realtimeRegisterText() const;
    QString realtimePollingText() const;
    bool realtimeRunning() const;

    QVariantList historyRows() const;
    QString visualImageSource() const;
    QString statusBarText() const;

    Q_INVOKABLE void selectPage(const QString& page);
    Q_INVOKABLE void requestCreateProject();
    Q_INVOKABLE void requestOpenProject();
    Q_INVOKABLE void requestImportImages();
    Q_INVOKABLE void requestOpenViewer();
    Q_INVOKABLE void requestResetViewer();
    Q_INVOKABLE void requestVisualReading();
    Q_INVOKABLE void requestManualReading();
    Q_INVOKABLE void requestShowHistory();
    Q_INVOKABLE void requestCreateGauge();
    Q_INVOKABLE void requestEditGauge();
    Q_INVOKABLE void requestConfigureRule();
    Q_INVOKABLE void requestStartRealtime();
    Q_INVOKABLE void requestStopRealtime();
    Q_INVOKABLE void requestRecordRealtime();
    Q_INVOKABLE void requestSettings();
    Q_INVOKABLE void refresh();

    void setSelectedMarkerId(const QString& markerId);

signals:
    void currentPageChanged();
    void dataChanged();
    void markerSelectionRequested(const QString& markerId);
    void createProjectRequested();
    void openProjectRequested();
    void importImagesRequested();
    void openViewerRequested();
    void resetViewerRequested();
    void visualReadingRequested();
    void manualReadingRequested();
    void showHistoryRequested();
    void createGaugeRequested();
    void editGaugeRequested();
    void configureRuleRequested();
    void startRealtimeRequested();
    void stopRealtimeRequested();
    void recordRealtimeRequested();
    void settingsRequested();

private:
    ProjectManager* m_projectManager = nullptr;
    realtime::RealtimeMonitoringController* m_realtimeController = nullptr;
    QString m_currentPage = QStringLiteral("overview");
    QString m_selectedMarkerId;

    bool m_hasProject = false;
    QString m_projectName;
    QString m_projectSummary;
    int m_imageCount = 0;
    int m_markerCount = 0;
    int m_gaugeCount = 0;
    int m_inspectionCount = 0;
    int m_alarmCount = 0;
    QString m_reconstructionText;
    QString m_reconstructionStageText;
    int m_reconstructionProgress = 0;
    QString m_reconstructionArtifactText;
    QString m_engineText;
    QString m_viewerText;

    QString m_deviceName;
    QString m_gaugeName;
    QString m_gaugeId;
    QString m_currentReadingText;
    QString m_readingUnit;
    QString m_readingSourceText;
    QString m_gaugeStatusText;
    QString m_gaugeStatusCode;
    QString m_rangeText;
    QString m_profileText;
    QString m_markerPositionText;
    QString m_updatedText;
    QString m_historyCountText;

    QString m_realtimeStateText;
    QString m_realtimeValueText;
    QString m_realtimeSourceText;
    QString m_realtimeConnectionText;
    QString m_realtimeHostText;
    QString m_realtimeRegisterText;
    QString m_realtimePollingText;
    bool m_realtimeRunning = false;

    QVariantList m_historyRows;
    QString m_visualImageSource;
    QString m_statusBarText;
};

} // namespace vision3d
