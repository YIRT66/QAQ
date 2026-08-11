import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var theme
    property var iconFont
    property var track
    property int index: 0
    property bool active: player.currentTrack.id === (track ? track.id : "")
    property string context: "auto"
    property bool favorite: { var snapshot = app.favorites; return app.isFavorite(root.track ? root.track.id : "") }
    property bool removable: false
    signal playRequested(var track, string context)
    signal removeRequested(var track)

    height: app.compactTrackRows ? 54 : 64
    radius: 9
    color: active ? (theme.isDark ? "#2F3029" : "#E8EAEE") : (rowHover.hovered ? (theme.isDark ? "#2B2B25" : "#F1F2F5") : "transparent")
    Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 110 : 0 } }

    Rectangle {
        visible: root.active
        width: 3; height: 22; radius: 2
        anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
        color: app.accentColor
        opacity: player.playing ? 1 : 0.55
    }

    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 7; spacing: 10
        Text {
            text: root.active && player.playing ? "\uf04b" : String(root.index + 1)
            font.family: root.active && player.playing ? iconFont.name : ""
            color: root.active ? app.accentColor : "#929299"; font.pixelSize: 10
            Layout.preferredWidth: 22; horizontalAlignment: Text.AlignHCenter
        }
        CoverImage {
            Layout.preferredWidth: app.compactTrackRows ? 38 : 46; Layout.preferredHeight: Layout.preferredWidth
            source: root.track ? root.track.cover : ""; requestedSize: 110; cornerRadius: 6; placeholderColor: theme.secondaryColor
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 1
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                Text { Layout.fillWidth: true; text: root.track ? root.track.title : ""; color: theme.textColor; font.pixelSize: app.compactTrackRows ? 11 : 12; font.bold: root.active; elide: Text.ElideRight }
                Rectangle {
                    visible: root.track && (root.track.access === "vip" || root.track.access === "account" || root.track.playable === false)
                    width: badgeText.implicitWidth + 10; height: 14; radius: 4
                    color: root.track && root.track.playable === false ? "#22D96565" : "#2013D9B0"
                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: root.track && root.track.playable === false ? "锁定" : (root.track && root.track.access === "account" ? "账号" : "VIP")
                        color: root.track && root.track.playable === false ? "#D96565" : app.accentColor
                        font.pixelSize: 7; font.bold: true
                    }
                }
            }
            Text { Layout.fillWidth: true; text: root.track ? root.track.artist : ""; color: "#8E8E95"; font.pixelSize: 9; elide: Text.ElideRight }
        }
        Text { visible: root.width > 720; Layout.preferredWidth: 150; text: root.track ? root.track.album : ""; color: "#929299"; font.pixelSize: 9; elide: Text.ElideRight }
        Rectangle {
            visible: app.showSourceBadges && root.width > 940 && root.track
            Layout.preferredWidth: sourceText.implicitWidth + 14; Layout.preferredHeight: 20; radius: 10
            color: theme.isDark ? "#303029" : "#ECEEF1"
            Text { id: sourceText; anchors.centerIn: parent; text: root.track ? ((root.track.sourceCount || 1) > 1 ? ((root.track.sourceCount || 1) + " 源") : (root.track.providerName || "")) : ""; color: "#85858C"; font.pixelSize: 8 }
        }
        Text { Layout.preferredWidth: 42; text: app.formatDuration(root.track ? root.track.duration : 0); color: "#8E8E95"; font.pixelSize: 9; horizontalAlignment: Text.AlignRight }
        PlayerIconButton { visible: root.removable && root.width > 620; width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf2ed"; glyphSize: 10; glyphColor: "#C85D65"; onClicked: if (root.track) root.removeRequested(root.track) }
        FavoriteButton { visible: root.width > 560; iconFont: root.iconFont; checked: root.favorite; normalColor: "#7F7F86"; buttonSize: 34; onClicked: if (root.track) app.toggleFavorite(root.track) }
        PlayerIconButton {
            width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf141"; glyphSize: 12; glyphColor: "#85858C"
            visible: root.width > 650 && (rowHover.hovered || root.active)
            onClicked: if (root.track) app.downloadTrack(root.track)
        }
    }
    HoverHandler { id: rowHover; acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: false; z: -1; cursorShape: Qt.PointingHandCursor; onDoubleClicked: root.playRequested(root.track, root.context) }
}
