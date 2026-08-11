import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    property var theme
    property var iconFont
    readonly property color cardColor: theme && theme.isDark ? "#292923" : "#FFFFFF"
    readonly property color softColor: theme && theme.isDark ? "#23231F" : "#F3F5F7"
    readonly property color mutedColor: theme && theme.isDark ? "#969790" : "#777B84"
    readonly property color borderColor: theme && theme.isDark ? "#17FFFFFF" : "#0F000000"
    readonly property bool inRoom: roomValue("code", "").length > 0

    function roomValue(key, fallback) {
        var r = app.togetherRoom || ({})
        return r[key] !== undefined && r[key] !== null ? r[key] : fallback
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "一起听"; color: theme.textColor; font.pixelSize: 24; font.bold: true }
                Text {
                    text: "同步听歌 · 临时房间聊天 · 图片/文件 · 实时语音"
                    color: root.mutedColor; font.pixelSize: 9
                }
            }
            Rectangle {
                width: 108; height: 30; radius: 15
                color: root.inRoom ? app.accentColor : root.softColor
                Text {
                    anchors.centerIn: parent
                    text: root.roomValue("code", "未加入房间")
                    color: root.inRoom ? "#17211F" : root.mutedColor
                    font.pixelSize: 10; font.bold: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 128
            radius: 18
            color: root.cardColor
            border.color: root.borderColor
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 14

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 7
                    Text {
                        text: root.inRoom ? "房间 " + root.roomValue("code", "") : "创建或加入一个房间"
                        color: theme.textColor; font.pixelSize: 17; font.bold: true
                    }
                    Text {
                        text: root.roomValue("isHost", false)
                              ? "你是房主 · 关闭/解散房间时聊天、语音缓存与临时附件会立即清理"
                              : (root.inRoom
                                 ? "正在跟随房主播放 · 房间结束后临时聊天内容不会保留"
                                 : "创建后把 6 位房间码发给好友，或直接邀请已关注用户")
                        color: root.mutedColor; font.pixelSize: 9
                    }
                    RowLayout {
                        AppC.AppButton {
                            theme: root.theme; variant: "primary"; text: "创建房间"
                            enabled: !root.inRoom
                            onClicked: app.createTogetherRoom()
                        }
                        AppC.AppTextField {
                            id: joinCode
                            theme: root.theme
                            Layout.preferredWidth: 155
                            placeholderText: "输入房间码"
                            maximumLength: 8
                            enabled: !root.inRoom
                        }
                        AppC.AppButton {
                            theme: root.theme; variant: "secondary"; text: "加入"
                            enabled: joinCode.text.trim().length >= 4 && !root.inRoom
                            onClicked: app.joinTogetherRoom(joinCode.text)
                        }
                        AppC.AppButton {
                            theme: root.theme; variant: "danger"; text: root.roomValue("isHost", false) ? "解散" : "离开"
                            visible: root.inRoom
                            onClicked: app.leaveTogetherRoom()
                        }
                    }
                }

                AppC.CoverImage {
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 96
                    source: player.currentTrack.cover || ""
                    requestedSize: 240
                    cornerRadius: 14
                    placeholderColor: theme.secondaryColor
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 500
                radius: 16
                color: root.cardColor
                border.width: 1
                border.color: root.borderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 9

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text { text: "房间聊天"; color: theme.textColor; font.pixelSize: 15; font.bold: true }
                            Text {
                                text: root.inRoom ? "只对当前房间成员可见 · 房间结束自动销毁" : "加入房间后可以聊天"
                                color: root.mutedColor; font.pixelSize: 8
                            }
                        }
                        Rectangle {
                            visible: app.togetherMicActive
                            width: 72; height: 5; radius: 3
                            color: root.softColor
                            Rectangle {
                                width: parent.width * Math.max(0.06, app.togetherMicLevel)
                                height: parent.height; radius: parent.radius
                                color: app.accentColor
                            }
                        }
                        AppC.AppButton {
                            theme: root.theme
                            variant: app.togetherMicActive ? "danger" : "secondary"
                            text: app.togetherMicActive ? "关闭麦克风" : "开麦说话"
                            enabled: root.inRoom
                            onClicked: app.setTogetherMicrophone(!app.togetherMicActive)
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: app.togetherVoiceError.length > 0
                        text: app.togetherVoiceError
                        color: "#E05B62"
                        font.pixelSize: 8
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 12
                        color: root.softColor

                        Text {
                            anchors.centerIn: parent
                            visible: !root.inRoom || app.togetherMessages.length === 0
                            text: root.inRoom ? "还没有消息，发一句开始聊天吧" : "加入房间后聊天、图片、文件和语音会出现在这里"
                            color: root.mutedColor; font.pixelSize: 9
                        }

                        ListView {
                            id: chatList
                            anchors.fill: parent
                            anchors.margins: 10
                            model: app.togetherMessages
                            spacing: 4
                            clip: true
                            onCountChanged: if (count > 0) positionViewAtEnd()

                            delegate: Item {
                                required property var modelData
                                width: ListView.view.width
                                height: bubble.height + 8

                                Rectangle {
                                    id: bubble
                                    width: Math.min(parent.width * 0.78, Math.max(190, bubbleContent.implicitWidth + 24))
                                    height: bubbleContent.implicitHeight + 18
                                    x: modelData.mine ? parent.width - width : 0
                                    radius: 12
                                    color: modelData.mine
                                           ? (theme.isDark ? "#173F36" : "#D9F7EF")
                                           : (theme.isDark ? "#30302A" : "#FFFFFF")
                                    border.width: modelData.mine ? 0 : 1
                                    border.color: root.borderColor

                                    ColumnLayout {
                                        id: bubbleContent
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 10
                                        spacing: 5

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.mine ? "我" : (modelData.senderName || modelData.senderUsername || "房间成员")
                                            color: modelData.mine ? app.accentColor : root.mutedColor
                                            font.pixelSize: 8
                                            font.bold: true
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            visible: modelData.kind === "text"
                                            text: modelData.text || ""
                                            color: theme.textColor
                                            font.pixelSize: 10
                                            wrapMode: Text.Wrap
                                            textFormat: Text.PlainText
                                        }

                                        Item {
                                            Layout.preferredWidth: 250
                                            Layout.preferredHeight: 160
                                            visible: modelData.kind === "image"
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 10
                                                color: root.cardColor
                                                border.width: 1
                                                border.color: root.borderColor
                                            }
                                            Image {
                                                anchors.fill: parent
                                                anchors.margins: 4
                                                source: app.togetherAttachmentUrls[modelData.attachmentId] || ""
                                                fillMode: Image.PreserveAspectFit
                                                asynchronous: true
                                                smooth: true
                                            }
                                            Text {
                                                anchors.centerIn: parent
                                                visible: !(app.togetherAttachmentUrls[modelData.attachmentId] || "").length
                                                text: "正在加载图片…"
                                                color: root.mutedColor; font.pixelSize: 8
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: app.openTogetherAttachment(modelData.attachmentId || "", modelData.fileName || "image.jpg")
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 50
                                            visible: modelData.kind === "file"
                                            radius: 9
                                            color: root.cardColor
                                            border.width: 1
                                            border.color: root.borderColor
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                spacing: 8
                                                Rectangle {
                                                    width: 32; height: 32; radius: 8
                                                    color: theme.isDark ? "#373731" : "#ECEFF3"
                                                    Text { anchors.centerIn: parent; text: "F"; color: app.accentColor; font.bold: true }
                                                }
                                                ColumnLayout {
                                                    Layout.fillWidth: true; spacing: 1
                                                    Text {
                                                        Layout.fillWidth: true
                                                        text: modelData.fileName || "文件"
                                                        color: theme.textColor; font.pixelSize: 9; font.bold: true
                                                        elide: Text.ElideMiddle
                                                    }
                                                    Text { text: app.formatFileSize(modelData.size || 0); color: root.mutedColor; font.pixelSize: 8 }
                                                }
                                                AppC.AppButton {
                                                    theme: root.theme; variant: "ghost"; text: "打开"
                                                    onClicked: app.openTogetherAttachment(modelData.attachmentId || "", modelData.fileName || "file")
                                                }
                                                AppC.AppButton {
                                                    theme: root.theme; variant: "soft"; text: "保存"
                                                    onClicked: app.saveTogetherAttachment(modelData.attachmentId || "", modelData.fileName || "file")
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        AppC.AppTextField {
                            id: chatInput
                            theme: root.theme
                            Layout.fillWidth: true
                            placeholderText: root.inRoom ? "发送消息…" : "请先加入房间"
                            enabled: root.inRoom
                            maximumLength: 2000
                            onAccepted: {
                                if (text.trim().length > 0) {
                                    app.sendTogetherText(text)
                                    clear()
                                }
                            }
                        }
                        AppC.AppButton {
                            theme: root.theme; variant: "primary"; text: "发送"
                            enabled: root.inRoom && chatInput.text.trim().length > 0
                            onClicked: { app.sendTogetherText(chatInput.text); chatInput.clear() }
                        }
                        AppC.AppButton {
                            theme: root.theme; variant: "soft"; text: "图片"
                            enabled: root.inRoom
                            onClicked: app.sendTogetherImage()
                        }
                        AppC.AppButton {
                            theme: root.theme; variant: "soft"; text: "文件"
                            enabled: root.inRoom
                            onClicked: app.sendTogetherFile()
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 310
                Layout.fillHeight: true
                radius: 16
                color: root.cardColor
                border.width: 1
                border.color: root.borderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 9

                    Text { text: "房间成员"; color: theme.textColor; font.pixelSize: 14; font.bold: true }
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(190, contentHeight)
                        Layout.minimumHeight: 90
                        clip: true
                        spacing: 5
                        model: root.roomValue("members", [])
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 42; radius: 9; color: root.softColor
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6
                                AppC.CoverImage {
                                    Layout.preferredWidth: 30; Layout.preferredHeight: 30
                                    source: modelData.avatar || ""; requestedSize: 80; cornerRadius: 15
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.displayName || modelData.username || "用户"
                                    color: theme.textColor; font.pixelSize: 9
                                    elide: Text.ElideRight
                                }
                                Text {
                                    visible: modelData.host === true
                                    text: "房主"; color: app.accentColor; font.pixelSize: 8; font.bold: true
                                }
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: root.borderColor }
                    Text { text: "邀请好友"; color: theme.textColor; font.pixelSize: 13; font.bold: true }
                    Text {
                        Layout.fillWidth: true
                        text: root.roomValue("isHost", false) ? "输入用户名发送邀请。" : "只有房主可以邀请新成员。"
                        color: root.mutedColor; font.pixelSize: 8; wrapMode: Text.Wrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        AppC.AppTextField {
                            id: inviteUser
                            theme: root.theme
                            Layout.fillWidth: true
                            placeholderText: "好友用户名"
                            enabled: root.roomValue("isHost", false)
                        }
                        AppC.AppButton {
                            theme: root.theme; variant: "primary"; text: "邀请"
                            enabled: root.roomValue("isHost", false) && inviteUser.text.trim().length > 0
                            onClicked: { app.inviteTogetherUser(inviteUser.text); inviteUser.clear() }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        visible: app.followingUsers.length > 0
                        spacing: 5
                        Repeater {
                            model: Math.min(5, app.followingUsers.length)
                            delegate: Rectangle {
                                required property int index
                                property var friend: app.followingUsers[index]
                                width: friendName.implicitWidth + 28; height: 25; radius: 13
                                color: friendMouse.containsMouse ? (theme.isDark ? "#34342E" : "#E5E7EB") : root.softColor
                                Text {
                                    id: friendName; anchors.centerIn: parent
                                    text: "+ " + (parent.friend.displayName || parent.friend.username || "好友")
                                    color: theme.textColor; font.pixelSize: 8
                                }
                                MouseArea {
                                    id: friendMouse; anchors.fill: parent; hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: root.roomValue("isHost", false)
                                    onClicked: app.inviteTogetherUser(parent.friend.username || "")
                                }
                            }
                        }
                    }

                    Text { text: "收到的邀请"; color: theme.textColor; font.pixelSize: 11; font.bold: true; Layout.topMargin: 4 }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: app.togetherInvites
                        spacing: 4
                        clip: true
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 42; radius: 9; color: root.softColor
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6
                                Text {
                                    Layout.fillWidth: true
                                    text: (modelData.fromName || modelData.fromUsername || "好友") + " · " + (modelData.code || "")
                                    color: theme.textColor; font.pixelSize: 8; elide: Text.ElideRight
                                }
                                AppC.AppButton {
                                    theme: root.theme; variant: "soft"; text: "加入"
                                    onClicked: app.joinTogetherRoom(modelData.code || "")
                                }
                                AppC.AppButton {
                                    theme: root.theme; variant: "ghost"; text: "忽略"
                                    onClicked: app.dismissTogetherInvite(modelData.id || "")
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "图片会自动优化；文件最大 20 MB。服务器未配置 ROOM_FILES 临时存储时，大文件发送会提示配置 R2。"
                        color: root.mutedColor; font.pixelSize: 7; wrapMode: Text.Wrap
                    }
                }
            }
        }
    }

    Component.onCompleted: { app.loadUserProfile(); app.refreshTogetherRoom() }
    onVisibleChanged: if (visible) { app.loadUserProfile(); app.refreshTogetherRoom() }
}
