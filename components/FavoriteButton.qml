import QtQuick

Item {
    id: root
    property var iconFont
    property bool checked: false
    property int buttonSize: 38
    property color normalColor: "#77777F"
    property color activeColor: "#FF4D68"
    signal clicked()
    width: buttonSize
    height: buttonSize

    Rectangle {
        id: hit
        anchors.centerIn: parent
        width: root.buttonSize
        height: root.buttonSize
        radius: width / 2
        color: mouse.containsMouse ? (root.checked ? "#12FF4D68" : "#0E000000") : "transparent"
        scale: mouse.pressed ? 0.88 : 1
        Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 95 : 0 } }
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 120 : 0 } }

        Text {
            id: heart
            anchors.centerIn: parent
            text: root.checked ? "\uf004" : "\uf08a"
            font.family: root.iconFont ? root.iconFont.name : ""
            font.pixelSize: 17
            color: root.checked ? root.activeColor : root.normalColor
            Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 150 : 0 } }
        }

        Rectangle {
            id: ring
            anchors.centerIn: parent
            width: 14; height: 14; radius: 7
            color: "transparent"
            border.width: 2
            border.color: root.activeColor
            opacity: 0
        }

        SequentialAnimation {
            id: pop
            ParallelAnimation {
                NumberAnimation { target: heart; property: "scale"; to: 1.35; duration: app.animationsEnabled ? 100 : 0; easing.type: Easing.OutCubic }
                NumberAnimation { target: ring; property: "scale"; from: 0.65; to: 2.6; duration: app.animationsEnabled ? 220 : 0 }
                NumberAnimation { target: ring; property: "opacity"; from: 0.72; to: 0; duration: app.animationsEnabled ? 220 : 0 }
            }
            NumberAnimation { target: heart; property: "scale"; to: 1.0; duration: app.animationsEnabled ? 140 : 0; easing.type: Easing.OutBack }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                pop.restart()
                root.clicked()
            }
        }
    }
}
