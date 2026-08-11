import QtQuick
import QtQuick.Controls

TextField {
    id: root
    property var theme
    property bool compact: false
    implicitHeight: compact ? 34 : 40
    leftPadding: 13
    rightPadding: 13
    topPadding: 0
    bottomPadding: 0
    selectByMouse: true
    color: theme ? theme.textColor : (app.darkMode ? "#F4F4F1" : "#202126")
    placeholderTextColor: app.darkMode ? "#75766F" : "#8C8E95"
    selectionColor: app.accentColor
    selectedTextColor: "#14201D"
    font.pixelSize: 10
    background: Rectangle {
        radius: 11
        color: app.darkMode ? "#24241F" : "#F2F4F7"
        border.width: root.activeFocus ? 1.5 : 1
        border.color: root.activeFocus ? app.accentColor : (app.darkMode ? "#18FFFFFF" : "#10000000")
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 120 : 0 } }
    }
}
