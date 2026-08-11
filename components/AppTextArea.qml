import QtQuick
import QtQuick.Controls

TextArea {
    id: root
    property var theme
    leftPadding: 13
    rightPadding: 13
    topPadding: 11
    bottomPadding: 11
    selectByMouse: true
    wrapMode: TextEdit.Wrap
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
    }
}
