import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }

    Rectangle {
        anchors.fill: parent
        color: theme.surface
        border.color: theme.border
        border.width: 1
    }
    Flickable {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        anchors.rightMargin: 28
        contentWidth: width
        contentHeight: contentColumn.height
        clip: true
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 10 }

        Column {
            id: contentColumn
            width: parent.width
            spacing: 14

            Row {
                width: parent.width
                spacing: 10
                Column {
                    width: parent.width - statusChip.width - 10
                    spacing: 3
                    Text { text: "设备检查器"; color: theme.textPrimary; font.family: theme.cjkFontFamily; font.pixelSize: 18; font.weight: Font.DemiBold }
                    Text { text: viewModel.deviceName; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; elide: Text.ElideRight; width: parent.width }
                }
                StatusChip { id: statusChip; text: viewModel.gaugeStatusText; statusCode: viewModel.gaugeStatusCode; theme: theme; anchors.verticalCenter: parent.verticalCenter }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "当前读数"; detail: viewModel.updatedText === "—" ? "未更新" : viewModel.updatedText; theme: theme }
                Row {
                    width: parent.width
                    spacing: 8
                    Text { text: viewModel.currentReadingText; color: theme.textPrimary; font.family: theme.fontFamily; font.pixelSize: 38; font.weight: Font.DemiBold }
                    Text { text: viewModel.readingUnit; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 14; anchors.baseline: parent.children[0].baseline }
                }
                Text { text: "来源 · " + viewModel.readingSourceText; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12 }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "资产属性"; theme: theme }
                PropertyRow { label: "仪表名称"; value: viewModel.gaugeName; theme: theme }
                PropertyRow { label: "量程"; value: viewModel.rangeText; theme: theme }
                PropertyRow { label: "视觉 Profile"; value: viewModel.profileText; theme: theme }
                PropertyRow { label: "世界坐标"; value: viewModel.markerPositionText; theme: theme }
                PropertyRow { label: "历史记录"; value: viewModel.historyCountText; theme: theme }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "快捷操作"; theme: theme }
                Row {
                    spacing: 8
                    AppButton { text: "视觉读数"; compact: true; enabled: viewModel.gaugeId.length > 0; theme: theme; onClicked: viewModel.requestVisualReading() }
                    SecondaryButton { text: "手动更新"; compact: true; enabled: viewModel.gaugeId.length > 0; theme: theme; onClicked: viewModel.requestManualReading() }
                }
                Row {
                    spacing: 8
                    SecondaryButton { text: "查看历史"; compact: true; enabled: viewModel.gaugeId.length > 0; theme: theme; onClicked: viewModel.requestShowHistory() }
                    GhostButton { text: "状态规则"; theme: theme; enabled: viewModel.gaugeId.length > 0; onClicked: viewModel.requestConfigureRule() }
                }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "实时监控"; detail: viewModel.realtimeStateText; theme: theme }
                PropertyRow { label: "来源"; value: viewModel.realtimeSourceText; theme: theme }
                PropertyRow { label: "连接"; value: viewModel.realtimeConnectionText; theme: theme }
                Row {
                    spacing: 8
                    AppButton { text: "启动模拟"; compact: true; enabled: viewModel.gaugeId.length > 0 && !viewModel.realtimeRunning; theme: theme; onClicked: viewModel.requestStartRealtime() }
                    SecondaryButton { text: "停止"; compact: true; enabled: viewModel.realtimeRunning; theme: theme; onClicked: viewModel.requestStopRealtime() }
                    GhostButton { text: "记录"; theme: theme; enabled: viewModel.realtimeRunning; onClicked: viewModel.requestRecordRealtime() }
                }
            }
        }
    }
}
