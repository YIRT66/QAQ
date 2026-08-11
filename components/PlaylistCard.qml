import QtQuick

Item {
    id: root
    property var theme
    property var playlist
    signal clicked(var playlist)
    width: 156; height: 188
    scale: mouse.pressed ? 0.975 : (mouse.containsMouse ? 1.018 : 1)
    Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 145 : 0; easing.type: Easing.OutCubic } }

    CoverImage {
        id: cover
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: width
        source: root.playlist ? root.playlist.cover : ""
        requestedSize: 320; cornerRadius: 10; placeholderColor: theme.secondaryColor
    }
    Rectangle {
        anchors.left: cover.left; anchors.right: cover.right; anchors.bottom: cover.bottom
        height: 36; radius: 10
        gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 1; color: "#8A000000" } }
        opacity: mouse.containsMouse ? 1 : 0.78
        Behavior on opacity { NumberAnimation { duration: app.animationsEnabled ? 130 : 0 } }
        Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7; text: "\uf04b  " + app.formatCount(root.playlist ? root.playlist.playCount : 0); color: "white"; font.pixelSize: 8 }
    }
    Text {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: cover.bottom; anchors.topMargin: 7
        text: root.playlist ? root.playlist.name : ""; color: theme.textColor; font.pixelSize: 10; lineHeight: 1.05
        maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight
    }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked(root.playlist) }
}
