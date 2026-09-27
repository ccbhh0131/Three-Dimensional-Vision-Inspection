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
                    width: parent.width - actions.width - 12
                    spacing: 4
                    Text {
                        text: "重建任务"
                        color: theme.textPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "查看数据集、阶段进度和三维产物。"
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                    }
                }
                Row {
                    id: actions
                    spacing: 8
                    SecondaryButton {
                        text: "打开三维场景"
                        compact: true
                        enabled: viewModel.hasProject
                        theme: theme
                        onClicked: {
                            viewModel.selectPage("scene")
                            viewModel.requestOpenViewer()
                        }
                    }
                    AppButton {
                        text: "技术设置"
                        compact: true
                        theme: theme
                        onClicked: viewModel.requestSettings()
                    }
                }
            }
            Row {
                width: parent.width
                spacing: 16
                SectionCard {
                    width: (parent.width - 16) * 0.58
                    height: 280
                    theme: theme
                    PanelTitle { title: "流水线"; detail: viewModel.reconstructionText; theme: theme }
                    Repeater {
                        model: ["图像资产", "特征与匹配", "稀疏重建", "稠密与融合", "网格产物"]
                        delegate: Row {
                            width: parent.width
                            height: 30
                            spacing: 10
                            Rectangle { width: 18; height: 18; radius: 9; anchors.verticalCenter: parent.verticalCenter; color: index < 4 && viewModel.reconstructionProgress >= (index + 1) * 20 ? theme.primary : theme.surfaceMuted; border.color: theme.border; Text { anchors.centerIn: parent; text: index < 4 && viewModel.reconstructionProgress >= (index + 1) * 20 ? "✓" : (index + 1); color: index < 4 && viewModel.reconstructionProgress >= (index + 1) * 20 ? theme.surface : theme.textMuted; font.pixelSize: 10 } }
                            Text { text: modelData; color: index === 4 && viewModel.reconstructionProgress === 100 ? theme.primary : theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
                SectionCard {
                    width: (parent.width - 16) * 0.42
                    height: 280
                    theme: theme
                    PanelTitle { title: "任务摘要"; theme: theme }
                    Text { text: viewModel.reconstructionProgress + "%"; color: theme.textPrimary; font.family: theme.fontFamily; font.pixelSize: 42; font.weight: Font.DemiBold }
                    Text { text: viewModel.reconstructionStageText; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 13 }
                    Rectangle { width: parent.width; height: 9; radius: 4; color: theme.surfaceMuted; Rectangle { width: parent.width * viewModel.reconstructionProgress / 100; height: parent.height; radius: 4; color: theme.primary } }
                    PropertyRow { label: "产物"; value: viewModel.reconstructionArtifactText; theme: theme }
                }
            }
            SectionCard {
                width: parent.width
                height: 126
                theme: theme
                PanelTitle { title: "操作提示"; theme: theme }
                Text { text: "重建控制、日志、COLMAP 配置和产物校验继续使用原有 QWidget / ReconstructionController。"; color: theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; wrapMode: Text.WordWrap; width: parent.width }
            }
        }
    }
}
