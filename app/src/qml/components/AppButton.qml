import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property string text: "操作"
    property color accent: theme.primary
    property color accentHover: theme.primaryHover
    property color accentPressed: theme.primaryPressed
    property bool busy: false
    property bool compact: false
    property alias fontSize: label.font.pixelSize
    property Theme theme: Theme {}
    signal clicked()

    implicitWidth: Math.max(92, label.implicitWidth + 32)
    implicitHeight: compact ? 32 : 36
    radius: theme.radiusInput
    color: !enabled ? theme.surfaceMuted : (mouse.containsMouse ? accentHover : accent)
    border.color: color
    opacity: enabled ? 1.0 : 0.55

    Text {
        id: label
        anchors.centerIn: parent
        text: root.busy ? "处理中…" : root.text
        color: theme.surface
        font.family: theme.cjkFontFamily
        font.pixelSize: root.compact ? 12 : 13
        font.weight: Font.DemiBold
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled && !root.busy
        onClicked: root.clicked()
    }
    Behavior on color { ColorAnimation { duration: 140 } }
}
