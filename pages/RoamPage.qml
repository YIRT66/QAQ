import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root

    property var theme
    property var iconFont
    property int lyricIndex: -1
    readonly property bool compactLayout: width < 900 || height < 590
    readonly property color panelColor: theme.isDark ? "#30352C" : "#E8ECE6"
    readonly property color primaryText: theme.isDark ? "#F5F5EF" : "#22251F"
    readonly property color secondaryText: theme.isDark ? "#B6BDB1" : "#687064"
    readonly property color dimText: theme.isDark ? "#737A70" : "#9AA096"
    readonly property bool currentFavorite: {
        var snapshot = app.favorites
        return app.isFavorite(player.currentTrack.id || "")
    }

    function updateLyricIndex() {
        var list = app.lyrics
        if (!list || list.length === 0) {
            lyricIndex = -1
            return
        }

        var pos = player.position + 120
        var idx = lyricIndex
        if (idx < 0 || idx >= list.length || list[idx].time > pos) {
            var lo = 0
            var hi = list.length - 1
            var found = -1
            while (lo <= hi) {
                var mid = (lo + hi) >> 1
                if (list[mid].time <= pos) {
                    found = mid
                    lo = mid + 1
                } else {
                    hi = mid - 1
                }
            }
            idx = found
        } else {
            while (idx + 1 < list.length && list[idx + 1].time <= pos)
                idx++
        }

        if (idx !== lyricIndex) {
            lyricIndex = idx
            if (idx >= 0 && lyricList.count > 0)
                lyricList.positionViewAtIndex(idx, ListView.Center)
        }
    }

    Connections {
        target: player
        enabled: root.visible
        function onPositionChanged() { root.updateLyricIndex() }
        function onCurrentTrackChanged() {
            root.lyricIndex = -1
            lyricList.positionViewAtBeginning()
        }
    }

    onVisibleChanged: {
        if (visible) {
            app.startRoam(false)
            updateLyricIndex()
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: root.panelColor
        clip: true

        Rectangle {
            width: parent.width * 0.58
            height: width
            radius: width / 2
            x: -width * 0.42
            y: -height * 0.62
            color: theme.isDark ? "#08FFFFFF" : "#16FFFFFF"
        }

        Rectangle {
            width: parent.width * 0.42
            height: width
            radius: width / 2
            x: parent.width - width * 0.54
            y: parent.height - height * 0.30
            color: theme.isDark ? "#08000000" : "#08000000"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: root.compactLayout ? 20 : 34
            anchors.rightMargin: root.compactLayout ? 20 : 34
            anchors.topMargin: root.compactLayout ? 18 : 24
            anchors.bottomMargin: root.compactLayout ? 16 : 24
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.compactLayout ? 42 : 48
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    RowLayout {
                        spacing: 9
                        Text {
                            text: "推荐"
                            color: root.primaryText
                            font.pixelSize: root.compactLayout ? 19 : 22
                            font.bold: true
                        }
                        Rectangle {
                            height: 20
                            width: roamBadgeText.implicitWidth + 16
                            radius: 10
                            color: app.roamActive ? app.accentColor : (theme.isDark ? "#3E443A" : "#D9DDD6")
                            Text {
                                id: roamBadgeText
                                anchors.centerIn: parent
                                text: app.roamLoading ? "AI 正在选歌" : "AI 漫游"
                                color: app.roamActive ? "#10251F" : root.secondaryText
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: app.roamReason || "根据你的喜欢与最近播放持续推荐"
                        color: root.secondaryText
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    width: root.compactLayout ? 82 : 94
                    height: 32
                    radius: 16
                    color: refreshMouse.containsMouse
                           ? (theme.isDark ? "#2AFFFFFF" : "#16000000")
                           : (theme.isDark ? "#16FFFFFF" : "#0C000000")
                    Row {
                        anchors.centerIn: parent
                        spacing: 7
                        Text {
                            text: "\uf074"
                            font.family: root.iconFont.name
                            color: app.roamLoading ? app.accentColor : root.secondaryText
                            font.pixelSize: 10
                        }
                        Text {
                            text: app.roamLoading ? "选歌中" : "换一批"
                            color: root.primaryText
                            font.pixelSize: 10
                        }
                    }
                    MouseArea {
                        id: refreshMouse
                        anchors.fill: parent
                        enabled: !app.roamLoading
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: app.startRoam(true)
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: root.compactLayout ? 24 : 28
                spacing: 7
                Repeater {
                    model: [
                        "原唱优先",
                        "探索度 " + app.recommendationDiversity + "%",
                        "多源极速播放"
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        height: 24
                        width: pillText.implicitWidth + 18
                        radius: 12
                        color: theme.isDark ? "#14FFFFFF" : "#0A000000"
                        border.color: "#12FFFFFF"
                        Text { id:pillText; anchors.centerIn: parent; text:modelData; color:root.secondaryText; font.pixelSize:8 }
                    }
                }
                Item { Layout.fillWidth: true }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    anchors.fill: parent
                    spacing: root.compactLayout ? 24 : Math.max(48, root.width * 0.045)

                    ColumnLayout {
                        Layout.preferredWidth: root.compactLayout
                                               ? Math.min(270, root.width * 0.38)
                                               : Math.min(430, root.width * 0.39)
                        Layout.fillHeight: true
                        spacing: 14

                        Item { Layout.fillHeight: true }

                        AppC.CoverImage {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: root.compactLayout
                                                   ? Math.min(230, root.width * 0.34, root.height * 0.47)
                                                   : Math.min(390, root.width * 0.34, root.height * 0.56)
                            Layout.preferredHeight: Layout.preferredWidth
                            source: player.currentTrack.cover || ""
                            requestedSize: root.compactLayout ? 420 : 760
                            cornerRadius: 14
                            placeholderColor: theme.isDark ? "#444A40" : "#D4D9D1"
                            glyphColor: root.dimText
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !root.compactLayout
                            text: player.currentTrack.title || (app.roamLoading ? "正在为你挑选歌曲…" : "开始智能漫游")
                            color: root.primaryText
                            font.pixelSize: 15
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !root.compactLayout && (player.currentTrack.artist || "").length > 0
                            text: player.currentTrack.artist || ""
                            color: root.secondaryText
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 10

                            Rectangle {
                                width: 34; height: 34; radius: 17
                                color: favoriteMouse.containsMouse ? (theme.isDark ? "#26FFFFFF" : "#12000000") : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: root.currentFavorite ? "\uf004" : "\uf08a"
                                    font.family: root.iconFont.name
                                    font.pixelSize: 13
                                    color: root.currentFavorite ? app.accentColor : root.secondaryText
                                }
                                MouseArea {
                                    id: favoriteMouse
                                    anchors.fill: parent
                                    enabled: (player.currentTrack.id || "").length > 0
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: app.toggleFavorite(player.currentTrack)
                                }
                            }

                            Rectangle {
                                width: 82; height: 32; radius: 16
                                color: nextMouse.containsMouse ? (theme.isDark ? "#28FFFFFF" : "#12000000") : (theme.isDark ? "#18FFFFFF" : "#0B000000")
                                Row {
                                    anchors.centerIn: parent
                                    spacing: 7
                                    Text { text: "\uf051"; font.family: root.iconFont.name; font.pixelSize: 9; color: root.secondaryText }
                                    Text { text: "下一首"; font.pixelSize: 10; color: root.primaryText }
                                }
                                MouseArea {
                                    id: nextMouse
                                    anchors.fill: parent
                                    enabled: player.queue.length > 1
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: player.next()
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.topMargin: root.compactLayout ? 12 : 34
                            anchors.bottomMargin: root.compactLayout ? 8 : 22
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: player.currentTrack.title || "智能漫游"
                                    color: root.primaryText
                                    font.pixelSize: root.compactLayout ? 17 : 21
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: player.currentTrack.artist || "Evolve AI 正在从你的喜好里挑选下一首"
                                    color: root.secondaryText
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.compactLayout ? 38 : 44
                                radius: 12
                                visible: (app.roamReason || "").length > 0
                                color: theme.isDark ? "#10FFFFFF" : "#09000000"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8
                                    Rectangle { width:6; height:6; radius:3; color:app.accentColor }
                                    Text { text:"为什么推荐"; color:root.primaryText; font.pixelSize:9; font.bold:true }
                                    Text { Layout.fillWidth:true; text:app.roamReason || ""; color:root.secondaryText; font.pixelSize:9; elide:Text.ElideRight }
                                }
                                Behavior on opacity { NumberAnimation { duration: app.animationLevel === "rich" ? 180 : 0 } }
                            }

                            ListView {
                                id: lyricList
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 5
                                model: app.lyrics
                                cacheBuffer: 800
                                preferredHighlightBegin: height * 0.39
                                preferredHighlightEnd: height * 0.58
                                highlightRangeMode: ListView.ApplyRange
                                topMargin: height * 0.29
                                bottomMargin: height * 0.34

                                delegate: Item {
                                    required property var modelData
                                    required property int index
                                    width: lyricList.width
                                    height: translation.visible ? 56 : 42
                                    property bool currentLine: index === root.lyricIndex

                                    Rectangle {
                                        visible: parent.currentLine
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 3
                                        height: Math.min(26, parent.height - 8)
                                        radius: 2
                                        color: app.accentColor
                                    }

                                    Column {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.leftMargin: parent.currentLine ? 14 : 0
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 4

                                        Text {
                                            width: parent.width
                                            text: modelData.text || ""
                                            color: currentLine
                                                   ? root.primaryText
                                                   : (index < root.lyricIndex - 3 || index > root.lyricIndex + 4
                                                      ? root.dimText : root.secondaryText)
                                            font.pixelSize: currentLine ? (root.compactLayout ? 17 : 20) : (root.compactLayout ? 13 : 15)
                                            font.bold: currentLine
                                            wrapMode: Text.Wrap
                                            opacity: currentLine ? 1 : 0.84
                                        }

                                        Text {
                                            id: translation
                                            visible: (modelData.translation || "").length > 0
                                            width: parent.width
                                            text: modelData.translation || ""
                                            color: currentLine ? root.secondaryText : root.dimText
                                            font.pixelSize: 9
                                            elide: Text.ElideRight
                                        }
                                    }
                                }

                                Column {
                                    visible: app.lyrics.length === 0
                                    anchors.centerIn: parent
                                    spacing: 8

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: app.roamLoading && !(player.currentTrack.id || "")
                                              ? "正在为你挑选下一首"
                                              : "暂无歌词"
                                        color: root.primaryText
                                        font.pixelSize: 17
                                        font.bold: true
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: app.roamLoading ? "AI 漫游正在更新推荐队列" : "歌曲会自动连续播放，播完后继续推荐"
                                        color: root.secondaryText
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
