import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property string label: "指标"
    property string value: "—"
    property string detail: ""
    property color accent: theme.primary
    property Theme theme: Theme {}

    implicitWidth: 180
    implicitHeight: 108
    radius: theme.radiusCard
    color: theme.surface
    border.color: theme.border

    Rectangle {
        width: 4
        height: parent.height - 28
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        radius: 2
        color: root.accent
    }
    Column {
        anchors.left: parent.left
        anchors.leftMargin: 30
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5
        Text {
            text: root.label
            color: theme.textSecondary
            font.family: theme.cjkFontFamily
            font.pixelSize: 12
        }
        Text {
            text: root.value
            color: theme.textPrimary
            font.family: theme.fontFamily
            font.pixelSize: 26
            font.weight: Font.DemiBold
        }
        Text {
            visible: root.detail.length > 0
            text: root.detail
            color: theme.textMuted
            font.family: theme.cjkFontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
