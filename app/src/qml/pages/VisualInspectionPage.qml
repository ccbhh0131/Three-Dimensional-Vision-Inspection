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
                spacing: 12
                Column {
                    width: parent.width - actions.width - 12
                    spacing: 4
                    Text {
                        text: "视觉巡检"
                        color: theme.textPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "保留现有 VisualGaugeReader 算法，QML 仅提供结果呈现与动作入口。"
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }
                Row {
                    id: actions
                    spacing: 8
                    AppButton {
                        text: "开始视觉读数"
                        compact: true
                        enabled: viewModel.gaugeId.length > 0
                        theme: theme
                        onClicked: viewModel.requestVisualReading()
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 16
                SectionCard {
                    width: (parent.width - 16) * 0.58
                    height: 330
                    theme: theme
                    PanelTitle { title: "图像与 ROI"; detail: viewModel.visualImageSource.length > 0 ? "最近关联图片" : "等待图片"; theme: theme }
                    Rectangle {
                        width: parent.width
                        height: 250
                        radius: theme.radiusInput
                        color: theme.surfaceMuted
                        border.color: theme.border
                        Image { anchors.fill: parent; anchors.margins: 1; source: viewModel.visualImageSource; fillMode: Image.PreserveAspectFit; visible: viewModel.visualImageSource.length > 0 }
                        EmptyState { anchors.fill: parent; visible: viewModel.visualImageSource.length === 0; title: "暂无视觉巡检图片"; detail: "执行视觉读数后，关联图片会显示在这里。"; theme: theme }
                    }
                }
                SectionCard {
                    width: (parent.width - 16) * 0.42
                    height: 330
                    theme: theme
                    PanelTitle { title: "读数结果"; theme: theme }
                    Text { text: viewModel.gaugeName; color: theme.textPrimary; font.family: theme.cjkFontFamily; font.pixelSize: 15; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                    Row {
                        spacing: 8
                        Text {
                            text: viewModel.currentReadingText
                            color: theme.textPrimary
                            font.family: theme.fontFamily
                            font.pixelSize: 40
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
                    StatusChip { text: viewModel.gaugeStatusText; statusCode: viewModel.gaugeStatusCode; theme: theme }
                    PropertyRow { label: "来源"; value: viewModel.readingSourceText; theme: theme }
                    PropertyRow { label: "更新时间"; value: viewModel.updatedText; theme: theme }
                    Row {
                        spacing: 8
                        SecondaryButton {
                            text: "手动更新"
                            compact: true
                            enabled: viewModel.gaugeId.length > 0
                            theme: theme
                            onClicked: viewModel.requestManualReading()
                        }
                        GhostButton {
                            text: "查看历史"
                            theme: theme
                            enabled: viewModel.gaugeId.length > 0
                            onClicked: viewModel.requestShowHistory()
                        }
                    }
                }
            }
            SectionCard {
                width: parent.width
                height: 122
                theme: theme
                PanelTitle { title: "巡检说明"; theme: theme }
                Text { text: "视觉读数入口沿用现有 GaugeProfile、ROI 和 VisualGaugeReader；本页面不复制阈值、状态或图像算法。"; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
    }
}
