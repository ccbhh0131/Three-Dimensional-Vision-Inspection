import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
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
        contentHeight: contentColumn.height + 32
        clip: true

        Column {
            id: contentColumn
            x: 16
            y: 16
            width: root.width - 32
            spacing: 16

            Column {
                width: parent.width
                spacing: 4
                Text {
                    text: "设置"
                    color: theme.textPrimary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "项目、三维显示与应用偏好"
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "项目与文件"; detail: "项目管理"; theme: theme }

                Text {
                    width: parent.width
                    text: "默认项目目录"
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                }
                Row {
                    width: parent.width
                    spacing: 10
                    Text {
                        width: parent.width - 118
                        text: root.viewModel.defaultProjectDirectory.length > 0
                            ? root.viewModel.defaultProjectDirectory
                            : "未设置（使用上次打开目录）"
                        color: theme.textPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                        wrapMode: Text.WrapAnywhere
                        elide: Text.ElideMiddle
                        maximumLineCount: 2
                    }
                    SecondaryButton {
                        text: "选择目录"
                        compact: true
                        theme: theme
                        onClicked: root.viewModel.requestSelectDefaultProjectDirectory()
                    }
                }

                Row {
                    width: parent.width
                    spacing: 12
                    Column {
                        width: parent.width - 58
                        spacing: 2
                        Text {
                            text: "启动时恢复上次项目"
                            color: theme.textPrimary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 13
                        }
                        Text {
                            width: parent.width
                            text: "启动时尝试打开上次成功打开的 project.json；文件不存在时跳过。"
                            color: theme.textMuted
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    Switch {
                        checked: root.viewModel.restoreLastProject
                        onToggled: root.viewModel.setRestoreLastProject(checked)
                    }
                }

                Row {
                    width: parent.width
                    spacing: 12
                    Column {
                        width: parent.width - 58
                        spacing: 2
                        Text {
                            text: "记住上次打开目录"
                            color: theme.textPrimary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 13
                        }
                        Text {
                            width: parent.width
                            text: "打开或新建项目成功后更新文件对话框的目录。"
                            color: theme.textMuted
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    Switch {
                        checked: root.viewModel.rememberLastDirectory
                        onToggled: root.viewModel.setRememberLastDirectory(checked)
                    }
                }

                PropertyRow {
                    label: "当前记住目录"
                    value: root.viewModel.lastProjectDirectory.length > 0
                        ? root.viewModel.lastProjectDirectory : "未记录"
                    theme: theme
                }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "三维显示"; detail: "Viewer"; theme: theme }

                Row {
                    width: parent.width
                    spacing: 12
                    Column {
                        width: parent.width - 58
                        spacing: 2
                        Text {
                            text: "打开模型后自动适配视图"
                            color: theme.textPrimary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 13
                        }
                        Text {
                            width: parent.width
                            text: "关闭后仍保持有效相机，仅不再执行额外的自动 Fit。"
                            color: theme.textMuted
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    Switch {
                        checked: root.viewModel.autoFit
                        onToggled: root.viewModel.setAutoFit(checked)
                    }
                }

                Row {
                    width: parent.width
                    spacing: 12
                    Text {
                        width: 142
                        text: "Orbit 灵敏度"
                        color: theme.textPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    Slider {
                        id: orbitSlider
                        width: Math.max(150, parent.width - 210)
                        from: 0.5
                        to: 2.0
                        stepSize: 0.1
                        value: root.viewModel.orbitSensitivity
                        onMoved: root.viewModel.setOrbitSensitivity(value)
                    }
                    Text {
                        width: 48
                        text: Number(orbitSlider.value).toFixed(1) + "x"
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Row {
                    width: parent.width
                    spacing: 12
                    Column {
                        width: parent.width - 58
                        spacing: 2
                        Text {
                            text: "显示设备标记"
                            color: theme.textPrimary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 13
                        }
                        Text {
                            width: parent.width
                            text: "关闭后只隐藏点位，不删除标记或改变其空间位置。"
                            color: theme.textMuted
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    Switch {
                        checked: root.viewModel.showMarkers
                        onToggled: root.viewModel.setShowMarkers(checked)
                    }
                }

                Row {
                    width: parent.width
                    spacing: 12
                    Text {
                        width: 142
                        text: "设备标记大小"
                        color: theme.textPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 13
                        verticalAlignment: Text.AlignVCenter
                    }
                    Slider {
                        id: markerSizeSlider
                        width: Math.max(150, parent.width - 210)
                        from: 0.75
                        to: 1.5
                        stepSize: 0.05
                        value: root.viewModel.markerSize
                        onMoved: root.viewModel.setMarkerSize(value)
                    }
                    Text {
                        width: 48
                        text: Number(markerSizeSlider.value).toFixed(2) + "x"
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text {
                    width: parent.width
                    text: "前 / 后 / 左 / 右 / 上 / 下 / 等轴、方向校正与手动适配仍在三维场景工具栏中使用。"
                    color: theme.textMuted
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "巡检与数据"; detail: "Inspection"; theme: theme }
                Row {
                    width: parent.width
                    spacing: 12
                    Column {
                        width: parent.width - 170
                        spacing: 2
                        Text {
                            text: "默认实时刷新周期"
                            color: theme.textPrimary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 13
                        }
                        Text {
                            width: parent.width
                            text: "用于没有显式设备轮询配置的运行时监控。"
                            color: theme.textMuted
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    ComboBox {
                        id: pollCombo
                        width: 138
                        model: ["250 ms", "500 ms", "1000 ms", "2000 ms"]
                        currentIndex: [250, 500, 1000, 2000].indexOf(root.viewModel.realtimePollIntervalMs)
                        onActivated: root.viewModel.setRealtimePollIntervalMs([250, 500, 1000, 2000][index])
                    }
                }
                PropertyRow {
                    label: "当前默认值"
                    value: root.viewModel.realtimePollIntervalMs + " ms"
                    theme: theme
                }
            }

            SectionCard {
                width: parent.width
                theme: theme
                PanelTitle { title: "关于"; detail: "Vision3D Inspector"; theme: theme }
                Row {
                    width: parent.width
                    spacing: 12
                    Image {
                        width: 64
                        height: 64
                        source: "qrc:/branding/icons/Vision3DInspector_Logo_Transparent.png"
                        sourceSize.width: 128
                        sourceSize.height: 128
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                    Column {
                        width: parent.width - 76
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4
                        Text {
                            text: "Vision3D Inspector"
                            color: theme.textPrimary
                            font.family: theme.fontFamily
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width
                            text: "工业设备视觉巡检与三维状态管理系统"
                            color: theme.textSecondary
                            font.family: theme.cjkFontFamily
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                PropertyRow { label: "产品版本"; value: root.viewModel.productVersion; theme: theme }
                PropertyRow { label: "构建类型"; value: root.viewModel.buildType; theme: theme }
                PropertyRow { label: "配置目录"; value: root.viewModel.configDirectory; theme: theme }
                Row {
                    width: parent.width
                    spacing: 10
                    Item { width: parent.width - 150; height: 1 }
                    SecondaryButton {
                        text: "第三方许可"
                        compact: true
                        theme: theme
                        onClicked: root.viewModel.requestOpenThirdPartyLicenses()
                    }
                }
            }

            SecondaryButton {
                text: "恢复默认设置"
                compact: true
                theme: theme
                onClicked: resetDialog.open()
            }
        }
    }

    Dialog {
        id: resetDialog
        modal: true
        title: "恢复默认设置"
        width: Math.min(380, root.width - 24)
        height: 218
        padding: 0
        background: Rectangle {
            color: theme.bg
            border.color: theme.border
            border.width: 1
            radius: theme.radiusCard
        }
        contentItem: Item {
            anchors.fill: parent
            Column {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 6
                Text {
                    text: "恢复默认设置"
                    color: theme.textPrimary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "仅恢复应用偏好，不影响项目内容。"
                    color: theme.textSecondary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                Item { width: 1; height: 8 }
                Text {
                    text: "设备标记、历史记录和方向校正都会保留。"
                    color: theme.textPrimary
                    font.family: theme.cjkFontFamily
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                Item { width: 1; height: 10 }
                Row {
                    width: parent.width
                    spacing: 8
                    layoutDirection: Qt.RightToLeft
                    AppButton {
                        text: "恢复默认"
                        compact: true
                        theme: theme
                        onClicked: resetDialog.accept()
                    }
                    SecondaryButton {
                        text: "取消"
                        compact: true
                        theme: theme
                        onClicked: resetDialog.reject()
                    }
                }
            }
        }
        onAccepted: root.viewModel.requestResetPreferences()
    }
}
