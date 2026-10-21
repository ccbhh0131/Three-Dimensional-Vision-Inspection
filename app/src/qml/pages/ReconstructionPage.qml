import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."
import "../components"

Item {
    id: root
    property var pipelineStages: ["图像资产", "特征与匹配", "稀疏重建", "稠密与融合", "网格产物", "模型优化"]
    property bool showVideoPreview: false
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    property var videoIngest: viewModel.videoIngest || ({})
    property bool reconstructionBusy: viewModel.reconstructionText === "进行中" || viewModel.reconstructionText === "准备中"
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
                        text: "设置"
                        compact: true
                        theme: theme
                        onClicked: viewModel.requestSettings()
                    }
                    AppButton {
                        text: "取消重建"
                        compact: true
                        enabled: viewModel.reconstructionText === "进行中" || viewModel.reconstructionText === "准备中"
                        theme: theme
                        onClicked: viewModel.requestCancelReconstruction()
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
                        model: root.pipelineStages.length
                        delegate: Row {
                            width: parent.width
                            height: 30
                            spacing: 10
                            Rectangle { width: 18; height: 18; radius: 9; anchors.verticalCenter: parent.verticalCenter; color: viewModel.reconstructionProgress >= (index + 1) * 100 / root.pipelineStages.length ? theme.primary : theme.surfaceMuted; border.color: theme.border; Text { anchors.centerIn: parent; text: viewModel.reconstructionProgress >= (index + 1) * 100 / root.pipelineStages.length ? "✓" : (index + 1); color: viewModel.reconstructionProgress >= (index + 1) * 100 / root.pipelineStages.length ? theme.surface : theme.textMuted; font.pixelSize: 10 } }
                            Text { text: root.pipelineStages[index]; color: index === root.pipelineStages.length - 1 && viewModel.reconstructionProgress === 100 ? theme.primary : theme.textSecondary; font.family: theme.cjkFontFamily; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
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
                    PropertyRow { label: "相机位姿"; value: viewModel.reconstructionRegistrationText; theme: theme }
                    Rectangle { width: parent.width; height: 9; radius: 4; color: theme.surfaceMuted; Rectangle { width: parent.width * viewModel.reconstructionProgress / 100; height: parent.height; radius: 4; color: theme.primary } }
                    PropertyRow { label: "产物"; value: viewModel.reconstructionArtifactText; theme: theme }
                }
            }
            Text {
                width: parent.width
                visible: !!viewModel.reconstructionErrorText
                text: viewModel.reconstructionErrorText || ""
                wrapMode: Text.Wrap
                color: "#c64242"
                font.family: theme.cjkFontFamily
                font.pixelSize: 14
            }
            Text {
                width: parent.width
                visible: viewModel.reconstructionText === "失败"
                text: "若有效帧未能连成同一个模型，请让物体保持静止，手机缓慢绕拍；保持距离和焦距稳定，让前后画面充分重叠。"
                wrapMode: Text.Wrap
                color: theme.textSecondary
                font.family: theme.cjkFontFamily
                font.pixelSize: 13
            }
            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "新建重建任务"; detail: "照片和视频共用同一套 COLMAP 与模型后处理流程。"; theme: theme }
                Text {
                    visible: !viewModel.hasProject
                    text: "点击导入照片或视频后，先创建项目，再选择素材。也可以从顶部打开已有项目。"
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
                Row {
                    spacing: 8
                    AppButton {
                        text: "导入照片"
                        enabled: !root.reconstructionBusy && !videoIngest.busy
                        theme: theme
                        onClicked: viewModel.requestImportImages()
                    }
                    AppButton {
                        text: "导入视频"
                        enabled: !root.reconstructionBusy && !videoIngest.busy
                        theme: theme
                        onClicked: viewModel.requestImportVideo()
                    }
                    AppButton {
                        text: "开始照片重建"
                        enabled: viewModel.hasProject && viewModel.imageCount >= 2 && viewModel.reconstructionText !== "进行中" && viewModel.reconstructionText !== "准备中"
                        theme: theme
                        onClicked: viewModel.requestStartPhotoReconstruction()
                    }
                }
                Text {
                    text: viewModel.imageCount + " 张照片已导入项目。视频输入：" + (videoIngest.videoFileName || "尚未选择")
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
                Text {
                    visible: !!videoIngest.hasInfo || !!videoIngest.errorText
                    text: videoIngest.infoText || videoIngest.errorText || ""
                    color: videoIngest.errorText ? "#c64242" : theme.textPrimary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
                Row {
                    spacing: 8
                    CheckBox { id: automaticSampling; text: "自动抽帧"; checked: true }
                    CheckBox { id: autoContinue; text: "抽帧完成后自动开始重建"; checked: false }
                    SpinBox {
                        id: manualTarget
                        visible: !automaticSampling.checked
                        from: 100
                        to: 500
                        value: videoIngest.recommendedFrames || 250
                        editable: true
                        Accessible.name: "目标帧数"
                    }
                    Text {
                        visible: automaticSampling.checked && videoIngest.hasInfo
                        text: "预计提取：" + videoIngest.recommendedFrames + " 张"
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Row {
                    spacing: 8
                    AppButton {
                        text: "抽帧并生成预览"
                        enabled: !!videoIngest.canPreprocess && !videoIngest.busy
                        busy: videoIngest.state === "extracting" || videoIngest.state === "analyzing"
                        theme: theme
                        onClicked: viewModel.requestPreprocessVideo(automaticSampling.checked ? 0 : manualTarget.value, autoContinue.checked)
                    }
                    AppButton {
                        text: "取消视频处理"
                        enabled: !!videoIngest.busy
                        theme: theme
                        onClicked: viewModel.requestCancelVideoProcessing()
                    }
                    AppButton {
                        text: "查看抽帧结果"
                        enabled: !!videoIngest.previewReady
                        theme: theme
                        onClicked: {
                            root.showVideoPreview = !root.showVideoPreview
                            viewModel.requestViewVideoPreview()
                        }
                    }
                    AppButton {
                        text: viewModel.canResumeVideoDense ? "继续三维重建" : "开始三维重建"
                        enabled: (!!videoIngest.canStartReconstruction || !!viewModel.canResumeVideoDense) && !root.reconstructionBusy
                        theme: theme
                        onClicked: viewModel.requestStartVideoReconstruction()
                    }
                }
                Text {
                    visible: !!videoIngest.statusText
                    text: videoIngest.statusText || ""
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
                ProgressBar {
                    visible: !!videoIngest.busy
                    from: 0
                    to: 100
                    value: videoIngest.progress || 0
                    indeterminate: videoIngest.state === "probing" || videoIngest.state === "analyzing"
                    width: parent.width
                }
                Text {
                    visible: !!videoIngest.previewReady
                    text: "原始候选：" + videoIngest.rawFrameCount + "　严重模糊：" + videoIngest.blurRejectedCount + "　重复帧：" + videoIngest.duplicateRejectedCount + "　覆盖恢复：" + videoIngest.coverageRecoveredCount + "　最终有效：" + videoIngest.acceptedFrameCount
                    color: theme.textPrimary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
                Image {
                    visible: root.showVideoPreview && videoIngest.previewReady
                    source: videoIngest.contactSheetSource || ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    width: parent.width
                    height: Math.min(420, parent.width * 0.52)
                }
                Text {
                    text: "拍摄建议：缓慢绕物体移动，保持主体在画面内，避免快速晃动和数字变焦；补拍高低角度并保持曝光稳定。"
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    width: parent.width
                }
            }
        }
    }
}
