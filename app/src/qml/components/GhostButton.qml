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
        color: theme.textSecondary
        font.family: theme.cjkFontFamily
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: theme.radiusInput
        color: !root.enabled ? theme.surfaceMuted : (root.pressed ? theme.surfaceMuted : (root.hovered ? theme.surfaceMuted : "transparent"))
        border.color: root.activeFocus ? theme.border : "transparent"
        border.width: root.activeFocus ? 1 : 0
        opacity: root.enabled ? 1.0 : 0.55
    }
}
