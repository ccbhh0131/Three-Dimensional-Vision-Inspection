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
        anchors.margins: 2
        contentWidth: width
        contentHeight: contentColumn.height + 28
        clip: true

        Column {
            id: contentColumn
            x: 16
            y: 16
            width: root.width - 32
            spacing: 16

            Row {
                width: parent.width
                spacing: 12
                Column {
                    width: parent.width - quickActions.width - 12
                    spacing: 4
                    Text { text: "项目总览"; color: theme.textPrimary; font.family: theme.cjkFontFamily; font.pixelSize: 24; font.weight: Font.DemiBold }
                    Text { text: viewModel.projectSummary; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; elide: Text.ElideRight; width: parent.width }
                }
                Row {
                    id: quickActions
                    spacing: 8
                    SecondaryButton { text: "导入图像"; compact: true; enabled: viewModel.hasProject; theme: theme; onClicked: viewModel.requestImportImages() }
                    AppButton { text: "查看三维"; compact: true; enabled: viewModel.hasProject; theme: theme; onClicked: { viewModel.selectPage("scene"); viewModel.requestOpenViewer() } }
                }
            }

            Flow {
                width: parent.width
                spacing: 12
                MetricCard { width: Math.max(150, (contentColumn.width - 36) / 4); label: "图像资产"; value: viewModel.imageCount; detail: "已纳入项目"; theme: theme; accent: theme.primary }
                MetricCard { width: Math.max(150, (contentColumn.width - 36) / 4); label: "设备标记"; value: viewModel.markerCount; detail: "三维场景绑定"; theme: theme; accent: theme.info }
                MetricCard { width: Math.max(150, (contentColumn.width - 36) / 4); label: "仪表资产"; value: viewModel.gaugeCount; detail: "已配置量程"; theme: theme; accent: theme.normal }
                MetricCard { width: Math.max(150, (contentColumn.width - 36) / 4); label: "巡检记录"; value: viewModel.inspectionCount; detail: "历史数据"; theme: theme; accent: theme.warning }
            }

            Row {
                width: parent.width
                spacing: 16
                SectionCard {
                    width: (parent.width - 16) * 0.58
                    height: 208
                    theme: theme
                    PanelTitle { title: "项目状态"; detail: viewModel.projectName; theme: theme }
                    PropertyRow { label: "重建状态"; value: viewModel.reconstructionText; theme: theme }
                    PropertyRow { label: "当前阶段"; value: viewModel.reconstructionStageText; theme: theme }
                    PropertyRow { label: "引擎"; value: viewModel.engineText; theme: theme }
                    PropertyRow { label: "三维查看器"; value: viewModel.viewerText; theme: theme }
                    Row {
                        width: parent.width
                        spacing: 10
                        Rectangle { width: parent.width - 52; height: 8; radius: 4; color: theme.surfaceMuted; anchors.verticalCenter: parent.verticalCenter; Rectangle { width: parent.width * viewModel.reconstructionProgress / 100; height: parent.height; radius: 4; color: theme.primary } }
                        Text { text: viewModel.reconstructionProgress + "%"; color: theme.textSecondary; font.family: theme.fontFamily; font.pixelSize: 11 }
                    }
                }
                SectionCard {
                    width: (parent.width - 16) * 0.42
                    height: 208
                    theme: theme
                    PanelTitle { title: "当前关注"; detail: "Inspector"; theme: theme }
                    Text { text: viewModel.deviceName; color: theme.textPrimary; font.family: theme.cjkFontFamily; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                    Row {
                        spacing: 7
                        Text { text: viewModel.currentReadingText; color: theme.textPrimary; font.family: theme.fontFamily; font.pixelSize: 32; font.weight: Font.DemiBold }
                        Text { text: viewModel.readingUnit; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 13; anchors.baseline: parent.children[0].baseline }
                        StatusChip { text: viewModel.gaugeStatusText; statusCode: viewModel.gaugeStatusCode; theme: theme; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Text { text: viewModel.gaugeName + " · " + viewModel.readingSourceText; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; elide: Text.ElideRight; width: parent.width }
                    AppButton { text: "打开仪表巡检"; compact: true; enabled: viewModel.gaugeId.length > 0; theme: theme; onClicked: viewModel.selectPage("visual") }
                }
            }

            Row {
                width: parent.width
                spacing: 16
                SectionCard {
                    width: (parent.width - 16) * 0.58
                    height: 150
                    theme: theme
                    PanelTitle { title: "最近巡检"; detail: viewModel.historyCountText; theme: theme }
                    Text { text: viewModel.updatedText === "—" ? "暂无最近读数" : "最近更新时间 · " + viewModel.updatedText; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12 }
                    Text { text: viewModel.readingSourceText + " · " + viewModel.currentReadingText + " " + viewModel.readingUnit; color: theme.textPrimary; font.family: theme.cjkFontFamily; font.pixelSize: 16; font.weight: Font.DemiBold }
                    SecondaryButton { text: "查看历史记录"; compact: true; enabled: viewModel.gaugeId.length > 0; theme: theme; onClicked: viewModel.selectPage("history") }
                }
                SectionCard {
                    width: (parent.width - 16) * 0.42
                    height: 150
                    theme: theme
                    PanelTitle { title: "需要关注"; theme: theme }
                    Text { text: viewModel.alarmCount > 0 ? (viewModel.alarmCount + " 个仪表处于报警") : "当前没有报警仪表"; color: viewModel.alarmCount > 0 ? theme.alarm : theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text { text: "状态规则与颜色在三维场景和检查器中保持一致。"; color: theme.textMuted; font.family: theme.cjkFontFamily; font.pixelSize: 11; wrapMode: Text.WordWrap; width: parent.width }
                }
            }
        }
    }
}
