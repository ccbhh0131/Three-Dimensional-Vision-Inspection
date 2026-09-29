import QtQml 2.15

// A presentation-only placeholder used for the short QML construction
// window before MainWindow injects the real AppShellViewModel.  It contains
// no project, gauge, inspection, or realtime logic.
QtObject {
    id: root

    property string currentPage: "overview"
    property string pageTitle: "项目总览"
    property bool hasProject: false
    property string projectName: "未打开项目"
    property string projectSummary: "等待打开项目"
    property int imageCount: 0
    property int markerCount: 0
    property int gaugeCount: 0
    property int inspectionCount: 0
    property int alarmCount: 0
    property string reconstructionText: "未开始"
    property string reconstructionStageText: "—"
    property int reconstructionProgress: 0
    property string reconstructionArtifactText: "—"
    property string engineText: "—"
    property string viewerText: "等待模型"
    property bool alignmentEditing: false
    property bool canAddMarker: false
    property bool canDeleteMarker: false

    property string deviceName: "未选择设备"
    property string gaugeName: "未绑定仪表"
    property string gaugeId: ""
    property string currentReadingText: "—"
    property string readingUnit: ""
    property string readingSourceText: "—"
    property string gaugeStatusText: "未知"
    property string gaugeStatusCode: "unknown"
    property string rangeText: "—"
    property string profileText: "—"
    property string markerPositionText: "—"
    property string updatedText: "—"
    property string historyCountText: "0 条记录"

    property string realtimeStateText: "Stopped"
    property string realtimeValueText: "—"
    property string realtimeSourceText: "未启动"
    property string realtimeConnectionText: "未连接"
    property string realtimeHostText: "—"
    property string realtimeRegisterText: "—"
    property string realtimePollingText: "—"
    property bool realtimeRunning: false

    property var historyRows: []
    property string visualImageSource: ""
    property string statusBarText: "就绪"
    property string defaultProjectDirectory: ""
    property string lastProjectDirectory: ""
    property bool restoreLastProject: false
    property bool rememberLastDirectory: true
    property bool autoFit: true
    property real orbitSensitivity: 1.0
    property bool showMarkers: true
    property real markerSize: 1.0
    property int realtimePollIntervalMs: 500
    property string productVersion: "Development"
    property string buildType: "Release"
    property string configDirectory: ""

    function selectPage(page) {
        root.currentPage = page;
    }
    function requestCreateProject() {}
    function requestOpenProject() {}
    function requestImportImages() {}
    function requestOpenViewer() {}
    function requestResetViewer() {}
    function requestCameraView(view) {}
    function requestBeginSceneAlignment() {}
    function requestSceneAlignmentRotation(axis, degrees) {}
    function requestResetSceneAlignment() {}
    function requestCancelSceneAlignment() {}
    function requestSaveSceneAlignment() {}
    function requestAddMarker() {}
    function requestDeleteMarker() {}
    function requestVisualReading() {}
    function requestManualReading() {}
    function requestShowHistory() {}
    function requestCreateGauge() {}
    function requestEditGauge() {}
    function requestConfigureRule() {}
    function requestStartRealtime() {}
    function requestStopRealtime() {}
    function requestRecordRealtime() {}
    function requestSettings() {}
    function requestSelectDefaultProjectDirectory() {}
    function requestOpenThirdPartyLicenses() {}
    function requestResetPreferences() {}
    function setRestoreLastProject(enabled) { root.restoreLastProject = enabled; }
    function setRememberLastDirectory(enabled) { root.rememberLastDirectory = enabled; }
    function setAutoFit(enabled) { root.autoFit = enabled; }
    function setOrbitSensitivity(value) { root.orbitSensitivity = value; }
    function setShowMarkers(enabled) { root.showMarkers = enabled; }
    function setMarkerSize(value) { root.markerSize = value; }
    function setRealtimePollIntervalMs(value) { root.realtimePollIntervalMs = value; }
    function setDefaultProjectDirectory(value) { root.defaultProjectDirectory = value; }
    function requestFitViewer() {}
    function refresh() {}
}
