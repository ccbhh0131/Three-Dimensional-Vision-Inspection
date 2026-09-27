import QtQuick 2.15
import "components"
import "pages"

Rectangle {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    Theme { id: theme }
    color: theme.bg

    Loader {
        anchors.fill: parent
        sourceComponent: viewModel.currentPage === "visual" ? visualPage
                       : viewModel.currentPage === "realtime" ? realtimePage
                       : viewModel.currentPage === "history" ? historyPage
                       : viewModel.currentPage === "reconstruction" ? reconstructionPage
                       : viewModel.currentPage === "settings" ? settingsPage
                       : viewModel.currentPage === "scene" ? scenePage
                       : overviewPage
    }

    Component { id: overviewPage; ProjectOverviewPage { viewModel: root.viewModel } }
    Component { id: scenePage; ScenePage { viewModel: root.viewModel } }
    Component { id: visualPage; VisualInspectionPage { viewModel: root.viewModel } }
    Component { id: realtimePage; RealtimePage { viewModel: root.viewModel } }
    Component { id: historyPage; HistoryPage { viewModel: root.viewModel } }
    Component { id: reconstructionPage; ReconstructionPage { viewModel: root.viewModel } }
    Component { id: settingsPage; EmptyState { title: "设置"; detail: "技术设置仍由现有 QWidget 对话框承载。"; theme: theme; actionText: "打开技术设置"; onActionClicked: root.viewModel.requestSettings() } }
}
