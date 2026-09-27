import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property string text: "操作"
    property Theme theme: Theme {}
    signal clicked()

    implicitWidth: Math.max(72, label.implicitWidth + 20)
    implicitHeight: 32
    radius: theme.radiusInput
    color: mouse.containsMouse ? theme.surfaceMuted : "transparent"
    border.color: "transparent"
    opacity: enabled ? 1.0 : 0.55

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: theme.textSecondary
        font.family: theme.cjkFontFamily
        font.pixelSize: 12
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
