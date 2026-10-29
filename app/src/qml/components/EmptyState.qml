import QtQuick 2.15
import ".."

Item {
    id: root
    property string title: "暂无数据"
    property string detail: ""
    property string actionText: ""
    property Theme theme: Theme {}
    signal actionClicked()
    implicitHeight: 150

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 420)
        spacing: 8
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.title
            color: theme.textPrimary
            font.family: theme.cjkFontFamily
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            text: root.detail
            color: theme.textSecondary
            font.family: theme.cjkFontFamily
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        SecondaryButton {
            visible: root.actionText.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.actionText
            theme: theme
            onClicked: root.actionClicked()
        }
    }
}
