import QtQuick 2.15
import QtQuick.Controls 2.15
import ".."

Button {
    id: root
    property color accent: theme.primary
    property color accentHover: theme.primaryHover
    property color accentPressed: theme.primaryPressed
    property bool busy: false
    property bool compact: false
    property alias fontSize: label.font.pixelSize
    property Theme theme: Theme {}

    hoverEnabled: true
    enabled: !busy
    implicitWidth: Math.max(92, label.implicitWidth + 32)
    implicitHeight: compact ? 32 : 36

    contentItem: Text {
        id: label
        text: root.busy ? "处理中…" : root.text
        color: theme.surface
        font.family: theme.cjkFontFamily
        font.pixelSize: root.compact ? 12 : 13
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: theme.radiusInput
        color: !root.enabled ? theme.surfaceMuted : (root.pressed ? root.accentPressed : (root.hovered ? root.accentHover : root.accent))
        border.color: root.activeFocus ? theme.primaryHover : color
        border.width: root.activeFocus ? 2 : 1
        opacity: root.enabled ? 1.0 : 0.55
        Behavior on color { ColorAnimation { duration: 140 } }
    }
}
