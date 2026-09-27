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

    function selectPage(page) {
        root.currentPage = page;
    }
    function requestCreateProject() {}
    function requestOpenProject() {}
    function requestImportImages() {}
    function requestOpenViewer() {}
    function requestResetViewer() {}
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
    function refresh() {}
}
