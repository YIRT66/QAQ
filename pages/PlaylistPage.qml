import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../components" as AppC

Item {
    id: root

    property var theme
    property var iconFont
    property string selectedCoverFile: ""
    readonly property bool canEditPlaylist: app.currentPlaylist.editable === true
        || (app.currentPlaylist.custom === true
            && (!(app.currentPlaylist.ownerId || "").length || app.currentPlaylist.ownerId === app.cloudUserId))

    opacity: visible ? 1 : 0
    Behavior on opacity {
        NumberAnimation { duration: app.animationsEnabled ? 160 : 0 }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: root.height < 620 ? 8 : 12

        // Header must keep only its implicit/preferred height.
        // The old layout contained top/bottom Layout.fillHeight spacers here,
        // which consumed the entire page and left the song ListView at height 0.
        GridLayout {
            id: header
            Layout.fillWidth: true
            Layout.fillHeight: false
            columns: root.width < 720 ? 1 : 2
            columnSpacing: root.width < 900 ? 16 : 22
            rowSpacing: 10

            AppC.CoverImage {
                Layout.preferredWidth: root.width < 720 ? 128 : (root.height < 620 ? 148 : 176)
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: root.width < 720 ? Qt.AlignHCenter : Qt.AlignLeft | Qt.AlignVCenter

                source: app.currentPlaylist.cover || ""
                requestedSize: 420
                cornerRadius: 11
                placeholderColor: theme.secondaryColor
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.alignment: root.width < 720
                                  ? Qt.AlignHCenter
                                  : Qt.AlignLeft | Qt.AlignVCenter
                spacing: root.height < 620 ? 6 : 8

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: root.width < 720 ? Text.AlignHCenter : Text.AlignLeft
                    text: app.currentPlaylist.name || "歌单"
                    color: theme.textColor
                    font.pixelSize: root.width < 720 ? 20 : (root.height < 620 ? 24 : 28)
                    font.bold: true
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.alignment: root.width < 720 ? Qt.AlignHCenter : Qt.AlignLeft
                    spacing: 7

                    Rectangle {
                        width: 24
                        height: 24
                        radius: 12
                        color: app.accentColor

                        AppC.CoverImage { anchors.fill: parent; source: app.currentPlaylist.ownerAvatar || ""; requestedSize: 72; cornerRadius: 12; placeholderColor: app.accentColor }
                    }

                    Text {
                        text: app.currentPlaylist.ownerName || app.currentPlaylist.ownerUsername || app.currentPlaylist.providerName || "智能多源"
                        color: "#77777F"
                        font.pixelSize: 10
                    }

                    Text {
                        text: "·  " + app.playlistTracks.length + " 首"
                        color: "#9A9AA0"
                        font.pixelSize: 10
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: (app.currentPlaylist.description || "").length > 0
                    horizontalAlignment: root.width < 720 ? Text.AlignHCenter : Text.AlignLeft
                    text: app.currentPlaylist.description || ""
                    color: "#8D8D94"
                    font.pixelSize: 10
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.alignment: root.width < 720 ? Qt.AlignHCenter : Qt.AlignLeft
                    Layout.topMargin: 4
                    spacing: 8

                    Rectangle {
                        width: 116
                        height: 38
                        radius: 19
                        color: playMouse.containsMouse
                               ? Qt.darker(app.accentColor, 1.05)
                               : app.accentColor

                        Behavior on color {
                            ColorAnimation { duration: app.animationsEnabled ? 100 : 0 }
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                text: "\uf04b"
                                font.family: iconFont.name
                                color: "#17211F"
                                font.pixelSize: 10
                            }

                            Text {
                                text: "播放全部"
                                color: "#17211F"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        MouseArea {
                            id: playMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (app.playlistTracks.length > 0)
                                    app.playTrack(app.playlistTracks[0], "playlist")
                            }
                        }
                    }

                    AppC.PlayerIconButton {
                        width: 38
                        height: 38
                        iconFont: root.iconFont
                        glyph: "\uf019"
                        glyphColor: "#77777F"
                        normalColor: theme.isDark ? "#303028" : "#E4E5E9"
                        onClicked: {
                            if (app.playlistTracks.length > 0)
                                app.downloadTrack(app.playlistTracks[0])
                        }
                    }

                    Rectangle {
                        visible: root.canEditPlaylist
                        width: 82; height: 38; radius: 19
                        color: editMouse.containsMouse ? (theme.isDark ? "#3A3A33" : "#DFE1E5") : (theme.isDark ? "#303028" : "#E8EAED")
                        Text { anchors.centerIn: parent; text: "编辑歌单"; color: theme.textColor; font.pixelSize: 9; font.bold: true }
                        MouseArea { id:editMouse; anchors.fill:parent; hoverEnabled:true; cursorShape:Qt.PointingHandCursor; onClicked:{ editName.text=app.currentPlaylist.name||""; editDescription.text=app.currentPlaylist.description||""; publicCheck.checked=app.currentPlaylist.isPublic===true; root.selectedCoverFile=""; editPopup.open() } }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: theme.isDark ? "#14FFFFFF" : "#0E000000"
        }

        // This item is the only vertical-expanding child in the page.
        // Therefore it always receives the remaining height.
        Item {
            id: listArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120

            ListView {
                id: list
                anchors.fill: parent
                spacing: 1
                clip: true
                model: app.playlistTracks
                reuseItems: true
                cacheBuffer: 900
                visible: count > 0

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                delegate: AppC.TrackRow {
                    required property var modelData
                    required property int index

                    width: ListView.view.width
                    theme: root.theme
                    iconFont: root.iconFont
                    track: modelData
                    context: "playlist"
                    removable: root.canEditPlaylist

                    onPlayRequested: function(t, c) {
                        app.playTrack(t, c)
                    }
                    onRemoveRequested: function(t) {
                        app.removeTrackFromCustomPlaylist(app.currentPlaylist.id || "", t.id || "")
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 10
                visible: list.count === 0
                opacity: visible ? 1 : 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: app.animationsEnabled ? 150 : 0
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: app.loading
                          ? "正在加载歌曲…"
                          : (root.canEditPlaylist ? "这个歌单还是空的" : "歌曲列表暂时没有加载出来")
                    color: theme.textColor
                    font.pixelSize: 13
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: app.loading
                          ? "正在从歌单来源读取歌曲详情"
                          : (root.canEditPlaylist ? "在底部播放器点击 + 就能把当前歌曲加入歌单" : "可以重新请求歌单，不会影响当前正在播放的歌曲")
                    color: "#92929A"
                    font.pixelSize: 10
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 96
                    height: 32
                    radius: 16
                    color: retryMouse.containsMouse
                           ? Qt.darker(app.accentColor, 1.04)
                           : app.accentColor
                    visible: !app.loading && !root.canEditPlaylist

                    Text {
                        anchors.centerIn: parent
                        text: "重新加载"
                        color: "#17211F"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        id: retryMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var sourceId = app.currentPlaylist.sourceId || ""
                            var providerId = app.currentPlaylist.providerId || "netease"
                            if (providerId === "evolve")
                                app.openCommunityPlaylist(app.currentPlaylist.id || "")
                            else if (sourceId.length > 0)
                                app.openPlaylist(sourceId, providerId)
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: editPopup
        parent: root
        modal: true; width: Math.min(450, root.width - 30); height: 370
        x:(root.width-width)/2; y:(root.height-height)/2; padding:18
        background: Rectangle { radius:18; color:theme.isDark?"#2B2B25":"#FFFFFF"; border.color:theme.isDark?"#20FFFFFF":"#10000000" }
        ColumnLayout {
            anchors.fill:parent; spacing:10
            Text { text:"编辑自建歌单"; color:theme.textColor; font.pixelSize:18; font.bold:true }
            AppC.AppTextField {
                theme: root.theme
                id:editName; Layout.fillWidth:true; placeholderText:"歌单标题"
            }
            AppC.AppTextArea {
                theme: root.theme
                id:editDescription; Layout.fillWidth:true; Layout.preferredHeight:92; placeholderText:"歌单简介"; wrapMode:TextEdit.Wrap
            }
            RowLayout { Layout.fillWidth:true
                AppC.AppButton { theme: root.theme; variant:"secondary"; text:"上传封面"; onClicked:{ var f=app.chooseImageFile(); if(f.length) root.selectedCoverFile=f } }
                Text { Layout.fillWidth:true; text:root.selectedCoverFile.length?"已选择新封面":"可保留当前封面"; color:"#8C8C94"; font.pixelSize:8 }
            }
            AppC.AppCheckBox { id:publicCheck; theme:root.theme; text:"公开发布到 Evolve 社区" }
            Item { Layout.fillHeight:true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppC.AppButton { theme: root.theme; variant:"ghost"
                    text: "取消"
                    onClicked: editPopup.close()
                }
                AppC.AppButton { theme: root.theme; variant:"primary"
                    text: "保存"
                    onClicked: {
                        app.updateCustomPlaylistMetadata(
                            app.currentPlaylist.id || "",
                            editName.text,
                            editDescription.text,
                            root.selectedCoverFile,
                            publicCheck.checked
                        )
                        editPopup.close()
                    }
                }
            }
        }
    }
}
