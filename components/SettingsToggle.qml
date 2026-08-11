import QtQuick

Rectangle {
    id: root
    property bool checked: false
    signal toggled(bool value)
    width: 44
    height: 25
    radius: 13
    color: checked ? app.accentColor : (app.darkMode ? "#41413A" : "#D9DDE2")
    border.width: 1
    border.color: checked ? Qt.darker(app.accentColor, 1.08) : (app.darkMode ? "#18FFFFFF" : "#10000000")
    scale: mouse.pressed ? 0.96 : 1
    Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 150 : 0 } }
    Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 90 : 0 } }

    Rectangle {
        width: 19; height: 19; radius: 10
        x: root.checked ? root.width - width - 3 : 3
        anchors.verticalCenter: parent.verticalCenter
        color: "white"
        Behavior on x { NumberAnimation { duration: app.animationsEnabled ? 170 : 0; easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
