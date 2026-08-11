import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    property var theme
    property var iconFont
    property bool addMode: true
    property var track: ({})
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(390, parent ? parent.width - 28 : 390)
    height: Math.min(500, parent ? parent.height - 80 : 500)
    padding: 0

    background: Rectangle { radius: 20; color: theme.isDark ? "#F52A2A24" : "#FCFFFFFF"; border.color: theme.isDark ? "#18FFFFFF" : "#12000000" }

    contentItem: ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Text { text: root.addMode ? "加入歌单" : "我的歌单"; color: theme.textColor; font.pixelSize: 17; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: "\uf00d"; font.family: iconFont.name; color: "#8B8B92"; font.pixelSize: 11; MouseArea { anchors.fill: parent; anchors.margins: -10; cursorShape: Qt.PointingHandCursor; onClicked: root.close() } }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42; radius: 13; color: theme.isDark ? "#36362F" : "#EEF0F2"
            RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 7; spacing: 8
                TextField { id: newName; Layout.fillWidth: true; placeholderText: "新歌单名称"; color: theme.textColor; font.pixelSize: 10; background: null; onAccepted: createNow() }
                Rectangle { width: 62; height: 30; radius: 15; color: newName.text.trim().length ? app.accentColor : "#6A6A6A"; Text { anchors.centerIn: parent; text: "创建"; color: newName.text.trim().length ? "#17211F" : "#CACACA"; font.pixelSize: 9; font.bold: true } MouseArea { anchors.fill: parent; enabled: newName.text.trim().length > 0; cursorShape: Qt.PointingHandCursor; onClicked: createNow() } }
            }
        }

        ListView {
            id: list; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4; model: app.customPlaylists
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width; height: 56; radius: 12
                color: itemMouse.containsMouse ? (theme.isDark ? "#3A3A32" : "#ECEEF1") : "transparent"
                RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 10
                    Rectangle { width: 36; height: 36; radius: 8; color: "#2413D9B0"; Text { anchors.centerIn: parent; text: "\uf001"; font.family: iconFont.name; color: app.accentColor; font.pixelSize: 11 } }
                    ColumnLayout { Layout.fillWidth: true; spacing: 1; Text { Layout.fillWidth: true; text: modelData.name; color: theme.textColor; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight } Text { text: (modelData.trackCount || (modelData.tracks ? modelData.tracks.length : 0)) + " 首 · 云端同步"; color: "#8D8D95"; font.pixelSize: 8 } }
                    Text { text: root.addMode ? "加入" : "打开"; color: app.accentColor; font.pixelSize: 9; font.bold: true }
                    Rectangle {
                        visible: !root.addMode
                        width: 30; height: 30; radius: 15
                        color: deleteMouse.containsMouse ? "#32D45D65" : "transparent"
                        Text { anchors.centerIn: parent; text: "\uf2ed"; font.family: iconFont.name; color: "#D45D65"; font.pixelSize: 9 }
                        MouseArea { id: deleteMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: function(mouse) { mouse.accepted = true; app.deleteCustomPlaylist(modelData.id) } }
                    }
                }
                MouseArea { id: itemMouse; anchors.fill: parent; anchors.rightMargin: root.addMode ? 0 : 38; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if (root.addMode && root.track && root.track.id) app.addTrackToCustomPlaylist(modelData.id, root.track); else app.openCustomPlaylist(modelData.id); root.close() } }
            }
            Text { anchors.centerIn: parent; visible: list.count === 0; text: "还没有自建歌单"; color: "#8D8D95"; font.pixelSize: 10 }
        }
    }

    function createNow() {
        var name = newName.text.trim()
        if (!name.length) return
        app.createCustomPlaylist(name)
        newName.text = ""
    }
}
