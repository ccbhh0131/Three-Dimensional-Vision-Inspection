import QtQuick 2.15
import ".."

Row {
    id: root
    property string title: "页面"
    property string subtitle: ""
    property Theme theme: Theme {}
    width: parent ? parent.width : 600
    spacing: 16

    Column {
        width: root.width - actions.width - root.spacing
        spacing: 4
        Text {
            text: root.title
            color: theme.textPrimary
            font.family: theme.cjkFontFamily
            font.pixelSize: 24
            font.weight: Font.DemiBold
        }
        Text {
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: theme.textSecondary
            font.family: theme.cjkFontFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }
    Item { id: actions; width: 1; height: 1 }
}
