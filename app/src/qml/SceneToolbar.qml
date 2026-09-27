import QtQuick 2.15
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
        Rectangle {
            width: 90
            height: 30
            radius: 7
            anchors.verticalCenter: parent.verticalCenter
            color: toolMouse.containsMouse ? "#45534B" : "transparent"
            border.color: "#647269"
            Text { anchors.centerIn: parent; text: "适配视图"; color: "#F1F4F1"; font.family: theme.cjkFontFamily; font.pixelSize: 12 }
            MouseArea { id: toolMouse; anchors.fill: parent; hoverEnabled: true; onClicked: viewModel.requestOpenViewer() }
        }
        Rectangle {
            width: 90
            height: 30
            radius: 7
            anchors.verticalCenter: parent.verticalCenter
            color: resetMouse.containsMouse ? "#45534B" : "transparent"
            border.color: "#647269"
            Text { anchors.centerIn: parent; text: "重置视图"; color: "#F1F4F1"; font.family: theme.cjkFontFamily; font.pixelSize: 12 }
            MouseArea { id: resetMouse; anchors.fill: parent; hoverEnabled: true; onClicked: viewModel.requestResetViewer() }
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
