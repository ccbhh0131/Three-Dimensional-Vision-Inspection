import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

ToolButton {
    id: root
    property string iconSource: ""
    property string tooltip: ""
    property int iconSize: 18
    property Theme theme: Theme {}

    width: 34
    height: 34
    hoverEnabled: true
    display: AbstractButton.IconOnly

    contentItem: Image {
        width: root.iconSize
        height: root.iconSize
        source: root.iconSource
        fillMode: Image.PreserveAspectFit
        opacity: root.enabled ? 1.0 : 0.45
    }
    background: Rectangle {
        radius: theme.radiusInput
        color: root.pressed ? theme.border : (root.hovered ? theme.surfaceHover : "transparent")
        border.color: root.activeFocus ? theme.borderStrong : "transparent"
        border.width: root.activeFocus ? 1 : 0
    }
    ToolTip.visible: root.hovered && root.tooltip.length > 0
    ToolTip.text: root.tooltip
    ToolTip.delay: 500
}
