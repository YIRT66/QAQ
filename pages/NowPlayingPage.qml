import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    property var theme
    property var iconFont
    property var window
    property int lyricIndex: -1
    signal backRequested()
    readonly property bool compactLayout: width < 860

    readonly property color pageColor: "#36352B"
    readonly property color primaryText: "#F5F3EA"
    readonly property color secondaryText: "#B8B6AB"
    readonly property color dimText: "#706F67"
    readonly property bool currentFavorite: {
        var snapshot = app.favorites
        return app.isFavorite(player.currentTrack.id || "")
    }

    function updateLyricIndex() {
        var list = app.lyrics
        if (!list || list.length === 0) { lyricIndex = -1; return }
        var pos = player.position + 120
        var idx = lyricIndex
        if (idx < 0 || idx >= list.length || list[idx].time > pos) {
            // Binary search only after seek/backwards movement.
            var lo = 0, hi = list.length - 1, found = -1
            while (lo <= hi) {
                var mid = (lo + hi) >> 1
                if (list[mid].time <= pos) { found = mid; lo = mid + 1 } else hi = mid - 1
            }
            idx = found
        } else {
            // Normal playback is monotonic: advance from the current row instead
            // of scanning the complete lyric list on every position tick.
            while (idx + 1 < list.length && list[idx + 1].time <= pos) idx++
        }
        if (idx !== lyricIndex) {
            lyricIndex = idx
            if (idx >= 0 && lyricList.count > 0) lyricList.positionViewAtIndex(idx, ListView.Center)
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

    onVisibleChanged: if (visible) updateLyricIndex()

    Rectangle {
        anchors.fill: parent
        color: root.pageColor

        // Ambient shapes: intentionally subtle so the lyrics stay readable.
        Rectangle {
            width: parent.width * 0.58
            height: width
            radius: width / 2
            x: -width * 0.35
            y: -height * 0.55
            color: "#0BFFFFFF"
        }
        Rectangle {
            width: parent.width * 0.48
            height: width
            radius: width / 2
            x: parent.width - width * 0.52
            y: parent.height - height * 0.32
            color: "#07000000"
        }

        RowLayout {
            id: windowBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 24
            anchors.rightMargin: 18
            height: 68
            spacing: 8
            z: 20

            AppC.PlayerIconButton {
                width: 42; height: 42
                iconFont: root.iconFont
                glyph: "\uf078"
                glyphColor: "#BEB2AF"
                glyphSize: 14
                onClicked: root.backRequested()
            }
            Item { Layout.fillWidth: true }
            AppC.PlayerIconButton {
                width: 38; height: 38
                iconFont: root.iconFont
                glyph: "\uf2d1"
                glyphColor: "#C7BCB9"
                glyphSize: 13
                onClicked: root.window.showMinimized()
            }
            AppC.PlayerIconButton {
                width: 38; height: 38
                iconFont: root.iconFont
                glyph: "\uf2d0"
                glyphColor: "#C7BCB9"
                glyphSize: 13
                onClicked: root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized()
            }
            AppC.PlayerIconButton {
                width: 38; height: 38
                iconFont: root.iconFont
                glyph: "\uf00d"
                glyphColor: "#C7BCB9"
                glyphSize: 14
                hoverColor: "#52C84C4C"
                onClicked: root.window.close()
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: windowBar.height
            z: 2
            onPressed: root.window.startSystemMove()
            onDoubleClicked: root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized()
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: windowBar.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: root.compactLayout ? 20 : Math.max(56, parent.width * 0.055)
            anchors.rightMargin: root.compactLayout ? 20 : Math.max(52, parent.width * 0.05)
            anchors.topMargin: 12
            anchors.bottomMargin: 18
            spacing: root.compactLayout ? 24 : Math.max(42, parent.width * 0.045)

            ColumnLayout {
                Layout.preferredWidth: root.compactLayout ? Math.min(260, root.width * 0.38) : Math.min(460, root.width * 0.39)
                Layout.fillHeight: true
                spacing: 20

                Item { Layout.fillHeight: true }

                AppC.CoverImage {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: root.compactLayout ? Math.min(230, root.width * 0.34, root.height * 0.43) : Math.min(420, root.width * 0.34, root.height * 0.52)
                    Layout.preferredHeight: Layout.preferredWidth
                    source: player.currentTrack.cover || ""
                    requestedSize: root.compactLayout ? 420 : 760
                    cornerRadius: 16
                    placeholderColor: "#4A493D"
                    glyphColor: "#8F8D83"
                }

                Item { Layout.fillHeight: true }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: Math.max(38, root.height * 0.055)
                    anchors.bottomMargin: Math.max(22, root.height * 0.035)
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            Layout.fillWidth: true
                            text: player.currentTrack.title || "正在播放"
                            color: root.primaryText
                            font.pixelSize: root.compactLayout ? 17 : 20
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: player.currentTrack.artist || ""
                            color: root.secondaryText
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    ListView {
                        id: lyricList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 7
                        model: app.lyrics
                        cacheBuffer: 900
                        preferredHighlightBegin: height * 0.40
                        preferredHighlightEnd: height * 0.58
                        highlightRangeMode: ListView.ApplyRange

                        topMargin: height * 0.30
                        bottomMargin: height * 0.34

                        delegate: Item {
                            required property var modelData
                            required property int index
                            width: lyricList.width
                            height: translationText.visible ? 64 : 48
                            property bool isCurrent: index === root.lyricIndex

                            Rectangle {
                                visible: parent.isCurrent
                                width: 3
                                height: Math.min(28, parent.height - 10)
                                radius: 2
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                color: app.accentColor
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: parent.isCurrent ? 14 : 0
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 5
                                Behavior on anchors.leftMargin { NumberAnimation { duration: 170 } }

                                Text {
                                    width: parent.width
                                    text: modelData.text || ""
                                    color: isCurrent ? root.primaryText : (index < root.lyricIndex - 3 || index > root.lyricIndex + 4 ? root.dimText : root.secondaryText)
                                    font.pixelSize: isCurrent ? (root.compactLayout ? 18 : 22) : (root.compactLayout ? 14 : 17)
                                    font.bold: isCurrent
                                    wrapMode: Text.Wrap
                                    opacity: isCurrent ? 1 : 0.82
                                    Behavior on font.pixelSize { NumberAnimation { duration: 170 } }
                                    Behavior on color { ColorAnimation { duration: 170 } }
                                    Behavior on opacity { NumberAnimation { duration: 170 } }
                                }
                                Text {
                                    id: translationText
                                    visible: (modelData.translation || "").length > 0
                                    width: parent.width
                                    text: modelData.translation || ""
                                    color: isCurrent ? "#BFB2AE" : "#766763"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Text {
                            visible: app.lyrics.length === 0
                            anchors.centerIn: parent
                            text: player.currentTrack.id ? "这首歌暂时没有歌词" : "播放一首歌后，这里会显示滚动歌词"
                            color: root.secondaryText
                            font.pixelSize: 15
                        }
                    }
                }
            }
        }
    }
}
