import QtQuick 2.15
import QtQuick.Controls 2.15
import "components"

Item {
    id: root
    property bool alignmentPanelVisible: false
    signal alignmentPanelVisibilityChanged(bool visible)
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    property var presetLabels: ["前", "后", "左", "右", "上", "下", "等轴"]
    property var presetKeys: ["front", "back", "left", "right", "top", "bottom", "isometric"]
    property var alignmentAxes: ["X", "Y", "Z"]
    property var alignmentAngles: [-90, -5, 5, 90]
    Theme { id: theme }

    onAlignmentPanelVisibleChanged: alignmentPanelVisibilityChanged(alignmentPanelVisible)

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 44
        radius: theme.radiusCard
        color: theme.toolbarBackground
        border.color: theme.toolbarBorder
    }
    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 44
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "三维场景"
            color: theme.toolbarTextPrimary
            font.family: theme.cjkFontFamily
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: viewModel.viewerText
            color: theme.toolbarTextSecondary
            font.family: theme.cjkFontFamily
            font.pixelSize: 10
        }
        Item { width: 1; height: 1 }
        Rectangle {
            width: 1
            height: 22
            anchors.verticalCenter: parent.verticalCenter
            color: theme.toolbarDivider
        }
        ToolButton {
            id: fitButton
            width: 58
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            text: "适配视图"
            onClicked: viewModel.requestFitViewer()
            contentItem: Text {
                text: fitButton.text
                color: theme.toolbarTextPrimary
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: fitButton.pressed ? theme.toolbarPressed : (fitButton.hovered ? theme.toolbarHover : "transparent")
                border.color: fitButton.activeFocus ? theme.toolbarFocus : theme.toolbarBorder
                border.width: fitButton.activeFocus ? 2 : 1
            }
        }
        ToolButton {
            id: resetButton
            width: 58
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            text: "重置视图"
            onClicked: viewModel.requestResetViewer()
            contentItem: Text {
                text: resetButton.text
                color: theme.toolbarTextPrimary
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: resetButton.pressed ? theme.toolbarPressed : (resetButton.hovered ? theme.toolbarHover : "transparent")
                border.color: resetButton.activeFocus ? theme.toolbarFocus : theme.toolbarBorder
                border.width: resetButton.activeFocus ? 2 : 1
            }
        }
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            Repeater {
                model: root.presetLabels.length
                delegate: ToolButton {
                    id: presetButton
                    width: index === 6 ? 25 : 18
                    height: 30
                    hoverEnabled: true
                    text: root.presetLabels[index]
                    onClicked: root.viewModel.requestCameraView(root.presetKeys[index])
                    contentItem: Text {
                        text: presetButton.text
                        color: theme.toolbarTextPrimary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 7
                        color: presetButton.pressed
                            ? theme.toolbarPressed
                            : (presetButton.hovered ? theme.toolbarHover : "transparent")
                        border.color: presetButton.activeFocus ? theme.toolbarFocus : theme.toolbarBorder
                        border.width: presetButton.activeFocus ? 2 : 1
                    }
                }
            }
        }
        ToolButton {
            id: alignmentButton
            width: 60
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            text: "方向校正"
            onClicked: {
                root.alignmentPanelVisible = true
                root.viewModel.requestBeginSceneAlignment()
            }
            contentItem: Text {
                text: alignmentButton.text
                color: theme.toolbarTextPrimary
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: alignmentButton.pressed
                    ? theme.toolbarPressed : (alignmentButton.hovered ? theme.toolbarHover : "transparent")
                border.color: alignmentButton.activeFocus ? theme.toolbarFocus : theme.toolbarBorder
                border.width: alignmentButton.activeFocus ? 2 : 1
            }
        }
        ToolButton {
            id: addMarkerButton
            width: 60
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            enabled: root.viewModel.canAddMarker
            text: "添加设备"
            onClicked: root.viewModel.requestAddMarker()
            contentItem: Text {
                text: addMarkerButton.text
                color: addMarkerButton.enabled ? theme.toolbarTextPrimary : theme.toolbarDisabledText
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: !addMarkerButton.enabled
                    ? theme.toolbarDisabledBackground
                    : (addMarkerButton.pressed
                        ? theme.toolbarPressed
                        : (addMarkerButton.hovered ? theme.toolbarHover : "transparent"))
                border.color: addMarkerButton.enabled ? theme.toolbarBorder : theme.toolbarDisabledBorder
                border.width: addMarkerButton.activeFocus ? 2 : 1
            }
        }
        ToolButton {
            id: deleteMarkerButton
            width: 60
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            enabled: root.viewModel.canDeleteMarker
            text: "删除设备"
            onClicked: root.viewModel.requestDeleteMarker()
            contentItem: Text {
                text: deleteMarkerButton.text
                color: deleteMarkerButton.enabled ? theme.toolbarTextPrimary : theme.toolbarDisabledText
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: !deleteMarkerButton.enabled
                    ? theme.toolbarDisabledBackground
                    : (deleteMarkerButton.pressed
                        ? theme.toolbarPressed
                        : (deleteMarkerButton.hovered ? theme.toolbarHover : "transparent"))
                border.color: deleteMarkerButton.enabled
                    ? theme.toolbarBorder
                    : theme.toolbarDisabledBorder
                border.width: deleteMarkerButton.activeFocus ? 2 : 1
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "拖拽旋转 · Shift 平移 · 滚轮缩放 · 点击表面拾取"
            color: theme.toolbarTextMuted
            font.family: theme.cjkFontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
            width: Math.max(0, root.width - x - 4)
        }
    }

    Rectangle {
        id: alignmentPanel
        visible: root.alignmentPanelVisible && root.viewModel.alignmentEditing
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: 50
        width: Math.min(Math.max(320, root.width - 28), 390)
        height: 154
        radius: 9
        color: theme.surface
        border.color: theme.borderStrong
        z: 2

        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            Text {
                text: "方向校正（临时预览）"
                color: theme.textPrimary
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Repeater {
                model: root.alignmentAxes.length
                delegate: Row {
                    id: axisRow
                    property string axisKey: root.alignmentAxes[index]
                    width: alignmentPanel.width - 20
                    height: 25
                    spacing: 5

                    Text {
                        width: 24
                        height: 25
                        text: root.alignmentAxes[index] + "："
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 11
                        verticalAlignment: Text.AlignVCenter
                    }

                    Repeater {
                        model: root.alignmentAngles.length
                        delegate: ToolButton {
                            id: angleButton
                            width: 62
                            height: 25
                            hoverEnabled: true
                            text: (root.alignmentAngles[index] > 0 ? "+" : "")
                                  + root.alignmentAngles[index] + "°"
                            onClicked: root.viewModel.requestSceneAlignmentRotation(
                                           axisRow.axisKey,
                                           root.alignmentAngles[index])
                            contentItem: Text {
                                text: angleButton.text
                                color: theme.textPrimary
                                font.family: theme.cjkFontFamily
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 5
                                color: angleButton.pressed
                                    ? theme.primarySoft : (angleButton.hovered ? theme.secondaryButtonHover : theme.surface)
                                border.color: theme.secondaryButtonBorder
                            }
                        }
                    }
                }
            }

            Row {
                spacing: 6
                anchors.right: parent.right

                ToolButton {
                    width: 58
                    height: 26
                    text: "重置"
                    onClicked: root.viewModel.requestResetSceneAlignment()
                    contentItem: Text {
                        text: parent.text
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 5
                        color: parent.pressed ? theme.surfaceHover : theme.surface
                        border.color: theme.border
                        border.width: 1
                    }
                }
                ToolButton {
                    width: 58
                    height: 26
                    text: "取消"
                    onClicked: root.viewModel.requestCancelSceneAlignment()
                    contentItem: Text {
                        text: parent.text
                        color: theme.textSecondary
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 5
                        color: parent.pressed ? theme.surfaceHover : theme.surface
                        border.color: theme.border
                        border.width: 1
                    }
                }
                ToolButton {
                    width: 72
                    height: 26
                    text: "保存方向"
                    onClicked: root.viewModel.requestSaveSceneAlignment()
                    contentItem: Text {
                        text: parent.text
                        color: theme.primaryForeground
                        font.family: theme.cjkFontFamily
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 5
                        color: theme.primary
                    }
                }
            }
        }
    }

    Connections {
        target: root.viewModel
        function onDataChanged() {
            if (!root.viewModel.alignmentEditing) {
                root.alignmentPanelVisible = false
            }
        }
    }
}
