import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property string text: "未知"
    property string statusCode: "unknown"
    property Theme theme: Theme {}

    implicitWidth: label.implicitWidth + 22
    implicitHeight: 26
    radius: 13
    color: chipColor(root.statusCode, 0.12)
    border.color: chipColor(root.statusCode, 0.28)

    function chipColor(code, alpha) {
        if (code === "normal" || code === "running" || code === "completed") {
            return Qt.rgba(theme.normal.r, theme.normal.g, theme.normal.b, alpha)
        }
        if (code === "warning" || code === "preparing") {
            return Qt.rgba(theme.warning.r, theme.warning.g, theme.warning.b, alpha)
        }
        if (code === "alarm" || code === "failed" || code === "error") {
            return Qt.rgba(theme.alarm.r, theme.alarm.g, theme.alarm.b, alpha)
        }
        return Qt.rgba(theme.unknown.r, theme.unknown.g, theme.unknown.b, alpha)
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: chipColor(root.statusCode, 1.0)
        font.family: theme.cjkFontFamily
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }
}
