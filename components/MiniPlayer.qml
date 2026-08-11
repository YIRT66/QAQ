pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var theme
    property var iconFont
    property bool immersive: false
    property bool compact: false
    property bool tiny: false
    readonly property string playerStyle: app.playerStyle || "balanced"
    property double rememberedVolume: 0.55
    signal openNowPlaying()
    signal queueRequested()
    signal addToPlaylistRequested()

    radius: playerStyle === "centered" ? 18 : (playerStyle === "cover" ? 12 : 8)
    color: immersive ? "#414034" : (playerStyle === "centered"
           ? (theme.isDark ? "#24251F" : "#F3F4F6")
           : (playerStyle === "cover" ? (theme.isDark ? "#303027" : "#FFFDF8")
                                      : (theme.isDark ? "#292922" : "#FAFBFC")))
    border.width: playerStyle === "cover" ? 1 : 1
    border.color: immersive ? "#12FFFFFF" : (playerStyle === "cover" ? "#3513D9B0" : (theme.isDark ? "#12FFFFFF" : "#0D000000"))
    Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 180 : 0 } }

    readonly property bool favorite: { var snapshot = app.favorites; return app.isFavorite(player.currentTrack.id || "") }
    property color primaryText: immersive ? "#F4F2EA" : theme.textColor
    property color secondaryText: immersive ? "#B5B3A8" : "#818188"
    property color controlText: immersive ? "#EAE8DF" : theme.textColor

    component ChoicePopup: Popup {
        id: choicePopup
        property string heading: ""
        property var choices: []
        property string selectedValue: ""
        signal choiceSelected(string value)
        width: 224
        height: 48 + choices.length * 42 + 10
        padding: 6
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: app.animationsEnabled ? 130 : 0 }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: app.animationsEnabled ? 150 : 0; easing.type: Easing.OutCubic }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: app.animationsEnabled ? 90 : 0 } }
        background: Rectangle {
            radius: 15
            color: root.theme.isDark ? "#F532322B" : "#FCFFFFFF"
            border.width: 1
            border.color: root.theme.isDark ? "#1FFFFFFF" : "#14000000"
        }
        contentItem: ColumnLayout {
            spacing: 2
            Text {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                Layout.leftMargin: 10; Layout.rightMargin: 8
                text: choicePopup.heading; color: root.primaryText
                verticalAlignment: Text.AlignVCenter; font.pixelSize: 11; font.bold: true
            }
            Repeater {
                model: choicePopup.choices
                delegate: Rectangle {
                    id: optionRow
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredHeight: 40; radius: 10
                    readonly property bool selected: choicePopup.selectedValue === String(modelData.value)
                    color: selected ? (root.theme.isDark ? "#343E38" : "#E7F5F0")
                                    : (choiceMouse.containsMouse ? (root.theme.isDark ? "#3A3A33" : "#EEF1F4") : "transparent")
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 9
                        Rectangle {
                            width: 20; height: 20; radius: 7
                            color: optionRow.selected ? app.accentColor : (root.theme.isDark ? "#3D3D36" : "#E5E7EA")
                            Text { anchors.centerIn: parent; text: optionRow.selected ? "✓" : ""; color: "#14201D"; font.pixelSize: 10; font.bold: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 0
                            Text { Layout.fillWidth: true; text: optionRow.modelData.label || ""; color: root.primaryText; font.pixelSize: 9; font.bold: optionRow.selected; elide: Text.ElideRight }
                            Text { Layout.fillWidth: true; visible: String(modelData.detail || "").length > 0; text: modelData.detail || ""; color: root.secondaryText; font.pixelSize: 7; elide: Text.ElideRight }
                        }
                    }
                    MouseArea {
                        id: choiceMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        onClicked: { choicePopup.choiceSelected(String(modelData.value)); choicePopup.close() }
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: progress.top
        anchors.leftMargin: root.tiny ? 7 : 10; anchors.rightMargin: root.tiny ? 7 : 12; anchors.topMargin: root.tiny ? 4 : 6; anchors.bottomMargin: 4; spacing: root.tiny ? 4 : 8

        Item {
            Layout.preferredWidth: root.tiny ? Math.min(132, root.width * 0.30)
                                                    : (root.compact ? 150
                                                       : (root.playerStyle === "cover" ? Math.min(360, Math.max(260, root.width * 0.29))
                                                                                       : (root.playerStyle === "centered" ? 190 : Math.min(310, Math.max(210, root.width * 0.25)))))
            Layout.fillHeight: true
            RowLayout {
                anchors.fill: parent; spacing: 10
                CoverImage { Layout.preferredWidth: root.tiny ? 38 : (root.compact ? 44 : (root.playerStyle === "cover" ? 62 : (root.playerStyle === "centered" ? 46 : 54))); Layout.preferredHeight: Layout.preferredWidth; source: player.currentTrack.cover || ""; requestedSize: 160; cornerRadius: root.playerStyle === "centered" ? Layout.preferredWidth / 2 : 7; placeholderColor: theme.secondaryColor }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    RowLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { Layout.fillWidth: true; text: player.currentTrack.title || "选择一首歌开始"; color: root.primaryText; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight }
                        Rectangle {
                            visible: app.activeSourceAccess === "vip"; width: 25; height: 13; radius: 4; color: "#2213D9B0"
                            Text { anchors.centerIn: parent; text: "VIP"; color: app.accentColor; font.pixelSize: 7; font.bold: true }
                        }
                    }
                    Text { Layout.fillWidth: true; text: player.busy ? player.engineStatus : (player.currentTrack.artist || ""); color: player.busy ? app.accentColor : root.secondaryText; font.pixelSize: 9; elide: Text.ElideRight }
                }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.openNowPlaying() }
        }

        Item { Layout.fillWidth: true }
        RowLayout {
            spacing: root.tiny ? 1 : 5
            FavoriteButton { visible: !root.compact && !root.tiny; iconFont: root.iconFont; checked: root.favorite; normalColor: root.secondaryText; buttonSize: 38; onClicked: if (player.currentTrack.id) app.toggleFavorite(player.currentTrack) }
            PlayerIconButton { width: root.tiny ? 34 : 38; height: width; iconFont: root.iconFont; glyph: "\uf048"; glyphSize: 13; glyphColor: root.controlText; onClicked: player.previous() }
            PlayerIconButton {
                width: root.tiny ? 42 : 46; height: width; iconFont: root.iconFont; glyph: player.playing ? "\uf04c" : "\uf04b"; glyphSize: 14
                glyphColor: immersive ? "#333229" : (root.playerStyle === "cover" ? "#15201D" : theme.textColor); normalColor: immersive ? "#EEECE4" : (root.playerStyle === "cover" ? app.accentColor : (theme.isDark ? "#E8E9E9" : "#E7E8EB"))
                hoverColor: immersive ? "#FFFFFF" : (theme.isDark ? "#FFFFFF" : "#DDDEE3"); pressedColor: "#D1D2D6"; onClicked: player.togglePlay()
            }
            PlayerIconButton { width: root.tiny ? 34 : 38; height: width; iconFont: root.iconFont; glyph: "\uf051"; glyphSize: 13; glyphColor: root.controlText; onClicked: player.next() }
            PlayerIconButton {
                visible: true
                width: root.tiny ? 32 : 38; height: width
                iconFont: root.iconFont
                // Keep this control visible at every responsive width. Use Unicode
                // text instead of version-specific Font Awesome
                // glyphs. Some bundled FA builds do not contain the old
                // single-repeat glyph, which made the loop button disappear.
                useIconFont: false
                glyph: player.playMode === 2 ? "⇄" : (player.playMode === 1 ? "↻¹" : "↻")
                glyphSize: player.playMode === 1 ? 13 : 16
                glyphColor: player.playMode === 0 ? root.secondaryText : app.accentColor
                checked: player.playMode !== 0
                onClicked: player.cyclePlayMode()
            }
        }
        Item { Layout.fillWidth: true }

        RowLayout {
            Layout.preferredWidth: root.tiny ? 44 : (root.compact ? 86 : 372); spacing: 3
            PlayerIconButton {
                id: sourceButton
                visible: !root.compact && !root.tiny; width: 42; height: 36; iconFont: root.iconFont
                useIconFont: false
                glyph: app.playbackSourcePreference === "netease" ? "网易"
                       : (app.playbackSourcePreference === "kugou" ? "酷狗"
                          : (app.playbackSourcePreference === "kuwo" ? "酷我" : "音源"))
                glyphSize: 8
                glyphColor: app.playbackSourcePreference.length > 0 ? app.accentColor : root.secondaryText
                checked: app.playbackSourcePreference.length > 0
                onClicked: root.openChoicePopup(sourcePopup, sourceButton)
            }
            PlayerIconButton {
                id: qualityButton
                visible: !root.compact && !root.tiny; width: 42; height: 36; iconFont: root.iconFont
                useIconFont: false; glyph: app.qualityLevel === "lossless" ? "无损" : (app.qualityLevel === "exhigh" ? "极高" : (app.qualityLevel === "higher" ? "较高" : "标准"))
                glyphSize: 8; glyphColor: root.secondaryText; onClicked: root.openChoicePopup(qualityPopup, qualityButton)
            }
            PlayerIconButton {
                id: speedButton
                visible: !root.compact && !root.tiny; width: 38; height: 36; iconFont: root.iconFont
                useIconFont: false; glyph: Number(player.playbackRate).toFixed(player.playbackRate === 1 ? 0 : 1) + "×"
                glyphSize: 9; glyphColor: player.playbackRate === 1 ? root.secondaryText : app.accentColor; onClicked: root.openChoicePopup(speedPopup, speedButton)
            }
            PlayerIconButton {
                id: styleButton
                visible: !root.compact && !root.tiny; width: 38; height: 36; iconFont: root.iconFont
                useIconFont: false; glyph: "型"; glyphSize: 11; glyphColor: root.secondaryText; onClicked: root.openChoicePopup(stylePopup, styleButton)
            }
            PlayerIconButton {
                id: volumeButton; visible: !root.compact && !root.tiny; width: 38; height: 38; iconFont: root.iconFont
                glyph: player.volume <= 0.001 ? "\uf6a9" : (player.volume < 0.5 ? "\uf027" : "\uf028"); glyphSize: 13; glyphColor: root.secondaryText
                onClicked: volumePopup.open()
            }
            PlayerIconButton { visible: !root.compact && !root.tiny; width: 38; height: 38; iconFont: root.iconFont; glyph: "词"; useIconFont: false; glyphSize: 15; glyphColor: root.secondaryText; onClicked: root.openNowPlaying() }
            PlayerIconButton { visible: !root.compact && !root.tiny; width: 38; height: 38; iconFont: root.iconFont; glyph: "\uf0fe"; glyphSize: 12; glyphColor: root.secondaryText; onClicked: if (player.currentTrack.id) root.addToPlaylistRequested() }
            PlayerIconButton { width: 38; height: 38; iconFont: root.iconFont; glyph: "\uf03a"; glyphSize: 13; glyphColor: root.secondaryText; onClicked: root.queueRequested() }
        }
    }

    Item {
        id: progress; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 7
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.leftMargin: 7; anchors.rightMargin: 7; anchors.bottomMargin: 2; height: 2; radius: 1; color: immersive ? "#26FFFFFF" : (theme.isDark ? "#24FFFFFF" : "#16000000")
            Rectangle { width: parent.width * (player.duration > 0 ? Math.max(0, Math.min(1, player.position / player.duration)) : 0); height: parent.height; radius: 1; color: app.accentColor }
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onPressed: function(m){ if(player.duration>0) player.seek(Math.round(player.duration*Math.max(0,Math.min(1,m.x/width)))) }; onPositionChanged: function(m){ if(pressed&&player.duration>0) player.seek(Math.round(player.duration*Math.max(0,Math.min(1,m.x/width)))) } }
    }

    Popup {
        id: volumePopup; width: 58; height: 160; padding: 0; modal: false; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(8, root.width - 205); y: -height - 8
        background: Rectangle { radius: 20; color: theme.isDark ? "#F236362F" : "#FAFFFFFF"; border.color: theme.isDark ? "#18FFFFFF" : "#12000000" }
        contentItem: Item {
            Rectangle { width: 6; height: 104; radius: 3; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 18; color: theme.isDark ? "#55554D" : "#E3E4E7"
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: parent.height * player.volume; radius: 3; color: app.accentColor }
                Rectangle { width: 16; height: 16; radius: 8; anchors.horizontalCenter: parent.horizontalCenter; y: Math.max(-4, parent.height * (1-player.volume) - 8); color: "white"; border.color: "#18000000" }
                MouseArea { anchors.fill: parent; anchors.margins: -12; onPressed: function(m){ root.setVol(m.y) }; onPositionChanged: function(m){ if(pressed) root.setVol(m.y) } }
            }
            Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; text: Math.round(player.volume*100); color: theme.textColor; font.pixelSize: 9 }
        }
    }

    ChoicePopup {
        id: sourcePopup
        heading: "选择歌曲源渠道"
        selectedValue: app.playbackSourcePreference
        choices: app.playbackSourceChoices
        onChoiceSelected: function(value) { app.selectPlaybackSource(value) }
    }
    ChoicePopup {
        id: qualityPopup; heading: "选择音质"; selectedValue: app.qualityLevel
        choices: [
            { value:"standard", label:"标准", detail:"128 kbps，节省流量" },
            { value:"higher", label:"较高", detail:"192 kbps" },
            { value:"exhigh", label:"极高", detail:"320 kbps" },
            { value:"lossless", label:"无损", detail:"FLAC，音源支持时使用" }
        ]
        onChoiceSelected: function(value) { app.setQualityLevel(value) }
    }
    ChoicePopup {
        id: speedPopup; heading: "播放速度"; selectedValue: String(player.playbackRate)
        choices: [
            { value:"0.5", label:"0.5×", detail:"慢速" }, { value:"0.75", label:"0.75×", detail:"稍慢" },
            { value:"1", label:"1×", detail:"正常速度" }, { value:"1.25", label:"1.25×", detail:"稍快" },
            { value:"1.5", label:"1.5×", detail:"快速" }, { value:"2", label:"2×", detail:"最快" }
        ]
        onChoiceSelected: function(value) { player.setPlaybackRate(Number(value)) }
    }
    ChoicePopup {
        id: stylePopup; heading: "播放器样式"; selectedValue: root.playerStyle
        choices: [
            { value:"balanced", label:"平衡布局", detail:"信息与控制均衡" },
            { value:"centered", label:"居中胶囊", detail:"圆形封面与居中控制" },
            { value:"cover", label:"封面强调", detail:"更大的封面与品牌色播放键" }
        ]
        onChoiceSelected: function(value) { app.setPlayerStyle(value) }
    }

    function setVol(y) { var top=18; var h=104; player.setVolume(Math.max(0,Math.min(1,1-(y-top)/h))) }
    function openChoicePopup(popup, button) {
        var point = button.mapToItem(root, 0, 0)
        popup.x = Math.max(8, Math.min(root.width - popup.width - 8, point.x + button.width - popup.width))
        popup.y = -popup.height - 8
        popup.open()
    }
}
