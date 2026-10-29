import QtQuick 2.15
import ".."

Row {
    id: root
    property string title: "面板"
    property string detail: ""
    property Theme theme: Theme {}
    width: parent ? parent.width : 280
    spacing: 8

    Text {
        text: root.title
        color: theme.textPrimary
        font.family: theme.cjkFontFamily
        font.pixelSize: 16
        font.weight: Font.DemiBold
    }
    Text {
        visible: root.detail.length > 0
        text: root.detail
        color: theme.textMuted
        font.family: theme.cjkFontFamily
        font.pixelSize: 11
        anchors.baseline: parent ? parent.children[0].baseline : undefined
    }
}
