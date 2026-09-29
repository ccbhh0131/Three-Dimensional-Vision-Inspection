import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Button {
    id: root
    property Theme theme: Theme {}

    hoverEnabled: true
    implicitWidth: Math.max(72, label.implicitWidth + 20)
    implicitHeight: 32

    contentItem: Text {
        id: label
        text: root.text
        color: root.enabled ? theme.textSecondary : theme.disabledText
        font.family: theme.cjkFontFamily
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: theme.radiusInput
        color: !root.enabled ? theme.disabledBackground : (root.pressed ? theme.surfaceHover : (root.hovered ? theme.surfaceHover : "transparent"))
        border.color: !root.enabled ? theme.disabledBorder : (root.activeFocus ? theme.borderStrong : "transparent")
        border.width: root.activeFocus ? 1 : 0
        opacity: 1.0
    }
}
