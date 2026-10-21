import QtQuick 2.15
import ".."

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
    Item {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 18
        anchors.bottomMargin: 14

        Column {
            id: primaryNavigation
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 4

            Text {
                text: "工作区"
                color: theme.textMuted
                font.family: theme.cjkFontFamily
                font.pixelSize: 11
                leftPadding: 10
                bottomPadding: 8
            }
            NavigationItem { label: "项目总览"; iconSource: "qrc:/stage5a/icons/project.svg"; selected: viewModel.currentPage === "overview"; theme: theme; onClicked: viewModel.selectPage("overview") }
            NavigationItem { label: "三维场景"; iconSource: "qrc:/stage5a/icons/scene.svg"; selected: viewModel.currentPage === "scene"; theme: theme; onClicked: viewModel.selectPage("scene") }
            NavigationItem { label: "视觉巡检"; iconSource: "qrc:/stage5a/icons/inspection.svg"; selected: viewModel.currentPage === "visual"; theme: theme; onClicked: viewModel.selectPage("visual") }
            NavigationItem { label: "实时监控"; iconSource: "qrc:/stage5a/icons/realtime.svg"; selected: viewModel.currentPage === "realtime"; theme: theme; onClicked: viewModel.selectPage("realtime") }
            NavigationItem { label: "历史记录"; iconSource: "qrc:/stage5a/icons/history.svg"; selected: viewModel.currentPage === "history"; theme: theme; onClicked: viewModel.selectPage("history") }
            NavigationItem { label: "重建任务"; iconSource: "qrc:/stage5a/icons/reconstruction.svg"; selected: viewModel.currentPage === "reconstruction"; theme: theme; onClicked: viewModel.selectPage("reconstruction") }
        }

        NavigationItem {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            label: "设置"
            iconSource: "qrc:/stage5a/icons/settings.svg"
            selected: viewModel.currentPage === "settings"
            theme: theme
            onClicked: { viewModel.selectPage("settings"); viewModel.requestSettings() }
        }
    }
}
