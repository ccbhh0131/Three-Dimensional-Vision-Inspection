import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Button {
    id: root
    property bool compact: false
    property Theme theme: Theme {}

    hoverEnabled: true
    implicitWidth: Math.max(92, label.implicitWidth + 30)
    implicitHeight: compact ? 32 : 36

    contentItem: Text {
        id: label
        text: root.text
        color: root.enabled ? theme.secondaryButtonText : theme.disabledText
        font.family: theme.cjkFontFamily
        font.pixelSize: root.compact ? 12 : 13
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: theme.radiusInput
        color: !root.enabled ? theme.disabledBackground : (root.pressed ? theme.secondaryButtonPressed : (root.hovered ? theme.secondaryButtonHover : theme.secondaryButtonBackground))
        border.color: !root.enabled ? theme.disabledBorder : (root.activeFocus ? theme.primaryHover : (root.hovered || root.pressed ? theme.secondaryButtonText : theme.secondaryButtonBorder))
        border.width: root.activeFocus ? 2 : 1
        opacity: 1.0
        Behavior on color { ColorAnimation { duration: 140 } }
    }
}
