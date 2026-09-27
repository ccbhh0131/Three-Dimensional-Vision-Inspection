import QtQuick 2.15
import ".."

Rectangle {
    id: root
    property Theme theme: Theme {}
    default property alias content: body.data
    implicitHeight: body.implicitHeight + 32
    radius: theme.radiusCard
    color: theme.surface
    border.color: theme.border

    Column {
        id: body
        anchors.fill: parent
        anchors.margins: theme.space16
        spacing: theme.space12
    }
}
