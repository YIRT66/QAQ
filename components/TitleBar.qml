import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Item {
    id: root
    property var theme
    property var iconFont
    property var window
    property bool canGoBack: false
    property bool canGoForward: false
    property bool compact: false
    signal searchRequested(string query)
    signal backRequested()
    signal forwardRequested()
    signal refreshRequested()
    signal settingsRequested()

    property var accountSnapshot: accounts.accounts
    readonly property var signedAccount: {
        var list = accountSnapshot
        for (var i = 0; i < list.length; ++i) if (list[i].loggedIn) return list[i]
        return null
    }
    readonly property string accountLabel: {
        if (app.accountLoggedIn)
            return String((app.userProfile && app.userProfile.displayName) || app.accountUsername || "Evolve 用户")
        if (root.signedAccount)
            return String(root.signedAccount.displayName || root.signedAccount.name || "音乐账号")
        return "登录音乐账号"
    }

    Timer {
        id: topSearchDebounce
        interval: 520
        repeat: false
        onTriggered: {
            var q = searchInput.text.trim()
            if (q.length >= 2)
                root.searchRequested(q)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 6 : 18
        anchors.rightMargin: 10
        spacing: 5

        PlayerIconButton { width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf104"; glyphSize: 13; glyphColor: "#8A8A91"; enabled: root.canGoBack; onClicked: root.backRequested() }
        PlayerIconButton { visible: !root.compact; width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf105"; glyphSize: 13; glyphColor: "#8A8A91"; enabled: root.canGoForward; onClicked: root.forwardRequested() }
        PlayerIconButton { visible: !root.compact; width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf2f1"; glyphSize: 12; glyphColor: "#707078"; onClicked: root.refreshRequested() }

        Rectangle {
            Layout.leftMargin: 6
            Layout.preferredWidth: root.compact ? Math.max(180, Math.min(280, root.width - 230)) : Math.min(340, Math.max(220, root.width * 0.28))
            Layout.preferredHeight: 34
            radius: 17
            color: theme.isDark ? "#3A3A32" : "#E9EAEE"
            border.color: searchInput.activeFocus ? app.accentColor : "transparent"
            Behavior on border.color { ColorAnimation { duration: app.animationsEnabled ? 130 : 0 } }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 13; anchors.rightMargin: 11; spacing: 8
                Text { text: "\uf002"; font.family: iconFont.name; color: "#8D8D94"; font.pixelSize: 10 }
                TextInput {
                    id: searchInput
                    Layout.fillWidth: true; color: theme.textColor; font.pixelSize: 11; clip: true
                    selectionColor: app.accentColor; selectedTextColor: "white"
                    onTextEdited: {
                        topSearchDebounce.stop()
                        if (text.trim().length >= 2)
                            topSearchDebounce.restart()
                    }
                    onAccepted: {
                        topSearchDebounce.stop()
                        if (text.trim().length)
                            root.searchRequested(text.trim())
                    }
                    Text { visible: !searchInput.text.length; text: "搜索音乐"; color: "#8E8E95"; font.pixelSize: 10 }
                }
            }
        }

        Item { Layout.fillWidth: true }

        Rectangle {
            visible: !root.compact && app.accountLoggedIn
            Layout.preferredWidth: onlineText.implicitWidth + 24
            Layout.preferredHeight: 28
            radius: 14
            color: theme.isDark ? "#24322D" : "#E8F4F0"
            border.width: 1
            border.color: theme.isDark ? "#263FD2A0" : "#2236B88D"
            RowLayout {
                anchors.centerIn: parent; spacing: 6
                Rectangle { width: 7; height: 7; radius: 4; color: app.cloudReady ? "#42D69A" : "#E0A85A" }
                Text { id: onlineText; text: app.onlineUsers + (app.language === "en-US" ? " online" : " 人在线"); color: theme.textColor; font.pixelSize: 8; font.bold: true }
            }
        }

        Rectangle {
            visible: !root.compact
            Layout.preferredWidth: Math.max(116, accountName.implicitWidth + 54)
            Layout.preferredHeight: 34
            radius: 17
            color: accountMouse.containsMouse ? (theme.isDark ? "#4A4A40" : "#FFFFFF") : (theme.isDark ? "#3A3A32" : "#F7F8FA")
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 10; spacing: 7
                Rectangle {
                    width: 24; height: 24; radius: 12; color: app.accentColor
                    Text { anchors.centerIn: parent; text: root.accountLabel === "登录音乐账号" ? "♪" : root.accountLabel.charAt(0).toUpperCase(); color: "#17211F"; font.pixelSize: 10; font.bold: true }
                }
                Text { id: accountName; Layout.fillWidth: true; text: root.accountLabel; color: theme.textColor; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
                Text { text: "\uf107"; font.family: iconFont.name; color: "#85858C"; font.pixelSize: 9 }
            }
            MouseArea { id: accountMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.settingsRequested() }
        }

        PlayerIconButton { width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf013"; glyphSize: 12; glyphColor: "#77777F"; onClicked: root.settingsRequested() }
        PlayerIconButton { width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf2d1"; glyphSize: 10; glyphColor: "#77777F"; onClicked: root.window.showMinimized() }
        PlayerIconButton { width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf2d0"; glyphSize: 10; glyphColor: "#77777F"; onClicked: root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized() }
        PlayerIconButton { width: 34; height: 34; iconFont: root.iconFont; glyph: "\uf00d"; glyphSize: 11; glyphColor: "#77777F"; hoverColor: "#35E74D4D"; onClicked: root.window.close() }
    }

    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: parent.height
        z: -1
        onPressed: root.window.startSystemMove()
        onDoubleClicked: root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized()
    }
}
