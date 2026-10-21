import QtQuick 2.15
import ".."
import "../components"

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.height + 32
        clip: true
        Column {
            id: contentColumn
            x: 16
            y: 16
            width: root.width - 32
            spacing: 16
            Row {
                width: parent.width
                Column {
                    width: parent.width - stateChip.width - 12
                    spacing: 4
                    Text {
                        text: "实时监控"
                        color: theme.textPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "模拟传感器与 Modbus TCP 共用现有数据源控制器。"
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                    }
                }
                StatusChip { id: stateChip; text: viewModel.realtimeStateText; statusCode: viewModel.realtimeRunning ? "running" : "unknown"; theme: theme; anchors.verticalCenter: parent.verticalCenter }
            }
            Row {
                width: parent.width
                spacing: 16
                SectionCard {
                    width: (parent.width - 16) * 0.58
                    height: 246
                    theme: theme
                    PanelTitle { title: "当前值"; detail: viewModel.deviceName; theme: theme }
                    Row {
                        spacing: 8
                        Text {
                            text: viewModel.realtimeValueText === "—" ? "—" : viewModel.realtimeValueText.split(" ")[0]
                            color: theme.textPrimary
                            font.family: theme.fontFamily
                            font.pixelSize: 46
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: viewModel.readingUnit
                            color: theme.textSecondary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 14
                            anchors.baseline: parent.children[0].baseline
                        }
                    }
                    Text { text: "数据源 · " + viewModel.realtimeSourceText; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12 }
                    Row {
                        spacing: 8
                        AppButton {
                            text: "启动模拟"
                            compact: true
                            enabled: viewModel.gaugeId.length > 0 && !viewModel.realtimeRunning
                            theme: theme
                            onClicked: viewModel.requestStartRealtime()
                        }
                        SecondaryButton {
                            text: "停止"
                            compact: true
                            enabled: viewModel.realtimeRunning
                            theme: theme
                            onClicked: viewModel.requestStopRealtime()
                        }
                        GhostButton {
                            text: "记录当前值"
                            theme: theme
                            enabled: viewModel.realtimeRunning
                            onClicked: viewModel.requestRecordRealtime()
                        }
                    }
                }
                SectionCard {
                    width: (parent.width - 16) * 0.42
                    height: 246
                    theme: theme
                    PanelTitle { title: "连接信息"; theme: theme }
                    PropertyRow { label: "状态"; value: viewModel.realtimeConnectionText; theme: theme }
                    PropertyRow { label: "来源"; value: viewModel.realtimeSourceText; theme: theme }
                    PropertyRow { label: "主机"; value: viewModel.realtimeHostText; theme: theme }
                    PropertyRow { label: "寄存器"; value: viewModel.realtimeRegisterText; theme: theme }
                    PropertyRow { label: "轮询"; value: viewModel.realtimePollingText; theme: theme }
                }
            }
            SectionCard {
                width: parent.width
                height: 136
                theme: theme
                PanelTitle { title: "状态说明"; theme: theme }
                Text { text: "实时状态、采样、持久化和 Modbus TCP 连接仍由 RealtimeMonitoringController 管理。"; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
    }
}
