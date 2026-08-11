import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import "components" as AppC
import "pages" as Pages
import "third_party/EvolveUI/components" as EUI

ApplicationWindow {
    id: root
    visible: !desktop.startInBackground
    width: 1360; height: 820; minimumWidth: 640; minimumHeight: 480
    title: "Evolve Music"; flags: Qt.Window | Qt.FramelessWindowHint; color: "transparent"
    // The UI font is applied once in C++ before the QML engine loads this tree.
    // Do NOT bind ApplicationWindow.font to app.fontFamily/resolvedFontFamily:
    // Qt 6.10 on Windows can recursively invalidate the whole QML tree when
    // live font NOTIFY/model invalidation previously caused a stack overflow during cloud hydration.

    property int pageIndex: 0
    property int lastContentPage: 0
    property string searchQuery: ""
    property int homeSection: 0
    property int libraryTab: 0
    property var navHistory: [0]
    property var pendingTogetherInvite: ({})
    property var selectedArtist: ({})
    property int navPos: 0
    readonly property bool immersivePlayer: pageIndex === 5
    readonly property bool shortWindow: height < 620
    readonly property bool tinyLayout: width < 720 || height < 520
    readonly property string layoutClass: tinyLayout ? "tiny" : ((width < 960 || shortWindow) ? "compact" : ((width < 1320 || height < 720) ? "normal" : "wide"))
    readonly property bool compactLayout: layoutClass === "tiny" || layoutClass === "compact"
    readonly property bool wideLayout: layoutClass === "wide"
    readonly property int sidebarWidth: immersivePlayer ? 0 : (tinyLayout ? 60 : (compactLayout ? 72 : (wideLayout ? 204 : 180)))
    readonly property int contentGutter: tinyLayout ? 8 : (compactLayout ? 12 : (wideLayout ? 30 : 20))
    readonly property int playerHeight: tinyLayout ? 64 : (compactLayout ? 70 : 76)
    readonly property int titleHeight: tinyLayout ? 46 : (compactLayout ? 52 : 58)
    readonly property color appCanvasColor: app.themePreset === "oled" ? "#090909"
                                            : app.themePreset === "midnight" ? "#171923"
                                            : app.themePreset === "forest" ? "#1C241E"
                                            : app.themePreset === "violet" ? "#211D29"
                                            : app.themePreset === "paper" ? "#F4F1E8"
                                            : (theme.isDark ? "#20201B" : "#F0F1F5")

    onClosing: function(close) {
        if (desktop.quitting)
            return
        close.accepted = false
        if (desktop.backgroundEnabled) {
            root.hide()
            desktop.showTrayMessage("EvolveMusic", "已在后台运行，双击托盘图标可重新打开。")
        } else {
            desktop.quitApplication()
        }
    }

    FontLoader { id: iconFont; source: "qrc:/new/prefix1/fonts/fontawesome-free-6.7.2-desktop/otfs/Font Awesome 6 Free-Solid-900.otf" }
    EUI.ETheme { id: theme; isDark: app.darkMode; focusColor: app.accentColor }

    AppC.WebPlaybackHost { }

    function navigate(index, record) {
        if (record === undefined) record = true
        if (index !== 5 && index !== 6) lastContentPage = index
        if (record && pageIndex !== index) {
            var h = navHistory.slice(0, navPos + 1); h.push(index); navHistory = h; navPos = h.length - 1
        }
        if (pageIndex === index) return
        if (app.animationLevel !== "off") pageHost.opacity = 0.35
        pageIndex = index
        Qt.callLater(function(){ pageHost.opacity = 1 })
    }
    function goBack() { if(navPos>0){ navPos--; navigate(navHistory[navPos], false) } }
    function goForward() { if(navPos<navHistory.length-1){ navPos++; navigate(navHistory[navPos], false) } }
    function openNowPlaying(){ if(pageIndex!==5&&pageIndex!==6) lastContentPage=pageIndex; navigate(5) }
    function leaveNowPlaying(){ navigate(lastContentPage) }
    function openHome(section){ homeSection=section; navigate(1) }
    function openLibrary(tab){ libraryTab=tab; navigate(4) }
    function refreshCurrent(){
        if(pageIndex===0) app.startRoam(true)
        else if(pageIndex===1) app.refreshHome()
        else if(pageIndex===2 && searchQuery.length) app.search(searchQuery)
        else if(pageIndex===6) { var ps=app.providers; for(var i=0;i<ps.length;i++) if(ps[i].enabled) app.testProvider(ps[i].id) }
    }

    Rectangle {
        id: shell; anchors.fill: parent; radius: (root.visibility===Window.Maximized || root.visibility===Window.FullScreen)?0:12; clip: true
        color: root.immersivePlayer ? "#36352B" : root.appCanvasColor
        border.color: theme.isDark ? "#14FFFFFF" : "#10000000"
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 220 : 0 } }

        AppC.TitleBar {
            id: titleBar; anchors.left: sidebar.right; anchors.right: parent.right; anchors.top: parent.top; height: root.immersivePlayer ? 0 : root.titleHeight; opacity: root.immersivePlayer?0:1; visible: opacity>0
            theme: theme; iconFont: iconFont; window: root; compact: root.compactLayout; canGoBack: root.navPos>0; canGoForward: root.navPos<root.navHistory.length-1; z:50
            onSearchRequested: function(q){ root.searchQuery=q; root.navigate(2); app.search(q) }
            onBackRequested: root.goBack(); onForwardRequested: root.goForward(); onRefreshRequested: root.refreshCurrent(); onSettingsRequested: root.navigate(6)
            Behavior on opacity { NumberAnimation { duration: app.animationsEnabled ? 150 : 0 } }
        }

        AppC.Sidebar {
            id: sidebar; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: mini.top; anchors.margins: root.immersivePlayer?0:4; width: root.sidebarWidth; opacity: root.immersivePlayer?0:1; visible: opacity>0
            theme: theme; iconFont: iconFont; currentIndex: root.pageIndex; homeSection: root.homeSection; libraryTab: root.libraryTab; compact: root.compactLayout
            onNavigate: function(i){
                root.navigate(i)
                if (i === 0) app.startRoam(false)
            }
            onBrowseRequested: function(s){ root.openHome(s) }
            onLibraryRequested: function(t){ root.openLibrary(t) }
            onCustomPlaylistRequested: function(id){ app.openCustomPlaylist(id) }
            onCreatePlaylistRequested: { playlistPicker.addMode = false; playlistPicker.track = ({}); playlistPicker.open() }
            Behavior on width { NumberAnimation { duration: app.animationsEnabled ? 180 : 0; easing.type:Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: app.animationsEnabled ? 140 : 0 } }
        }

        Item {
            id: pageHost; anchors.left: root.immersivePlayer?parent.left:sidebar.right; anchors.right: parent.right; anchors.top: root.immersivePlayer?parent.top:titleBar.bottom; anchors.bottom: mini.top
            anchors.leftMargin: root.immersivePlayer?0:root.contentGutter; anchors.rightMargin: root.immersivePlayer?0:root.contentGutter; anchors.topMargin: root.immersivePlayer?0:(root.compactLayout?6:12); anchors.bottomMargin: root.immersivePlayer?0:8
            Behavior on opacity { NumberAnimation { duration: app.animationLevel === "rich" ? 190 : (app.animationLevel === "balanced" ? 120 : 0); easing.type: Easing.OutCubic } }
            StackLayout {
                anchors.fill: parent; currentIndex: root.pageIndex
                Pages.RoamPage { theme: theme; iconFont: iconFont }
                Pages.HomePage {
                    theme: theme
                    iconFont: iconFont
                    section: 0
                    onSearchRequested: function(q) {
                        root.searchQuery = q
                        root.navigate(2)
                        app.search(q)
                    }
                    onAllPlaylistsRequested: root.navigate(10)
                }
                Pages.SearchPage {
                    theme: theme; iconFont: iconFont; query: root.searchQuery
                    onArtistRequested: function(artist) {
                        root.selectedArtist = artist
                        root.navigate(9)
                    }
                }
                Pages.PlaylistPage { theme: theme; iconFont: iconFont }
                Pages.LibraryPage { theme: theme; iconFont: iconFont; tab: root.libraryTab; onTabChanged: root.libraryTab=tab }
                Pages.NowPlayingPage { id: nowPlayingPage; theme: theme; iconFont: iconFont; window: root; onBackRequested: root.leaveNowPlaying() }
                Pages.SettingsPage { theme: theme; iconFont: iconFont }
                Pages.TogetherPage { theme: theme; iconFont: iconFont }
                Pages.ProfilePage { theme: theme; iconFont: iconFont }
                Pages.ArtistPage { theme: theme; iconFont: iconFont; artist: root.selectedArtist }
                Pages.AllPlaylistsPage { theme: theme; iconFont: iconFont }
            }
        }

        AppC.PluginHost { anchors.fill: parent }

        AppC.MiniPlayer {
            id: mini; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.leftMargin: root.immersivePlayer?8:4; anchors.rightMargin: root.immersivePlayer?8:4; anchors.bottomMargin: root.immersivePlayer?8:4; height: root.playerHeight
            theme: theme; iconFont: iconFont; immersive: root.immersivePlayer; compact: root.compactLayout; tiny: root.tinyLayout; z:90; onOpenNowPlaying: root.openNowPlaying(); onQueueRequested: queuePopup.open(); onAddToPlaylistRequested: { playlistPicker.addMode = true; playlistPicker.track = player.currentTrack; playlistPicker.open() }
        }
        AppC.QueuePopup { id:queuePopup; parent:root.contentItem; theme:theme; iconFont:iconFont; width:Math.min(390, root.width - 24); x:Math.max(8,root.width-width-12); y:Math.max(8,root.height-height-mini.height-10); z:180 }
        AppC.PlaylistPickerPopup { id: playlistPicker; parent: root.contentItem; theme: theme; iconFont: iconFont; x: Math.max(14, (root.width-width)/2); y: Math.max(14, (root.height-height)/2); z: 260 }

        Popup {
            id: togetherInvitePopup
            parent: root.contentItem
            modal: false
            focus: true
            width: Math.min(430, root.width - 32)
            height: 190
            x: Math.max(16, root.width - width - 22)
            y: root.titleHeight + 14
            padding: 0
            z: 420
            closePolicy: Popup.NoAutoClose
            background: Rectangle {
                radius: 18
                color: theme.isDark ? "#302F29" : "#FFFFFF"
                border.width: 1
                border.color: theme.isDark ? "#24FFFFFF" : "#12000000"
            }
            contentItem: ColumnLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Rectangle { width: 36; height: 36; radius: 12; color: app.accentColor; Text { anchors.centerIn: parent; text: "♪"; color: "#17211F"; font.pixelSize: 14; font.bold: true } }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 1
                        Text { text: app.language === "en-US" ? "Listen Together invitation" : "一起听邀请"; color: theme.textColor; font.pixelSize: 14; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            text: (root.pendingTogetherInvite.fromName || root.pendingTogetherInvite.fromUsername || "好友")
                                  + (app.language === "en-US" ? " invited you to room " : " 邀请你进入房间 ")
                                  + (root.pendingTogetherInvite.code || "")
                            color: theme.isDark ? "#A8A9A2" : "#70747C"; font.pixelSize: 9; elide: Text.ElideRight
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    AppC.AppButton {
                        theme: theme; variant: "ghost"; text: app.language === "en-US" ? "Ignore" : "忽略"
                        onClicked: { app.dismissTogetherInvite(root.pendingTogetherInvite.id || ""); togetherInvitePopup.close() }
                    }
                    AppC.AppButton {
                        theme: theme; variant: "primary"; text: app.language === "en-US" ? "Join" : "加入房间"
                        onClicked: {
                            var c = root.pendingTogetherInvite.code || ""
                            togetherInvitePopup.close()
                            root.navigate(7)
                            if (c.length) app.joinTogetherRoom(c)
                        }
                    }
                }
            }
        }

        Rectangle {
            id: toast
            property string message: ""
            width: Math.min(root.width - 32, 520, Math.max(180, toastText.implicitWidth + 36))
            height: 40
            radius: 12
            clip: true
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: mini.top
            anchors.bottomMargin: 12
            color: theme.isDark ? "#F03A3A33" : "#F8FFFFFF"
            border.color: "#18FFFFFF"
            opacity: 0
            scale: 0.96
            z: 300

            Text {
                id: toastText
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                text: toast.message
                color: theme.textColor
                font.pixelSize: 11
                elide: Text.ElideMiddle
                maximumLineCount: 1
                horizontalAlignment: Text.AlignHCenter
            }

            Behavior on opacity { NumberAnimation { duration:app.animationsEnabled?150:0 } }
            Behavior on scale { NumberAnimation { duration:app.animationsEnabled?160:0; easing.type:Easing.OutBack } }
            Timer { id:toastTimer; interval:2200; onTriggered:{toast.opacity=0;toast.scale=0.96} }
        }
        BusyIndicator { anchors.centerIn:pageHost; running:app.loading&&root.pageIndex!==5; visible:running; z:100 }
        Loader {
            id:loginOverlay; anchors.fill:parent; z:500; active:false
            source:accounts.activeProviderId==="netease"?"pages/NeteaseQrLoginPage.qml":(accounts.embeddedBrowserAvailable?"pages/LoginBrowserPage.qml":"pages/LoginBrowserFallback.qml")
            onLoaded:{ item.theme=theme; item.iconFont=iconFont; item.window=root; item.closeRequested.connect(function(){accounts.cancelLogin();loginOverlay.active=false}) }
        }
        Pages.AccountGate {
            anchors.fill: parent
            theme: theme
            iconFont: iconFont
            window: root
            visible: opacity > 0.01
            opacity: app.accountLoggedIn ? 0 : 1
            scale: app.accountLoggedIn ? 1.025 : 1
            enabled: !app.accountLoggedIn
            z: 1000
            Behavior on opacity { NumberAnimation { duration: app.animationLevel === "off" ? 0 : 380; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: app.animationLevel === "off" ? 0 : 420; easing.type: Easing.OutCubic } }
        }
    }
    Connections {
        target: app
        function onToastRequested(m){ toast.message=m; toast.opacity=1; toast.scale=1; toastTimer.restart() }
        function onPlaylistOpened(){ root.navigate(3) }
        function onTogetherInviteArrived(invite){
            root.pendingTogetherInvite = invite
            togetherInvitePopup.open()
            desktop.showTrayMessage("EvolveMusic", (invite.fromName || invite.fromUsername || "好友") + " 邀请你一起听歌")
        }
        function onTogetherRoomClosed(message){
            root.navigate(7)
        }
    }
    Connections { target:accounts; function onBrowserRequested(){loginOverlay.active=true} function onBrowserLoginSucceeded(){loginOverlay.active=false} function onToastRequested(m){toast.message=m;toast.opacity=1;toast.scale=1;toastTimer.restart()} }
    Connections {
        target: desktop
        function onShowRequested(){ root.show(); root.raise(); root.requestActivate() }
        function onHideRequested(){ root.hide() }
        function onToastRequested(m){ toast.message=m; toast.opacity=1; toast.scale=1; toastTimer.restart() }
    }
    Shortcut { sequence:"Space"; onActivated:player.togglePlay() }
    Shortcut { sequence:"Ctrl+Right"; onActivated:player.next() }
    Shortcut { sequence:"Ctrl+Left"; onActivated:player.previous() }
    Shortcut {
        sequence: "F11"
        enabled: root.immersivePlayer
        onActivated: nowPlayingPage.toggleFullScreen()
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.immersivePlayer
        onActivated: {
            if (root.visibility === Window.FullScreen)
                nowPlayingPage.exitFullScreen()
            else
                root.leaveNowPlaying()
        }
    }
}
