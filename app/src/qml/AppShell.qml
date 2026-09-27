import QtQuick 2.15

Item {
    id: root
    AppShellFallback { id: fallbackViewModel }
    property var viewModel: appShellViewModel ? appShellViewModel : fallbackViewModel
    signal pageRequested(string page)
}
