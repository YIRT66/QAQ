import QtQuick

Rectangle {
    id: root
    property var iconFont
    property string glyph: ""
    property color glyphColor: "#34343A"
    property color normalColor: "transparent"
    property color hoverColor: "#10000000"
    property color pressedColor: "#18000000"
    property int glyphSize: 17
    property bool useIconFont: true
    property bool checked: false
    property color checkedColor: app.accentColor
    signal clicked()

    radius: Math.min(width, height) / 2
    color: !enabled ? "transparent" : (mouse.pressed ? pressedColor : (mouse.containsMouse ? hoverColor : normalColor))
    opacity: enabled ? 1 : 0.35
    scale: mouse.pressed ? 0.88 : (mouse.containsMouse ? 1.04 : 1.0)
    Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 110 : 0 } }
    Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 110 : 0; easing.type: Easing.OutCubic } }

    Text {
        anchors.centerIn: parent
        text: root.glyph
        color: root.checked ? root.checkedColor : root.glyphColor
        font.family: root.useIconFont && root.iconFont ? root.iconFont.name : ""
        font.pixelSize: root.glyphSize
        renderType: Text.NativeRendering
        scale: root.checked ? 1.04 : 1
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 140 : 0 } }
        Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 150 : 0; easing.type: Easing.OutBack } }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
