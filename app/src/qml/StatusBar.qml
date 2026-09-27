import QtQuick 2.15

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }
    Rectangle { anchors.fill: parent; color: theme.surface; border.color: theme.border; border.width: 1 }
    Item {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        Text {
            id: statusDot
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "●"
            color: theme.normal
            font.pixelSize: 10
        }
        Text {
            anchors.left: statusDot.right
            anchors.leftMargin: 10
            anchors.right: implementationLabel.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: viewModel.statusBarText
            color: theme.textSecondary
            font.family: theme.cjkFontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Text {
            id: implementationLabel
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "QML Hybrid · OpenGL Viewer"
            color: theme.textMuted
            font.family: theme.fontFamily
            font.pixelSize: 10
        }
    }
}
