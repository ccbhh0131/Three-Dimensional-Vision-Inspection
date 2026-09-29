import QtQuick 2.15
import QtQml 2.15

QtObject {
    // Light surfaces use graphite text; these are the single source of truth
    // for the QML presentation layer.
    readonly property color bg: "#F7F5F0"
    readonly property color surface: "#FFFEFB"
    readonly property color surfaceMuted: "#F2F0EB"
    readonly property color surfaceHover: "#ECEAE4"
    readonly property color border: "#DDD9D1"
    readonly property color borderStrong: "#C8C4BB"
    readonly property color divider: "#E7E3DB"
    readonly property color textPrimary: "#202522"
    readonly property color textSecondary: "#59615C"
    readonly property color textMuted: "#747D77"
    readonly property color textDisabled: "#8C948F"
    readonly property color icon: "#59615C"

    readonly property color primary: "#176B4D"
    readonly property color primaryHover: "#145D43"
    readonly property color primaryPressed: "#104D38"
    // QML reserves names beginning with "on" for signal handlers.
    readonly property color primaryForeground: "#FFFFFF"
    readonly property color primarySoft: "#E9F1EC"
    readonly property color primarySelected: "#DDEBE3"

    readonly property color secondaryButtonBackground: "#FFFEFB"
    readonly property color secondaryButtonHover: "#F2F0EB"
    readonly property color secondaryButtonPressed: "#E9F1EC"
    readonly property color secondaryButtonText: "#285D49"
    readonly property color secondaryButtonBorder: "#D8D5CE"

    readonly property color disabledBackground: "#F0EFEB"
    readonly property color disabledBorder: "#E1DED7"
    readonly property color disabledText: "#8C948F"

    readonly property color normal: "#2D7A57"
    readonly property color warning: "#B87919"
    readonly property color alarm: "#B4443F"
    readonly property color unknown: "#737C77"
    readonly property color info: "#6D7D74"

    // Dark three-dimensional toolbar tokens intentionally remain separate
    // from the light application surfaces.
    readonly property color toolbarBackground: "#2F3733"
    readonly property color toolbarTextPrimary: "#F4F6F4"
    readonly property color toolbarTextSecondary: "#C3CBC6"
    readonly property color toolbarTextMuted: "#A9B3AD"
    readonly property color toolbarBorder: "#647269"
    readonly property color toolbarHover: "#45534B"
    readonly property color toolbarPressed: "#3F4B44"
    readonly property color toolbarFocus: "#A7B5AC"
    readonly property color toolbarDivider: "#58645D"
    readonly property color toolbarDisabledBackground: "#36403A"
    readonly property color toolbarDisabledBorder: "#4A554E"
    readonly property color toolbarDisabledText: "#7F8A82"
    readonly property int space4: 4
    readonly property int space8: 8
    readonly property int space12: 12
    readonly property int space16: 16
    readonly property int space24: 24
    readonly property int space32: 32
    readonly property int radiusSmall: 6
    readonly property int radiusInput: 8
    readonly property int radiusCard: 12
    readonly property int radiusPanel: 14
    readonly property string fontFamily: "Segoe UI"
    readonly property string cjkFontFamily: "Microsoft YaHei UI"
}
