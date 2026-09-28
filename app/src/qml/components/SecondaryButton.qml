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
        color: theme.primary
        font.family: theme.cjkFontFamily
        font.pixelSize: root.compact ? 12 : 13
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: theme.radiusInput
        color: !root.enabled ? theme.surfaceMuted : (root.pressed ? theme.primarySoft : (root.hovered ? theme.primarySoft : theme.surface))
        border.color: root.activeFocus ? theme.primaryHover : (root.hovered || root.pressed ? theme.primary : theme.border)
        border.width: root.activeFocus ? 2 : 1
        opacity: root.enabled ? 1.0 : 0.55
        Behavior on color { ColorAnimation { duration: 140 } }
    }
}
