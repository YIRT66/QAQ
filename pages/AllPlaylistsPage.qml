import QtQuick
import QtQuick.Layouts
import "../components" as AppC

Flickable {
    id: root
    required property var theme
    required property var iconFont
    clip: true
    contentWidth: width
    contentHeight: content.implicitHeight + 30
    boundsBehavior: Flickable.StopAtBounds

    function openPlaylist(row) {
        var id = String((row && (row.sourceId || row.id)) || "")
        if (!id.length) return
        if (row && row.providerId === "evolve") app.openCommunityPlaylist(id)
        else app.openPlaylist(id, (row && row.providerId) || "netease")
    }

    function visiblePlaylists() {
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
        append(app.homePlaylists)
        append(app.communityPlaylists)
        return out
    }

    ColumnLayout {
        id: content
        width: root.width
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout { Layout.fillWidth: true; spacing: 2
                Text { text: "全部推荐歌单"; color: theme.textColor; font.pixelSize: 24; font.bold: true }
                Text { text: "聚合 API 歌单优先展示，社区歌单自动补位；点击卡片直接打开"; color: "#8D8D95"; font.pixelSize: 9 }
            }
            Text { text: root.visiblePlaylists().length + " 个"; color: "#8D8D95"; font.pixelSize: 9 }
        }
    Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            spacing: 14
            Repeater {
                model: root.visiblePlaylists()
                delegate: Rectangle {
                    required property var modelData
                    width: Math.max(170, Math.min(220, (root.width - 56) / 4)); height: width + 58; radius: 15
                    color: cardMouse.containsMouse ? (theme.isDark ? "#33332C" : "#FFFFFF") : (theme.isDark ? "#292923" : "#F7F8FA")
                    AppC.CoverImage { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; height: parent.width - 16; source: modelData.cover || ""; requestedSize: 420; cornerRadius: 11; placeholderColor: theme.secondaryColor }
                    Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: detail.top; anchors.margins: 11; text: modelData.name || "推荐歌单"; color: theme.textColor; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight }
                    Text { id: detail; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 11; text: modelData.providerId === "evolve" ? ("by " + (modelData.ownerName || modelData.ownerUsername || "Evolve 用户")) : (modelData.description || "网易云歌单"); color: "#8C8C94"; font.pixelSize: 8; elide: Text.ElideRight }
                    MouseArea { id: cardMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.openPlaylist(modelData) }
                }
            }
        }
        Rectangle {
            visible: root.visiblePlaylists().length === 0
            Layout.fillWidth: true; Layout.preferredHeight: 150; radius: 16
            color: theme.isDark ? "#292923" : "#F7F8FA"
            Column { anchors.centerIn: parent; spacing: 7
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: app.loading ? "正在加载全部歌单…" : "暂无推荐歌单"; color: theme.textColor; font.pixelSize: 12; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "返回发现页刷新后会自动同步到这里"; color: "#8C8C94"; font.pixelSize: 8 }
            }
        }
    }

    Component.onCompleted: if (root.visiblePlaylists().length === 0) { app.refreshHome(); app.loadCommunityPlaylists() }
}
