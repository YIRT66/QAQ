import QtQuick
import QtQuick.Controls

Button {
    id: root
    property var theme
    property string variant: "secondary"   // primary | secondary | ghost | danger | soft
    property string iconText: ""
    property int buttonHeight: 36
    property int sidePadding: 16

    implicitHeight: buttonHeight
    implicitWidth: Math.max(76, contentRow.implicitWidth + sidePadding * 2)
    leftPadding: sidePadding
    rightPadding: sidePadding
    topPadding: 0
    bottomPadding: 0

    readonly property bool dark: theme ? theme.isDark : app.darkMode
    readonly property color fg: !enabled ? (dark ? "#6F706B" : "#9A9CA3")
                                  : variant === "primary" ? "#14201D"
                                  : variant === "danger" ? "#FFFFFF"
                                  : (theme ? theme.textColor : (dark ? "#F4F4F1" : "#202126"))
    readonly property color base: variant === "primary" ? app.accentColor
                                    : variant === "danger" ? "#D95656"
                                    : variant === "ghost" ? "transparent"
                                    : variant === "soft" ? (dark ? "#223A34" : "#E6F7F2")
                                    : (dark ? "#34342E" : "#EEF0F3")
    readonly property color hoverBase: variant === "primary" ? Qt.lighter(app.accentColor, 1.06)
                                         : variant === "danger" ? "#E56464"
                                         : variant === "ghost" ? (dark ? "#14FFFFFF" : "#0A000000")
                                         : variant === "soft" ? (dark ? "#294940" : "#DDF2EC")
                                         : (dark ? "#404039" : "#E3E6EA")
    readonly property color pressedBase: variant === "primary" ? Qt.darker(app.accentColor, 1.08)
                                           : variant === "danger" ? "#BF4747"
                                           : variant === "ghost" ? (dark ? "#20FFFFFF" : "#10000000")
                                           : (dark ? "#2A2A25" : "#D9DDE2")

    background: Rectangle {
        radius: 11
        color: !root.enabled ? (root.dark ? "#262621" : "#E9EBEE")
                              : root.down ? root.pressedBase
                              : root.hovered ? root.hoverBase
                              : root.base
        border.width: root.variant === "secondary" || root.variant === "ghost" ? 1 : 0
        border.color: root.dark ? "#1AFFFFFF" : "#10000000"
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 110 : 0 } }
    }

    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight
        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: root.iconText.length > 0 ? 7 : 0
            Text {
                visible: root.iconText.length > 0
                text: root.iconText
                color: root.fg
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                text: root.text
                color: root.fg
                font.pixelSize: 10
                font.bold: root.variant === "primary" || root.variant === "danger"
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    scale: down ? 0.975 : (hovered && enabled ? 1.015 : 1.0)
    opacity: enabled ? 1 : 0.58
    Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 100 : 0; easing.type: Easing.OutCubic } }
}
