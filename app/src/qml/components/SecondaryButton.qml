import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property string text: "操作"
    property bool compact: false
    property Theme theme: Theme {}
    signal clicked()

    implicitWidth: Math.max(92, label.implicitWidth + 30)
    implicitHeight: compact ? 32 : 36
    radius: theme.radiusInput
    color: mouse.containsMouse ? theme.primarySoft : theme.surface
    border.color: mouse.containsMouse ? theme.primary : theme.border
    opacity: enabled ? 1.0 : 0.55

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: theme.primary
        font.family: theme.cjkFontFamily
        font.pixelSize: root.compact ? 12 : 13
        font.weight: Font.DemiBold
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        onClicked: root.clicked()
    }
    Behavior on color { ColorAnimation { duration: 140 } }
}
