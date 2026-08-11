import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var theme
    property var iconFont
    property var playlist
    property color cardColor: "#13D9B0"
    signal clicked(var playlist)
    radius: 14
    color: cardColor
    clip: true
    scale: mouse.pressed ? 0.985 : (mouse.containsMouse ? 1.012 : 1)
    Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 150 : 0; easing.type: Easing.OutCubic } }

    Rectangle {
        width: 170; height: 170; radius: 85
        x: parent.width - 105; y: 28
        color: "#14000000"
    }
    Rectangle {
        width: 120; height: 120; radius: 60
        x: parent.width - 72; y: 54
        color: "#10000000"
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.topMargin: 13
        anchors.bottomMargin: 13
        width: parent.width * 0.56
        spacing: 5
        Text { text: "Discover"; color: "#171719"; font.pixelSize: 13; font.bold: true }
        Text {
            Layout.fillWidth: true
            text: root.playlist ? root.playlist.name : "发现更多好音乐"
            color: "#171719"
            font.pixelSize: 14
            font.bold: true
            maximumLineCount: 2
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }
        Item { Layout.fillHeight: true }
        Rectangle {
            width: 28; height: 20; radius: 10; color: "#171719"
            Text { anchors.centerIn: parent; text: "\uf04b"; font.family: root.iconFont ? root.iconFont.name : ""; font.pixelSize: 8; color: "white" }
        }
    }

    CoverImage {
        width: Math.min(parent.height - 24, 120)
        height: width
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        source: root.playlist ? root.playlist.cover : ""
        requestedSize: 300
        cornerRadius: 12
        placeholderColor: "#24FFFFFF"
        glyphColor: "#5B5B5B"
    }

    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked(root.playlist) }
}
