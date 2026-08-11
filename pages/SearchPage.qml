pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    property var theme
    property var iconFont
    property string query: ""
    property string lastIssuedQuery: ""
    property int category: 0
    signal artistRequested(var artist)

    function usableProviderCount() { var l=app.providers,n=0; for(var i=0;i<l.length;i++) if(l[i].usable)n++; return n }
    function variantMode() { var q=input.text.toLowerCase(); if(/(?:dj|remix|混音)\s*$/.test(q))return "dj"; if(/(?:翻唱|cover)\s*$/.test(q))return "cover"; if(/(?:伴奏|instrumental|ktv)\s*$/.test(q))return "instrumental"; return "original" }
    function baseQuery() { return input.text.trim().replace(/\s+(DJ|Remix|混音|翻唱|Cover|伴奏|Instrumental|KTV)$/i,"").trim() }
    function setVariant(mode) { var b=baseQuery(); var suffix=mode==="dj"?" DJ":(mode==="cover"?" 翻唱":(mode==="instrumental"?" 伴奏":"")); input.text=(b+suffix).trim(); root.query=input.text; debounce.stop(); root.issue(true) }
    function issue(force) { var q=input.text.trim(); root.query=q; if(!q.length)return; if(!force&&q===root.lastIssuedQuery)return; root.lastIssuedQuery=q; app.search(q) }
    function openPlaylist(row) { var id=String((row&&(row.sourceId||row.id))||""); if(id.length)app.openPlaylist(id,(row&&row.providerId)||"netease") }
    Timer { id: debounce; interval: 280; repeat: false; onTriggered: root.issue(false) }

    component CategoryChip: Rectangle {
        property int value: 0
        property string label: ""
        height: 30; width: labelText.implicitWidth + 24; radius: 15
        color: root.category === value ? app.accentColor : (chipMouse.containsMouse ? (theme.isDark ? "#383831" : "#E1E3E7") : (theme.isDark ? "#2B2B25" : "#ECEEF1"))
        Text { id: labelText; anchors.centerIn: parent; text: label; color: root.category === value ? "#14201D" : theme.textColor; font.pixelSize: 9; font.bold: root.category === value }
        MouseArea { id: chipMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.category = value }
    }

    component PlaylistGrid: GridView {
        clip: true; cellWidth: Math.max(190, width / Math.max(1, Math.floor(width / 220))); cellHeight: 250
        delegate: Item {
            required property var modelData
            width: GridView.view.cellWidth; height: GridView.view.cellHeight
            Rectangle {
                anchors.fill: parent; anchors.margins: 6; radius: 15
                color: playlistMouse.containsMouse ? (theme.isDark ? "#33332C" : "#FFFFFF") : (theme.isDark ? "#292923" : "#F7F8FA")
                AppC.CoverImage { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; height: parent.width - 16; source: modelData.cover || ""; requestedSize: 360; cornerRadius: 11; placeholderColor: theme.secondaryColor }
                Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: owner.top; anchors.leftMargin: 10; anchors.rightMargin: 10; text: modelData.name || "歌单"; color: theme.textColor; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
                Text { id: owner; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; text: (modelData.ownerName || "网易云音乐") + " · " + (modelData.trackCount || 0) + " 首"; color: "#8B8B93"; font.pixelSize: 8; elide: Text.ElideRight }
                MouseArea { id: playlistMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.openPlaylist(modelData) }
            }
        }
    }

    component ArtistGrid: GridView {
        clip: true; cellWidth: Math.max(180, width / Math.max(1, Math.floor(width / 210))); cellHeight: 230
        delegate: Item {
            required property var modelData
            width: GridView.view.cellWidth; height: GridView.view.cellHeight
            Column {
                anchors.fill: parent; anchors.margins: 8; spacing: 8
                AppC.CoverImage { width: Math.min(parent.width, 168); height: width; anchors.horizontalCenter: parent.horizontalCenter; source: modelData.cover || ""; requestedSize: 360; cornerRadius: width/2; placeholderColor: theme.secondaryColor }
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: modelData.name || "歌手"; color: theme.textColor; font.pixelSize: 11; font.bold: true; elide: Text.ElideRight }
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: (modelData.trackCount || 0) + " 首歌曲"; color: "#8B8B93"; font.pixelSize: 8 }
            }
            MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.artistRequested(modelData) }
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 10
        RowLayout { Layout.fillWidth: true; Layout.preferredHeight: 40
            Text { text: root.query.length ? "搜索 “"+root.query+"”" : "搜索音乐"; color: theme.textColor; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: app.searchResults.length + " 首 · " + app.searchPlaylists.length + " 个歌单 · " + app.searchArtists.length + " 位歌手"; color: "#8C8C93"; font.pixelSize: 9 }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 42; radius: 21; color: theme.isDark?"#2D2D26":"#E7E8EC"; border.color: input.activeFocus?app.accentColor:"transparent"
            RowLayout { anchors.fill: parent; anchors.leftMargin:14; anchors.rightMargin:8; spacing:8
                Text { text:"\uf002"; font.family:iconFont.name; color:"#88888F"; font.pixelSize:10 }
                TextInput { id:input; Layout.fillWidth:true; text:root.query; color:theme.textColor; font.pixelSize:11; selectionColor:app.accentColor; onTextEdited:{root.query=text.trim();debounce.restart()} onAccepted:{debounce.stop();root.issue(true)} Text{visible:!input.text.length;text:"单曲 / 歌单 / 歌手";color:"#909097";font.pixelSize:10} }
                Rectangle { width:78;height:30;radius:15;color:app.accentColor; Text{anchors.centerIn:parent;text:app.loading?"搜索中":"搜索";color:"#17211F";font.pixelSize:9;font.bold:true} MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:{debounce.stop();root.issue(true)}} }
            }
        }
        RowLayout { Layout.fillWidth: true; Layout.preferredHeight: 32; spacing: 7
            CategoryChip { value:0; label:"综合" }
            CategoryChip { value:1; label:"单曲 " + app.searchResults.length }
            CategoryChip { value:2; label:"歌单 " + app.searchPlaylists.length }
            CategoryChip { value:3; label:"歌手 " + app.searchArtists.length }
            Item { Layout.fillWidth: true }
            Repeater { model:[{key:"original",label:"原唱"},{key:"dj",label:"DJ / Remix"},{key:"cover",label:"翻唱"},{key:"instrumental",label:"伴奏"}]
                delegate: Rectangle { required property var modelData; readonly property bool active:root.variantMode()===modelData.key; height:26;width:vtext.implicitWidth+18;radius:13;color:active?"#2213D9B0":(theme.isDark?"#292923":"#ECEEF1");border.color:active?app.accentColor:"transparent"; Text{id:vtext;anchors.centerIn:parent;text:modelData.label;color:active?app.accentColor:theme.textColor;font.pixelSize:8} MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:root.setVariant(modelData.key)} }
            }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: root.category
            ListView {
                id: comprehensiveResults
                clip: true; spacing: 1; model: app.searchResults; reuseItems: true; cacheBuffer: 1200
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                header: Item {
                    width: comprehensiveResults.width
                    height: (app.searchArtists.length > 0 || app.searchPlaylists.length > 0) ? 162 : 38
                    RowLayout {
                        visible: parent.height > 40
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        height: 124; spacing: 12
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 6
                            RowLayout { Layout.fillWidth: true
                                Text { text: "歌手"; color: theme.textColor; font.pixelSize: 11; font.bold: true }
                                Text { text: app.searchArtists.length + " 位"; color: "#85858D"; font.pixelSize: 8 }
                                Item { Layout.fillWidth: true }
                                Text { text: "查看全部 ›"; color: app.accentColor; font.pixelSize: 8; MouseArea { anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor; onClicked: root.category = 3 } }
                            }
                            ListView {
                                Layout.fillWidth: true; Layout.fillHeight: true; orientation: ListView.Horizontal; spacing: 7; clip: true
                                model: app.searchArtists
                                delegate: Rectangle {
                                    required property var modelData
                                    width: 152; height: 88; radius: 12
                                    color: artistSummaryMouse.containsMouse ? (theme.isDark ? "#35352E" : "#ECEEF2") : (theme.isDark ? "#292923" : "#F6F7F9")
                                    RowLayout { anchors.fill: parent; anchors.margins: 8; spacing: 8
                                        AppC.CoverImage { Layout.preferredWidth: 54; Layout.preferredHeight: 54; source: modelData.cover || ""; requestedSize: 160; cornerRadius: 27; placeholderColor: theme.secondaryColor }
                                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                                            Text { Layout.fillWidth: true; text: modelData.name || "歌手"; color: theme.textColor; font.pixelSize: 9; font.bold: true; elide: Text.ElideRight }
                                            Text { text: (modelData.trackCount || 0) + " 首"; color: "#85858D"; font.pixelSize: 7 }
                                        }
                                    }
                                    MouseArea { id: artistSummaryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.artistRequested(modelData) }
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 6
                            RowLayout { Layout.fillWidth: true
                                Text { text: "歌单"; color: theme.textColor; font.pixelSize: 11; font.bold: true }
                                Text { text: app.searchPlaylists.length + " 个"; color: "#85858D"; font.pixelSize: 8 }
                                Item { Layout.fillWidth: true }
                                Text { text: "查看全部 ›"; color: app.accentColor; font.pixelSize: 8; MouseArea { anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor; onClicked: root.category = 2 } }
                            }
                            ListView {
                                Layout.fillWidth: true; Layout.fillHeight: true; orientation: ListView.Horizontal; spacing: 7; clip: true
                                model: app.searchPlaylists
                                delegate: Rectangle {
                                    required property var modelData
                                    width: 188; height: 88; radius: 12
                                    color: playlistSummaryMouse.containsMouse ? (theme.isDark ? "#35352E" : "#ECEEF2") : (theme.isDark ? "#292923" : "#F6F7F9")
                                    RowLayout { anchors.fill: parent; anchors.margins: 8; spacing: 8
                                        AppC.CoverImage { Layout.preferredWidth: 58; Layout.preferredHeight: 58; source: modelData.cover || ""; requestedSize: 160; cornerRadius: 8; placeholderColor: theme.secondaryColor }
                                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                                            Text { Layout.fillWidth: true; text: modelData.name || "歌单"; color: theme.textColor; font.pixelSize: 9; font.bold: true; maximumLineCount: 2; wrapMode: Text.Wrap; elide: Text.ElideRight }
                                            Text { text: (modelData.trackCount || 0) + " 首"; color: "#85858D"; font.pixelSize: 7 }
                                        }
                                    }
                                    MouseArea { id: playlistSummaryMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.openPlaylist(modelData) }
                                }
                            }
                        }
                    }
                    RowLayout { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 32
                        Text { text: "单曲"; color: theme.textColor; font.pixelSize: 11; font.bold: true }
                        Text { text: app.searchResults.length + " 首"; color: "#85858D"; font.pixelSize: 8 }
                        Item { Layout.fillWidth: true }
                    }
                }
                delegate: AppC.TrackRow {
                    required property var modelData
                    required property int index
                    width: ListView.view.width; theme: root.theme; iconFont: root.iconFont; track: modelData; context: "search"
                    onPlayRequested: function(t,c) { app.playTrack(t,c) }
                }
            }
            ListView { clip:true;spacing:1;model:app.searchResults;reuseItems:true;cacheBuffer:1000;delegate:AppC.TrackRow{required property var modelData;width:ListView.view.width;theme:root.theme;iconFont:root.iconFont;track:modelData;context:"search";onPlayRequested:function(t,c){app.playTrack(t,c)}} }
            PlaylistGrid { model: app.searchPlaylists }
            ArtistGrid { model: app.searchArtists }
        }
        Text { visible: !app.loading && root.query.length > 0 && app.searchResults.length===0 && app.searchPlaylists.length===0 && app.searchArtists.length===0; Layout.alignment:Qt.AlignHCenter; text:"没有找到结果"; color:"#8B8B92"; font.pixelSize:12 }
    }
}
