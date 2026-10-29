import QtQuick 2.15
import ".."
import "../components"

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        radius: theme.radiusCard
        color: theme.surface
        border.color: theme.border
        EmptyState {
            anchors.fill: parent
            anchors.margins: theme.space16
            title: "三维场景已切换"
            detail: "OpenGL Viewer 由 QWidget 承载，QML 负责工具条与检查器。"
            theme: theme
            actionText: "加载模型"
            onActionClicked: viewModel.requestOpenViewer()
        }
    }
}
