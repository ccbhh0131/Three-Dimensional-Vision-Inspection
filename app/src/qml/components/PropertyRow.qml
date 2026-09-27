import QtQuick 2.15
import ".."

Row {
    id: root
    property string label: "属性"
    property string value: "—"
    property Theme theme: Theme {}
    width: parent ? parent.width : 280
    spacing: 12

    Text {
        width: Math.min(92, root.width * 0.34)
        text: root.label
        color: theme.textSecondary
        font.family: theme.cjkFontFamily
        font.pixelSize: 12
        elide: Text.ElideRight
    }
    Text {
        width: root.width - 104
        text: root.value
        color: theme.textPrimary
        font.family: theme.cjkFontFamily
        font.pixelSize: 12
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignRight
    }
}
