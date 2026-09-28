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
        radius: theme.radiusCard
        color: "#2F3733"
        border.color: "#424C47"
    }
    Row {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 10
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "三维场景"
            color: "#FFFFFF"
            font.family: theme.cjkFontFamily
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: viewModel.viewerText
            color: "#BBC5BF"
            font.family: theme.cjkFontFamily
            font.pixelSize: 11
        }
        Item { width: 1; height: 1 }
        Rectangle {
            width: 1
            height: 22
            anchors.verticalCenter: parent.verticalCenter
            color: "#58645D"
        }
        ToolButton {
            id: fitButton
            width: 90
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            text: "适配视图"
            onClicked: viewModel.requestOpenViewer()
            contentItem: Text {
                text: fitButton.text
                color: "#F1F4F1"
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: fitButton.pressed ? "#3F4B44" : (fitButton.hovered ? "#45534B" : "transparent")
                border.color: fitButton.activeFocus ? "#A7B5AC" : "#647269"
                border.width: fitButton.activeFocus ? 2 : 1
            }
        }
        ToolButton {
            id: resetButton
            width: 90
            height: 30
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            text: "重置视图"
            onClicked: viewModel.requestResetViewer()
            contentItem: Text {
                text: resetButton.text
                color: "#F1F4F1"
                font.family: theme.cjkFontFamily
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: 7
                color: resetButton.pressed ? "#3F4B44" : (resetButton.hovered ? "#45534B" : "transparent")
                border.color: resetButton.activeFocus ? "#A7B5AC" : "#647269"
                border.width: resetButton.activeFocus ? 2 : 1
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "拖拽旋转 · Shift 平移 · 滚轮缩放 · 点击表面拾取"
            color: "#AAB6AE"
            font.family: theme.cjkFontFamily
            font.pixelSize: 11
        }
    }
}
