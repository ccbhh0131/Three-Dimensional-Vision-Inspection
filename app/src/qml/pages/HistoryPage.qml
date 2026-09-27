import QtQuick 2.15
import ".."
import "../components"

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }
    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16
        Row {
            width: parent.width
            Column {
                width: parent.width - action.width - 12
                spacing: 4
                Text {
                    text: "历史记录"
                    color: theme.textPrimary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "按时间查看仪表读数、来源和状态。记录由现有 InspectionRecord 模型提供。"
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                }
            }
            SecondaryButton { id: action; text: "打开旧版详情"; compact: true; enabled: viewModel.gaugeId.length > 0; theme: theme; onClicked: viewModel.requestShowHistory() }
        }
        SectionCard {
            width: parent.width
            height: parent.height - 82
            theme: theme
            Row {
                width: parent.width
                height: 28
                Text { width: parent.width * 0.26; text: "时间"; color: theme.textMuted; font.family: theme.cjkFontFamily; font.pixelSize: 11 }
                Text { width: parent.width * 0.18; text: "读数"; color: theme.textMuted; font.family: theme.cjkFontFamily; font.pixelSize: 11 }
                Text { width: parent.width * 0.18; text: "来源"; color: theme.textMuted; font.family: theme.cjkFontFamily; font.pixelSize: 11 }
                Text { width: parent.width * 0.18; text: "状态"; color: theme.textMuted; font.family: theme.cjkFontFamily; font.pixelSize: 11 }
                Text { width: parent.width * 0.20; text: "图片"; color: theme.textMuted; font.family: theme.cjkFontFamily; font.pixelSize: 11 }
            }
            ListView {
                id: historyList
                width: parent.width
                height: parent.height - 44
                clip: true
                model: viewModel.historyRows
                delegate: Rectangle {
                    width: historyList.width
                    height: 44
                    color: index % 2 === 0 ? theme.surface : theme.surfaceMuted
                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        Text { width: parent.width * 0.25; anchors.verticalCenter: parent.verticalCenter; text: modelData.timestamp; color: theme.textPrimary; font.family: theme.fontFamily; font.pixelSize: 11; elide: Text.ElideRight }
                        Text { width: parent.width * 0.18; anchors.verticalCenter: parent.verticalCenter; text: modelData.value + " " + modelData.unit; color: theme.textPrimary; font.family: theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
                        Text { width: parent.width * 0.18; anchors.verticalCenter: parent.verticalCenter; text: modelData.source; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 11 }
                        Item { width: parent.width * 0.18; height: parent.height; StatusChip { anchors.verticalCenter: parent.verticalCenter; text: modelData.status; statusCode: modelData.statusCode; theme: theme } }
                        Text { width: parent.width * 0.20; anchors.verticalCenter: parent.verticalCenter; text: modelData.image; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 11; elide: Text.ElideRight }
                    }
                }
                EmptyState { anchors.fill: parent; visible: viewModel.historyRows.length === 0; title: "暂无历史记录"; detail: "完成一次手动、视觉或实时采样后，记录会出现在这里。"; theme: theme }
            }
        }
    }
}
