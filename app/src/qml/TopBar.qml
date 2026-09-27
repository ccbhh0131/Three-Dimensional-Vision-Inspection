import QtQuick 2.15
import "components"

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }

    Rectangle {
        anchors.fill: parent
        color: theme.surface
        border.color: theme.border
        border.width: 1
    }
    Row {
        anchors.fill: parent
        anchors.leftMargin: 22
        anchors.rightMargin: 18
        spacing: 18

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Text {
                text: "Vision3D Inspector"
                color: theme.textPrimary
                font.family: theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Text {
                text: "三维视觉检测工作台"
                color: theme.textMuted
                font.family: theme.cjkFontFamily
                font.pixelSize: 11
            }
        }

        Rectangle { width: 1; height: 30; anchors.verticalCenter: parent.verticalCenter; color: theme.divider }

        Column {
            width: Math.max(180, parent.width * 0.25)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: viewModel.projectName
                color: theme.textPrimary
                font.family: theme.cjkFontFamily
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                width: parent.width
            }
            Text {
                text: viewModel.pageTitle
                color: theme.textSecondary
                font.family: theme.cjkFontFamily
                font.pixelSize: 11
            }
        }

        Item { width: 1; height: 1 }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            SecondaryButton {
                text: "打开项目"
                compact: true
                theme: theme
                onClicked: viewModel.requestOpenProject()
            }
            AppButton {
                text: "新建项目"
                compact: true
                theme: theme
                onClicked: viewModel.requestCreateProject()
            }
            IconButton {
                iconSource: "qrc:/stage5a/icons/settings.svg"
                tooltip: "设置"
                theme: theme
                onClicked: viewModel.requestSettings()
            }
        }
    }
}
