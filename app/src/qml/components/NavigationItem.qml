import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

AbstractButton {
    id: root
    property string label: "导航"
    property string iconSource: ""
    property bool selected: false
    property Theme theme: Theme {}

    width: parent ? parent.width : 176
    height: 42
    hoverEnabled: true

    background: Rectangle {
        radius: theme.radiusInput
        color: root.selected ? theme.primarySelected : (root.hovered ? theme.surfaceMuted : "transparent")
        border.color: root.activeFocus ? theme.border : "transparent"
        border.width: root.activeFocus ? 1 : 0
    }

    contentItem: Item {
        Image {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            source: root.iconSource
            fillMode: Image.PreserveAspectFit
            opacity: root.selected ? 1.0 : 0.82
        }
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 44
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            color: root.selected ? theme.primary : theme.textSecondary
            font.family: theme.cjkFontFamily
            font.pixelSize: 13
            font.weight: root.selected ? Font.DemiBold : Font.Normal
        }
        Rectangle {
            visible: root.selected
            width: 3
            height: 20
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            radius: 1.5
            color: theme.primary
        }
    }
}
