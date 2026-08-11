import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    property var theme
    property var iconFont
    property string selectedAvatar: ""
    property string selectedCover: ""
    property string editingPlaylistId: ""
    readonly property color cardColor: theme && theme.isDark ? "#292923" : "#FFFFFF"
    readonly property color softColor: theme && theme.isDark ? "#23231F" : "#F3F5F7"
    readonly property color mutedColor: theme && theme.isDark ? "#969790" : "#777B84"
    readonly property color borderColor: theme && theme.isDark ? "#17FFFFFF" : "#0F000000"

    function profileValue(key, fallback) {
        var p = app.userProfile || ({})
        return p[key] !== undefined && p[key] !== null ? p[key] : fallback
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 16
                AppC.CoverImage {
                    Layout.preferredWidth: 104; Layout.preferredHeight: 104
                    source: root.selectedAvatar.length ? root.selectedAvatar : root.profileValue("avatar", "")
                    requestedSize: 256; cornerRadius: 52
                    placeholderColor: theme.secondaryColor
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 5
                    Text { text: root.profileValue("displayName", root.profileValue("username", "Evolve 用户")); color: theme.textColor; font.pixelSize: 24; font.bold: true }
                    Text { text: "@" + root.profileValue("username", "user"); color: "#898991"; font.pixelSize: 10 }
                    Text {
                        Layout.fillWidth: true
                        text: root.profileValue("bio", "还没有个人简介")
                        color: root.mutedColor; font.pixelSize: 10; wrapMode: Text.Wrap
                    }
                    RowLayout {
                        spacing: 14
                        Text { text: root.profileValue("followers", 0) + " 关注者"; color: theme.textColor; font.pixelSize: 10 }
                        Text { text: root.profileValue("followingCount", 0) + " 正在关注"; color: theme.textColor; font.pixelSize: 10 }
                        Text { text: app.communityPlaylists.length + " 社区歌单"; color: theme.textColor; font.pixelSize: 10 }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: profileEdit.implicitHeight + 34; radius: 16
                color: root.cardColor; border.color: root.borderColor; border.width: 1
                ColumnLayout {
                    id: profileEdit; anchors.fill: parent; anchors.margins: 16; spacing: 10
                    Text { text: "编辑个人资料"; color: theme.textColor; font.pixelSize: 15; font.bold: true }
                    AppC.AppTextField {
                        theme: root.theme
                        id: displayName; Layout.fillWidth: true; placeholderText: "昵称"; text: root.profileValue("displayName", "")
                    }
                    AppC.AppTextArea {
                        theme: root.theme
                        id: bio; Layout.fillWidth: true; Layout.preferredHeight: 70; placeholderText: "个人简介"; text: root.profileValue("bio", ""); wrapMode: TextEdit.Wrap
                    }
                    RowLayout {
                        AppC.AppButton { theme: root.theme; variant: "secondary"; text: "选择头像"; onClicked: { var f = app.chooseImageFile(); if (f.length) root.selectedAvatar = f } }
                        AppC.AppButton { theme: root.theme; variant: "primary"; text: "保存资料"; onClicked: app.updateUserProfile(displayName.text, bio.text, root.selectedAvatar) }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 230; radius: 16
                color: root.cardColor; border.color: root.borderColor; border.width: 1
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 9
                    Text { text: "找朋友 / 关注"; color: theme.textColor; font.pixelSize: 15; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true
                        AppC.AppTextField {
                            theme: root.theme
                            id: userQuery; Layout.fillWidth: true; placeholderText: "搜索用户名或昵称"; onAccepted: app.searchUsers(text)
                        }
                        AppC.AppButton { theme: root.theme; variant: "primary"; text: "搜索"; onClicked: app.searchUsers(userQuery.text) }
                    }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5; model: app.userSearchResults
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 48; radius: 10
                            color: root.softColor
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 7; spacing: 9
                                AppC.CoverImage { Layout.preferredWidth: 34; Layout.preferredHeight: 34; source: modelData.avatar || ""; requestedSize: 96; cornerRadius: 17 }
                                ColumnLayout { Layout.fillWidth: true; spacing: 0
                                    Text { text: modelData.displayName || modelData.username || "用户"; color: theme.textColor; font.pixelSize: 10; font.bold: true }
                                    Text { text: "@" + (modelData.username || ""); color: root.mutedColor; font.pixelSize: 8 }
                                }
                                AppC.AppButton { theme: root.theme; variant: modelData.following ? "soft" : "primary"; text: modelData.following ? "已关注" : "关注"; onClicked: app.setFollowing(modelData.id || "", !modelData.following) }
                            }
                        }
                    }
                }
            }

            Text { text: "我的歌单 · 可发布到 Evolve 社区"; color: theme.textColor; font.pixelSize: 17; font.bold: true; Layout.topMargin: 4 }
            Repeater {
                model: app.customPlaylists
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredHeight: 76; radius: 14
                    color: root.cardColor; border.color: root.borderColor; border.width: 1
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 10
                        AppC.CoverImage {
                            Layout.preferredWidth: 54; Layout.preferredHeight: 54
                            source: modelData.cover || ((modelData.tracks && modelData.tracks.length) ? modelData.tracks[0].cover : "")
                            requestedSize: 160; cornerRadius: 9
                        }
                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                            Text { text: modelData.name || "未命名歌单"; color: theme.textColor; font.pixelSize: 11; font.bold: true }
                            Text { text: (modelData.trackCount || 0) + " 首 · " + (modelData.isPublic ? "已发布" : "仅自己可见"); color: root.mutedColor; font.pixelSize: 8 }
                        }
                        AppC.AppButton { theme: root.theme; variant: "secondary"
                            text: "编辑 / 发布"
                            onClicked: {
                                root.editingPlaylistId = modelData.id || ""
                                playlistName.text = modelData.name || ""
                                playlistDescription.text = modelData.description || ""
                                publicCheck.checked = modelData.isPublic === true
                                root.selectedCover = ""
                                playlistEditor.open()
                            }
                        }
                    }
                }
            }

            Text { text: "社区歌单"; color: theme.textColor; font.pixelSize: 17; font.bold: true; Layout.topMargin: 4 }
            Flow {
                Layout.fillWidth: true; spacing: 12
                Repeater {
                    model: app.communityPlaylists
                    delegate: Rectangle {
                        required property var modelData
                        width: 170; height: 218; radius: 14
                        color: root.cardColor
                        AppC.CoverImage { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; height: 154; source: modelData.cover || ""; requestedSize: 360; cornerRadius: 10 }
                        Text { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: owner.top; anchors.margins: 9; text: modelData.name || "歌单"; color: theme.textColor; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
                        Text { id: owner; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 9; text: "by " + (modelData.ownerName || modelData.ownerUsername || "Evolve 用户"); color: root.mutedColor; font.pixelSize: 8; elide: Text.ElideRight }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: app.openCommunityPlaylist(modelData.id || "") }
                    }
                }
            }
            Item { Layout.preferredHeight: 20 }
        }
    }

    Popup {
        id: playlistEditor
        parent: root
        modal: true; width: Math.min(460, root.width - 36); height: 390
        x: (root.width-width)/2; y: (root.height-height)/2; padding: 18
        background: Rectangle { radius: 18; color: root.cardColor; border.width: 1; border.color: root.borderColor }
        ColumnLayout {
            anchors.fill: parent; spacing: 10
            Text { text: "歌单资料与发布"; color: theme.textColor; font.pixelSize: 18; font.bold: true }
            AppC.AppTextField {
                theme: root.theme
                id: playlistName; Layout.fillWidth: true; placeholderText: "歌单标题"
            }
            AppC.AppTextArea {
                theme: root.theme
                id: playlistDescription; Layout.fillWidth: true; Layout.preferredHeight: 92; placeholderText: "歌单简介"; wrapMode: TextEdit.Wrap
            }
            RowLayout { Layout.fillWidth: true
                AppC.AppButton { theme: root.theme; variant: "secondary"; text: "选择封面"; onClicked: { var f = app.chooseImageFile(); if (f.length) root.selectedCover = f } }
                Text { Layout.fillWidth: true; text: root.selectedCover.length ? "已选择新封面" : "保留现有封面"; color: root.mutedColor; font.pixelSize: 9 }
            }
            AppC.AppCheckBox { id: publicCheck; theme: root.theme; text: "发布到 Evolve 社区，其他用户可以查看和收藏" }
            Item { Layout.fillHeight: true }
            RowLayout { Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                AppC.AppButton { theme: root.theme; variant: "ghost"; text: "取消"; onClicked: playlistEditor.close() }
                AppC.AppButton { theme: root.theme; variant: "primary"; text: "保存"; onClicked: { app.updateCustomPlaylistMetadata(root.editingPlaylistId, playlistName.text, playlistDescription.text, root.selectedCover, publicCheck.checked); playlistEditor.close() } }
            }
        }
    }

    Component.onCompleted: { app.loadUserProfile(); app.loadCommunityPlaylists() }
    onVisibleChanged: if (visible) { app.loadUserProfile(); app.loadCommunityPlaylists() }
}
