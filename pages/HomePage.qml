import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Flickable {
    id: root

    property var theme
    property var iconFont
    property int section: 0
    signal searchRequested(string query)
    signal allPlaylistsRequested()

    clip: true
    contentWidth: width
    contentHeight: pageColumn.implicitHeight + 42
    boundsBehavior: Flickable.StopAtBounds

    function slice(list, start, count) {
        var out = []
        if (!list)
            return out
        for (var i = start; i < Math.min(list.length, start + count); ++i)
            out.push(list[i])
        return out
    }

    function trackAt(index) {
        if (app.homeTracks && app.homeTracks.length > index)
            return app.homeTracks[index]
        return null
    }

    function playlistAt(index) {
        var rows = recommendationPlaylists()
        if (rows.length > index)
            return rows[index]
        return null
    }

    function recommendationPlaylists() {
        var out = []
        var seen = ({})
        function append(rows) {
            if (!rows) return
            for (var i = 0; i < rows.length; ++i) {
                var row = rows[i]
                var key = String((row && row.providerId) || "") + ":" +
                          String((row && (row.sourceId || row.id)) || "")
                if (!row || !key.length || seen[key]) continue
                seen[key] = true
                out.push(row)
            }
        }
        // 聚合 API 是发现页的主来源；社区歌单只负责在数量不足或接口异常时补位。
        append(app.homePlaylists)
        append(app.communityPlaylists)
        return out
    }

    function heroQuery(index) {
        if (root.section === 0) {
            return ["周杰伦", "林俊杰", "陈奕迅"][index]
        }
        return ["告五人", "五月天", "邓紫棋"][index]
    }

    function heroTitle(index) {
        var t = trackAt(index)
        if (t)
            return t.title
        return root.section === 0
               ? ["今日为你推荐", "熟悉的华语旋律", "换一种情绪"][index]
               : ["本周发现", "乐队与现场感", "新的声音"][index]
    }

    function heroArtist(index) {
        var t = trackAt(index)
        if (t)
            return t.artist
        return root.section === 0
               ? ["从云端精选开始", "经典与流行", "继续探索更多好歌"][index]
               : ["Discover", "Discover", "Discover"][index]
    }

    function heroCover(index) {
        var t = trackAt(index)
        return t && t.cover ? t.cover : ""
    }

    function playHero(index) {
        var t = trackAt(index)
        if (t)
            app.playTrack(t, "discovery")
        else
            root.searchRequested(heroQuery(index))
    }

    function openPlaylistOrSearch(index) {
        var p = playlistAt(index)
        if (!p)
            return
        var id = String(p.sourceId || p.id || "")
        if (!id.length) return
        if (p.providerId === "evolve") app.openCommunityPlaylist(id)
        else app.openPlaylist(id, p.providerId || "netease")
    }

    function playlistCover(playlist) {
        if (!playlist) return ""
        if (playlist.cover) return playlist.cover
        var tracks = playlist.tracks || []
        for (var i = tracks.length - 1; i >= 0; --i)
            if (tracks[i] && tracks[i].cover) return tracks[i].cover
        return ""
    }

    readonly property var heroColors: [
        "#12D7B0",
        "#9B84F3",
        "#EF7BDD"
    ]

    // Keep playlist cards inside their layout cell at every desktop size.
    // The old fixed 188px delegate height combined with `cover.height: width`
    // made the covers grow past the GridLayout when the window was maximized,
    // so the chart section was painted on top of the covers.
    readonly property int shelfColumns: width >= 2900 ? 12
                                        : (width >= 2600 ? 11
                                        : (width >= 2300 ? 10
                                        : (width >= 2000 ? 9
                                        : (width >= 1700 ? 8
                                        : (width >= 1580 ? 7
                                        : (width >= 1240 ? 6
                                        : (width >= 1020 ? 5
                                        : (width >= 800 ? 4
                                        : (width >= 580 ? 3 : 2)))))))))
    readonly property real shelfGap: 12
    readonly property real shelfCardWidth: Math.max(126,
        Math.min(240, (width - shelfGap * (shelfColumns - 1)) / shelfColumns))
    readonly property real shelfCoverSize: shelfCardWidth
    readonly property int shelfItemCount: Math.min(shelfColumns * 2,
                                                   recommendationPlaylists().length)

    ColumnLayout {
        id: pageColumn
        width: root.width
        spacing: 18

        // Header
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 48

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    text: "发现"
                    color: theme.textColor
                    font.pixelSize: 22
                    font.bold: true
                }

                Text {
                    text: "发现歌单、新歌与正在流行的作品"
                    color: "#898991"
                    font.pixelSize: 9
                }
            }

            Rectangle {
                width: 72
                height: 30
                radius: 15
                color: refreshMouse.containsMouse
                       ? (theme.isDark ? "#35352E" : "#E2E4E8")
                       : (theme.isDark ? "#2E2E28" : "#E8EAED")

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "\uf021"
                        font.family: iconFont.name
                        color: app.loading ? app.accentColor : "#777780"
                        font.pixelSize: 9
                    }

                    Text {
                        text: app.loading ? "刷新中" : "刷新"
                        color: theme.textColor
                        font.pixelSize: 9
                    }
                }

                MouseArea {
                    id: refreshMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.refreshHome()
                }
            }
        }

        // Three large Discover hero cards, matching the reference structure.
        GridLayout {
            Layout.fillWidth: true
            columns: root.width >= 980 ? 3 : (root.width >= 620 ? 2 : 1)
            columnSpacing: 14
            rowSpacing: 14

            Repeater {
                model: 3

                delegate: Rectangle {
                    required property int index

                    Layout.fillWidth: true
                    Layout.preferredHeight: root.width >= 980 ? 154 : 142
                    radius: 16
                    clip: true
                    color: root.heroColors[index]

                    Rectangle {
                        anchors.fill: parent
                        color: "#08000000"
                    }

                    // Decorative wave shapes.
                    Repeater {
                        model: 3

                        Rectangle {
                            required property int index
                            width: 4
                            height: 54 - index * 10
                            radius: 2
                            color: "#20FFFFFF"
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: heroCover.left
                            anchors.rightMargin: 18 + index * 7
                        }
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.top: parent.top
                        anchors.topMargin: 16
                        anchors.right: heroCover.left
                        anchors.rightMargin: 22
                        spacing: 7

                        Text {
                            width: parent.width
                            text: root.section === 0 ? "For You" : "Discover"
                            color: "#111318"
                            font.pixelSize: 18
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: root.heroTitle(index)
                            color: "#15161B"
                            font.pixelSize: 13
                            font.bold: true
                            maximumLineCount: 2
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: root.heroArtist(index)
                            color: "#B5101116"
                            font.pixelSize: 9
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        width: 34
                        height: 34
                        radius: 17
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 16
                        color: "#15161B"

                        Text {
                            anchors.centerIn: parent
                            text: "\uf04b"
                            font.family: iconFont.name
                            color: "white"
                            font.pixelSize: 10
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.playHero(index)
                        }
                    }

                    AppC.CoverImage {
                        id: heroCover
                        width: 132
                        height: 132
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        source: root.heroCover(index)
                        requestedSize: 320
                        cornerRadius: 13
                        placeholderColor: "#26000000"
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.playHero(index)
                    }
                }
            }
        }

        Rectangle {
            visible: root.recommendationPlaylists().length === 0
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            radius: 14
            color: theme.isDark ? "#292923" : "#F7F8FA"
            Column {
                anchors.centerIn: parent; spacing: 5
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: app.loading ? "正在整理推荐歌单…" : "推荐歌单暂时没有加载出来"; color: theme.textColor; font.pixelSize: 10; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: app.loading ? "歌曲推荐会先显示，歌单随后补齐" : "点击右上角刷新重新获取"; color: "#8C8C94"; font.pixelSize: 8 }
            }
        }

        // Treasure playlist shelf.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2

            Text {
                text: root.section === 0 ? "为你选的歌单" : "宝藏歌单库"
                color: theme.textColor
                font.pixelSize: 16
                font.bold: true
            }

            Rectangle {
                width: 28; height: 28; radius: 14
                color: allPlaylistMouse.containsMouse ? (theme.isDark ? "#35352E" : "#E2E4E8") : "transparent"
                Text { anchors.centerIn: parent; text: "›"; color: allPlaylistMouse.containsMouse ? app.accentColor : "#7F7F87"; font.pixelSize: 20 }
                MouseArea { id: allPlaylistMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.allPlaylistsRequested() }
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: root.recommendationPlaylists().length > 0
                text: root.recommendationPlaylists().length
                      + " 个 · 聚合 API 优先"
                color: "#96969D"
                font.pixelSize: 8
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.shelfColumns
            columnSpacing: root.shelfGap
            rowSpacing: 16

            Repeater {
                model: root.shelfItemCount

                delegate: Item {
                    required property int index
                    Layout.preferredWidth: root.shelfCardWidth
                    Layout.minimumWidth: root.shelfCardWidth
                    Layout.maximumWidth: root.shelfCardWidth
                    Layout.preferredHeight: root.shelfCoverSize + 44

                    property var p: root.playlistAt(index)
                    property var fallbackTrack: root.trackAt(index + 3)

                    AppC.CoverImage {
                        id: shelfCover
                        width: root.shelfCoverSize
                        height: width
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        source: p && p.cover
                                ? p.cover
                                : (fallbackTrack && fallbackTrack.cover
                                   ? fallbackTrack.cover : "")
                        requestedSize: 320
                        cornerRadius: 11
                        placeholderColor: theme.secondaryColor
                    }

                    Rectangle {
                        anchors.fill: shelfCover
                        radius: 11
                        color: shelfMouse.containsMouse ? "#10000000" : "transparent"

                        Behavior on color {
                            ColorAnimation {
                                duration: app.animationsEnabled ? 110 : 0
                            }
                        }
                    }

                    Rectangle {
                        visible: shelfMouse.containsMouse
                        width: 32
                        height: 32
                        radius: 16
                        anchors.centerIn: shelfCover
                        color: "#E818191F"

                        Text {
                            anchors.centerIn: parent
                            text: "\uf04b"
                            font.family: iconFont.name
                            color: "white"
                            font.pixelSize: 9
                        }
                    }

                    Text {
                        anchors.left: shelfCover.left
                        anchors.right: shelfCover.right
                        anchors.top: shelfCover.bottom
                        anchors.topMargin: 7
                        text: p && (p.name || p.title)
                              ? (p.name || p.title)
                              : (fallbackTrack ? fallbackTrack.title : "云端精选")
                        color: theme.textColor
                        font.pixelSize: 10
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: shelfMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.openPlaylistOrSearch(index)
                    }
                }
            }
        }

        // Charts: hot + new, like the reference.
        GridLayout {
            Layout.fillWidth: true
            columns: root.width >= 820 ? 2 : 1
            columnSpacing: 34
            rowSpacing: 24
            Layout.topMargin: 4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: root.section === 0 ? "猜你喜欢 · 智能排序" : "热歌榜"
                        color: theme.textColor
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Rectangle {
                        width: 26
                        height: 26
                        radius: 13
                        color: theme.isDark ? "#30302A" : "#E3E5E9"

                        Text {
                            anchors.centerIn: parent
                            text: "\uf04b"
                            font.family: iconFont.name
                            color: "#72727A"
                            font.pixelSize: 8
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (app.homeTracks.length > 0)
                                           app.playTrack(app.homeTracks[0], "discovery")
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: "实时从可播放的云端内容中整理"
                    color: "#98989F"
                    font.pixelSize: 8
                }

                Repeater {
                    model: Math.min(6, app.homeTracks.length)

                    delegate: Rectangle {
                        required property int index

                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        radius: 9
                        color: hotMouse.containsMouse
                               ? (theme.isDark ? "#14FFFFFF" : "#08000000")
                               : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            AppC.CoverImage {
                                Layout.preferredWidth: 46
                                Layout.preferredHeight: 46
                                source: app.homeTracks[index] ? app.homeTracks[index].cover : ""
                                requestedSize: 120
                                cornerRadius: 7
                                placeholderColor: theme.secondaryColor
                            }

                            Text {
                                text: index + 1
                                color: index < 3 ? app.accentColor : "#8B8B93"
                                font.pixelSize: 11
                                font.bold: index < 3
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: app.homeTracks[index] ? app.homeTracks[index].title : ""
                                    color: theme.textColor
                                    font.pixelSize: 10
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: app.homeTracks[index] ? app.homeTracks[index].artist : ""
                                    color: "#8D8D95"
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                visible: app.homeTracks[index]
                                         && app.homeTracks[index].access === "vip"
                                width: 28
                                height: 15
                                radius: 5
                                color: "#1C13D9B0"

                                Text {
                                    anchors.centerIn: parent
                                    text: "VIP"
                                    color: app.accentColor
                                    font.pixelSize: 7
                                    font.bold: true
                                }
                            }

                            AppC.PlayerIconButton {
                                width: 28
                                height: 28
                                iconFont: root.iconFont
                                glyph: "\uf04b"
                                glyphSize: 8
                                glyphColor: "#888890"
                                onClicked: app.playTrack(app.homeTracks[index], "discovery")
                            }
                        }

                        MouseArea {
                            id: hotMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                Rectangle {
                    visible: app.homeTracks.length === 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    radius: 14
                    color: theme.isDark ? "#292923" : "#FFFFFF"
                    border.color: theme.isDark ? "#10FFFFFF" : "#10000000"

                    Column {
                        anchors.centerIn: parent
                        spacing: 7

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "正在从 Ourcraft Music API 加载榜单…"
                            color: theme.textColor
                            font.pixelSize: 10
                            font.bold: true
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "网易 / 酷狗 / 酷我会自动检测并选择可用来源"
                            color: "#96969D"
                            font.pixelSize: 8
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: root.section === 0 ? "最近播放" : "新歌榜"
                        color: theme.textColor
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Rectangle {
                        width: 26
                        height: 26
                        radius: 13
                        color: theme.isDark ? "#30302A" : "#E3E5E9"

                        Text {
                            anchors.centerIn: parent
                            text: "\uf04b"
                            font.family: iconFont.name
                            color: "#72727A"
                            font.pixelSize: 8
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Text {
                    text: root.section === 0
                          ? "继续听你最近播放过的内容 · 推荐会结合收藏、历史和探索度自动变化"
                          : "从云端推荐中发现新内容"
                    color: "#98989F"
                    font.pixelSize: 8
                }

                Repeater {
                    model: {
                        if (root.section === 0 && app.history.length > 0)
                            return Math.min(6, app.history.length)
                        return Math.min(6, Math.max(0, app.homeTracks.length - 6))
                    }

                    delegate: Rectangle {
                        required property int index

                        property var rowTrack: root.section === 0 && app.history.length > 0
                                               ? app.history[index]
                                               : app.homeTracks[index + 6]

                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        radius: 9
                        color: newMouse.containsMouse
                               ? (theme.isDark ? "#14FFFFFF" : "#08000000")
                               : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 10

                            AppC.CoverImage {
                                Layout.preferredWidth: 46
                                Layout.preferredHeight: 46
                                source: rowTrack ? rowTrack.cover : ""
                                requestedSize: 120
                                cornerRadius: 7
                                placeholderColor: theme.secondaryColor
                            }

                            Text {
                                text: index + 1
                                color: index < 3 ? app.accentColor : "#8B8B93"
                                font.pixelSize: 11
                                font.bold: index < 3
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    Layout.fillWidth: true
                                    text: rowTrack ? rowTrack.title : ""
                                    color: theme.textColor
                                    font.pixelSize: 10
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: rowTrack ? rowTrack.artist : ""
                                    color: "#8D8D95"
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                }
                            }

                            AppC.PlayerIconButton {
                                width: 28
                                height: 28
                                iconFont: root.iconFont
                                glyph: "\uf04b"
                                glyphSize: 8
                                glyphColor: "#888890"
                                onClicked: if (rowTrack) app.playTrack(rowTrack, root.section === 0 ? "history" : "discovery")
                            }
                        }

                        MouseArea {
                            id: newMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }
        }

        ColumnLayout {
            visible: app.communityPlaylists.length > 0
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Evolve 社区歌单"; color: theme.textColor; font.pixelSize: 17; font.bold: true }
                Text { text: "来自用户公开发布的歌单"; color: "#919198"; font.pixelSize: 8 }
                Item { Layout.fillWidth: true }
                Text { text: app.communityPlaylists.length + " 个"; color: "#919198"; font.pixelSize: 8 }
            }
            Flow {
                Layout.fillWidth: true; spacing: 12
                Repeater {
                    model: Math.min(6, app.communityPlaylists.length)
                    delegate: Rectangle {
                        required property int index
                        property var p: app.communityPlaylists[index]
                        width: Math.max(150, Math.min(190, (root.width - 60) / 5)); height: width + 54; radius: 14
                        color: communityMouse.containsMouse ? (theme.isDark ? "#32322B" : "#FFFFFF") : (theme.isDark ? "#292923" : "#F7F8FA")
                        Behavior on scale { NumberAnimation { duration: app.animationLevel === "rich" ? 130 : 0 } }
                        scale: communityMouse.containsMouse && app.animationLevel === "rich" ? 1.018 : 1
                        AppC.CoverImage { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; height: parent.width - 16; source: root.playlistCover(p); requestedSize: 360; cornerRadius: 10 }
                        Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: ownerText.top; anchors.leftMargin: 10; anchors.rightMargin: 10; text: p ? (p.name || "社区歌单") : ""; color: theme.textColor; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
                        Text { id: ownerText; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; text: p ? ("by " + (p.ownerName || p.ownerUsername || "Evolve 用户")) : ""; color: "#8C8C94"; font.pixelSize: 8; elide: Text.ElideRight }
                        MouseArea { id: communityMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: if(p) app.openCommunityPlaylist(p.id || "") }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
        }
    }

    Component.onCompleted: app.loadCommunityPlaylists()
}
