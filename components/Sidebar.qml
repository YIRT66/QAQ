import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var theme
    property var iconFont
    property int currentIndex: 0
    property int homeSection: 0
    property int libraryTab: 0
    property bool compact: false
    signal navigate(int index)
    signal browseRequested(int section)
    signal libraryRequested(int tab)
    signal customPlaylistRequested(string playlistId)
    signal createPlaylistRequested()

    color: theme.isDark ? "#24241E" : "#F7F8FA"
    radius: 10
    border.color: theme.isDark ? "#10FFFFFF" : "#0A000000"

    function navColor(active, hover) {
        if (active) return theme.isDark ? "#3A3A31" : "#E6E8EC"
        return hover ? (theme.isDark ? "#18FFFFFF" : "#09000000") : "transparent"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? 8 : 16
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            spacing: 8
            Rectangle {
                width: 28; height: 28; radius: 8
                color: app.accentColor
                Text { anchors.centerIn: parent; text: "E"; color: "white"; font.pixelSize: 19; font.bold: true; font.italic: true }
            }
            Text { visible: !root.compact; text: "EVOLVE"; color: theme.textColor; font.pixelSize: 18; font.bold: true; font.letterSpacing: -0.6 }
            Item { Layout.fillWidth: true }
        }

        Repeater {
            model: [
                { title: app.uiText("recommend"), icon: "\uf06c", index: 0 },
                { title: app.uiText("discover"), icon: "\uf144", index: 1 }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: 38; radius: 19
                property bool active: root.currentIndex === modelData.index
                color: root.navColor(active, navMouse.containsMouse)
                Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 130 : 0 } }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10; spacing: 10
                    Text { text: modelData.icon; font.family: iconFont.name; font.pixelSize: 12; color: active ? theme.textColor : "#77777F"; Layout.preferredWidth: 18; horizontalAlignment: Text.AlignHCenter }
                    Text { visible: !root.compact; text: modelData.title; color: theme.textColor; font.pixelSize: 12; font.bold: active; Layout.fillWidth: true }
                }
                MouseArea { id: navMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.navigate(modelData.index) }
            }
        }

        Text { visible: !root.compact; text: app.language === "en-US" ? "My Music" : "我的音乐"; color: "#8B8B92"; font.pixelSize: 10; Layout.topMargin: 20; Layout.leftMargin: 7; Layout.bottomMargin: 5 }

        Repeater {
            model: [
                { title: app.uiText("favorites"), icon: "\uf08a", tab: 0 },
                { title: app.uiText("history"), icon: "\uf017", tab: 1 }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: 38; radius: 10
                property bool active: root.currentIndex === 4 && root.libraryTab === modelData.tab
                color: root.navColor(active, libMouse.containsMouse)
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10; spacing: 10
                    Text { text: modelData.icon; font.family: iconFont.name; font.pixelSize: 12; color: active ? app.accentColor : "#77777F"; Layout.preferredWidth: 18; horizontalAlignment: Text.AlignHCenter }
                    Text { visible: !root.compact; text: modelData.title; color: theme.textColor; font.pixelSize: 12; font.bold: active; Layout.fillWidth: true }
                    Text { visible: !root.compact && modelData.tab === 0 && app.favorites.length > 0; text: app.favorites.length; color: "#A0A0A6"; font.pixelSize: 9 }
                }
                MouseArea { id: libMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.libraryRequested(modelData.tab) }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 38; radius: 10
            color: root.navColor(root.currentIndex === 5, playingMouse.containsMouse)
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10; spacing: 10
                Text { text: "\uf001"; font.family: iconFont.name; font.pixelSize: 12; color: root.currentIndex === 5 ? app.accentColor : "#77777F"; Layout.preferredWidth: 18; horizontalAlignment: Text.AlignHCenter }
                Text { visible: !root.compact; text: app.uiText("nowPlaying"); color: theme.textColor; font.pixelSize: 12; font.bold: root.currentIndex === 5; Layout.fillWidth: true }
            }
            MouseArea { id: playingMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.navigate(5) }
        }

        RowLayout {
            visible: !root.compact
            Layout.fillWidth: true
            Layout.topMargin: 20
            Layout.leftMargin: 7
            Layout.rightMargin: 6
            Layout.bottomMargin: 4
            Text { text: app.language === "en-US" ? "My Playlists" : "我的歌单"; color: "#8B8B92"; font.pixelSize: 10; Layout.fillWidth: true }
            Text {
                text: "\uf067"; font.family: iconFont.name; color: "#8B8B92"; font.pixelSize: 9
                MouseArea { anchors.fill: parent; anchors.margins: -8; cursorShape: Qt.PointingHandCursor; onClicked: root.createPlaylistRequested() }
            }
        }

        Repeater {
            model: app.customPlaylists
            delegate: Rectangle {
                required property var modelData
                required property int index
                visible: !root.compact && index < 5
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 42 : 0
                radius: 9
                color: playlistMouse.containsMouse ? (theme.isDark ? "#16FFFFFF" : "#09000000") : "transparent"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 7; anchors.rightMargin: 6; spacing: 8
                    CoverImage {
                        Layout.preferredWidth: 32; Layout.preferredHeight: 32
                        source: modelData.cover || (modelData.tracks && modelData.tracks.length > 0 ? (modelData.tracks[0].cover || "") : "")
                        requestedSize: 96; cornerRadius: 5; placeholderColor: theme.secondaryColor
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 0
                        Text { Layout.fillWidth: true; text: modelData.name || "未命名歌单"; color: theme.textColor; font.pixelSize: 10; elide: Text.ElideRight }
                        Text { text: (modelData.trackCount || (modelData.tracks ? modelData.tracks.length : 0)) + " 首"; color: "#929299"; font.pixelSize: 8 }
                    }
                }
                MouseArea { id: playlistMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.customPlaylistRequested(modelData.id) }
            }
        }

        Item { Layout.fillHeight: true }

        Repeater {
            model: [
                { title: app.language === "en-US" ? "Listen Together" : "一起听", icon: "\uf0c0", index: 7 },
                { title: app.language === "en-US" ? "Profile" : "个人主页", icon: "\uf2bd", index: 8 }
            ]
            delegate: Rectangle {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: 36; radius: 10
                color: root.navColor(root.currentIndex === modelData.index, extraMouse.containsMouse)
                RowLayout { anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 8; spacing: 10
                    Text { text: modelData.icon; font.family: iconFont.name; font.pixelSize: 12; color: root.currentIndex === modelData.index ? app.accentColor : "#77777F" }
                    Text { visible: !root.compact; text: modelData.title; color: theme.textColor; font.pixelSize: 11; font.bold: root.currentIndex === modelData.index; Layout.fillWidth: true }
                    Rectangle {
                        visible: modelData.index === 7 && app.togetherInvites.length > 0
                        width: Math.max(18, inviteCount.implicitWidth + 8); height: 18; radius: 9
                        color: app.accentColor
                        Text { id: inviteCount; anchors.centerIn: parent; text: app.togetherInvites.length > 99 ? "99+" : app.togetherInvites.length; color: "#14201D"; font.pixelSize: 7; font.bold: true }
                    }
                }
                MouseArea { id: extraMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.navigate(modelData.index) }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 36; radius: 10
            color: settingsMouse.containsMouse ? (theme.isDark ? "#18FFFFFF" : "#09000000") : "transparent"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; spacing: 10
                Text { text: "\uf013"; font.family: iconFont.name; font.pixelSize: 12; color: root.currentIndex === 6 ? app.accentColor : "#77777F" }
                Text { visible: !root.compact; text: app.uiText("settings"); color: theme.textColor; font.pixelSize: 11; font.bold: root.currentIndex === 6 }
            }
            MouseArea { id: settingsMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.navigate(6) }
        }
    }
}
