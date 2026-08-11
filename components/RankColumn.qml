import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var theme
    property var iconFont
    property string title: "新歌速递"
    property string subtitle: "实时更新"
    property var tracks: []
    property string context: "discovery"
    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 7
        ColumnLayout {
            Layout.fillWidth: true; spacing: 1
            Text { text: root.title; color: theme.textColor; font.pixelSize: 17; font.bold: true }
            Text { text: root.subtitle; color: "#919198"; font.pixelSize: 9 }
        }
        Rectangle {
            width: 24; height: 24; radius: 12
            color: mouse.containsMouse ? (theme.isDark ? "#24FFFFFF" : "#0D000000") : "transparent"
            Text { anchors.centerIn: parent; text: "\uf04b"; font.family: root.iconFont ? root.iconFont.name : ""; font.pixelSize: 8; color: theme.textColor }
            MouseArea {
                id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: if (root.tracks.length > 0) app.playTrack(root.tracks[0], root.context)
            }
        }
    }

    Repeater {
        model: Math.min(6, root.tracks.length)
        delegate: Rectangle {
            required property int index
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            radius: 9
            color: rowMouse.containsMouse ? (theme.isDark ? "#16FFFFFF" : "#0A000000") : "transparent"
            Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 100 : 0 } }
            RowLayout {
                anchors.fill: parent; spacing: 9
                Text { text: index + 1; color: "#898990"; font.pixelSize: 11; Layout.preferredWidth: 16; horizontalAlignment: Text.AlignHCenter }
                CoverImage {
                    Layout.preferredWidth: 38; Layout.preferredHeight: 38
                    source: root.tracks[index] ? root.tracks[index].cover : ""
                    requestedSize: 96; cornerRadius: 5; placeholderColor: theme.secondaryColor
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    RowLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { Layout.fillWidth: true; text: root.tracks[index] ? root.tracks[index].title : ""; color: theme.textColor; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight }
                        Rectangle {
                            visible: root.tracks[index] && root.tracks[index].access === "vip"
                            width: 26; height: 13; radius: 4; color: "#1C13D9B0"
                            Text { anchors.centerIn: parent; text: "VIP"; color: app.accentColor; font.pixelSize: 7; font.bold: true }
                        }
                    }
                    Text { Layout.fillWidth: true; text: root.tracks[index] ? root.tracks[index].artist : ""; color: "#898990"; font.pixelSize: 9; elide: Text.ElideRight }
                }
                PlayerIconButton {
                    width: 28; height: 28; iconFont: root.iconFont; glyph: "\uf04b"; glyphSize: 8
                    glyphColor: "#9A9AA1"; hoverColor: theme.isDark ? "#20FFFFFF" : "#10000000"
                    onClicked: app.playTrack(root.tracks[index], root.context)
                }
            }
            MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
        }
    }
}
