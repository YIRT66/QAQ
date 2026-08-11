import QtQuick
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    required property var theme
    required property var iconFont
    property var artist: ({})

    function artistTracks() {
        var out = []
        var target = String(artist.name || "").toLowerCase()
        var rows = app.searchResults || []
        for (var i = 0; i < rows.length; ++i) {
            var name = String(rows[i].artist || "").toLowerCase()
            if (!target.length || name.indexOf(target) >= 0 || target.indexOf(name) >= 0)
                out.push(rows[i])
        }
        return out.length ? out : rows
    }

    onArtistChanged: {
        var name = String((artist && artist.name) || "")
        if (name.length) app.search(name)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 174
            spacing: 22
            AppC.CoverImage {
                Layout.preferredWidth: 158; Layout.preferredHeight: 158
                source: root.artist.cover || ""; requestedSize: 420; cornerRadius: 79
                placeholderColor: theme.secondaryColor
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 7
                Text { text: "歌手"; color: app.accentColor; font.pixelSize: 9; font.bold: true }
                Text { Layout.fillWidth: true; text: root.artist.name || "未知歌手"; color: theme.textColor; font.pixelSize: 30; font.bold: true; elide: Text.ElideRight }
                Text { Layout.fillWidth: true; text: (root.artist.aliases || []).join(" · "); color: "#8B8B93"; font.pixelSize: 10; elide: Text.ElideRight }
                Text { text: (root.artist.trackCount || root.artistTracks().length) + " 首歌曲  ·  " + (root.artist.albumCount || 0) + " 张专辑"; color: "#8B8B93"; font.pixelSize: 9 }
                Rectangle {
                    width: 108; height: 34; radius: 17; color: app.accentColor
                    Row { anchors.centerIn: parent; spacing: 7
                        Text { text: "▶"; color: "#15201D"; font.pixelSize: 9 }
                        Text { text: "播放热门"; color: "#15201D"; font.pixelSize: 9; font.bold: true }
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { var rows=root.artistTracks(); if(rows.length) app.playTrack(rows[0], "search") } }
                }
            }
        }
        RowLayout { Layout.fillWidth: true
            Text { text: "热门单曲"; color: theme.textColor; font.pixelSize: 17; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: root.artistTracks().length + " 首"; color: "#8B8B93"; font.pixelSize: 8 }
        }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 1
            model: root.artistTracks(); reuseItems: true; cacheBuffer: 1000
            delegate: AppC.TrackRow {
                required property var modelData
                width: ListView.view.width; theme: root.theme; iconFont: root.iconFont; track: modelData; context: "search"
                onPlayRequested: function(t,c) { app.playTrack(t,c) }
            }
        }
    }
}
