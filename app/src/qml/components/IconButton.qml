import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property string iconSource: ""
    property string tooltip: ""
    property int iconSize: 18
    property Theme theme: Theme {}
    signal clicked()

    width: 34
    height: 34
    radius: theme.radiusInput
    color: mouse.containsMouse ? theme.surfaceMuted : "transparent"
    border.color: "transparent"

    Image {
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        source: root.iconSource
        fillMode: Image.PreserveAspectFit
        opacity: root.enabled ? 1.0 : 0.45
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
