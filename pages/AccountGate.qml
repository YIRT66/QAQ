import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Rectangle {
    id: root
    required property var theme
    required property var iconFont
    required property var window
    color: theme.isDark ? "#1E1E1A" : "#EEF0F4"
    property int authMode: 0 // 0 登录, 1 注册, 2 找回密码
    readonly property bool registerMode: authMode === 1
    readonly property bool resetMode: authMode === 2
    property int codeCooldown: 0
    property bool compactAuth: width < 760

    Rectangle {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 50
        color: "transparent"; z: 5
        MouseArea { anchors.left: parent.left; anchors.right: controls.left; anchors.top: parent.top; anchors.bottom: parent.bottom; onPressed: window.startSystemMove() }
        Row {
            id: controls; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.rightMargin: 8; spacing: 2
            Repeater {
                model: [{glyph:"\uf2d1",action:0},{glyph:"\uf2d0",action:1},{glyph:"\uf00d",action:2}]
                delegate: Rectangle {
                    required property var modelData
                    width: 42; height: 34; radius: 7
                    color: cm.containsMouse ? (modelData.action===2 ? "#D84B4B" : (theme.isDark ? "#18FFFFFF" : "#0B000000")) : "transparent"
                    Text { anchors.centerIn: parent; text: modelData.glyph; font.family: iconFont.name; font.pixelSize: 10; color: cm.containsMouse && modelData.action===2 ? "white" : theme.textColor }
                    MouseArea { id: cm; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { if(modelData.action===0) window.showMinimized(); else if(modelData.action===1) window.visibility===Window.Maximized?window.showNormal():window.showMaximized(); else desktop.quitApplication() } }
                }
            }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(parent.width - 34, 950)
        height: Math.min(parent.height - 70, root.registerMode ? 700 : (root.resetMode ? 620 : 540))
        radius: root.compactAuth ? 18 : 26
        clip: true
        color: theme.isDark ? "#292923" : "#FFFFFF"
        border.color: theme.isDark ? "#14FFFFFF" : "#10000000"
        scale: 0.985
        Component.onCompleted: scale = 1
        Behavior on scale { NumberAnimation { duration: app.animationLevel === "off" ? 0 : 380; easing.type: Easing.OutBack } }

        RowLayout {
            anchors.fill: parent; spacing: 0

            Rectangle {
                Layout.preferredWidth: root.compactAuth ? 0 : 370
                Layout.fillHeight: true
                visible: !root.compactAuth
                color: app.accentColor
                clip: true
                Rectangle { width: 360; height: 360; radius: 180; x: -180; y: -130; color: "#20FFFFFF" }
                Rectangle { width: 260; height: 260; radius: 130; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.rightMargin: -90; anchors.bottomMargin: -80; color: "#13000000" }
                Column {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.margins: 34; anchors.bottomMargin: 42; spacing: 10
                    Text { text: "E"; color: "white"; font.pixelSize: 60; font.bold: true; font.italic: true }
                    Text { text: "EVOLVE MUSIC"; color: "#17211F"; font.pixelSize: 23; font.bold: true }
                    Text { width: parent.width; text: app.language === "en-US" ? "Sync favorites, history, playlists and social listening across your devices." : "同步收藏、历史、自建歌单和一起听。\n双云节点自动容灾，音乐与社交状态持续同步。"; color: "#B517211F"; font.pixelSize: 10; lineHeight: 1.45; wrapMode: Text.Wrap }
                    Rectangle { width: parent.width; height: 1; color: "#2517211F" }
                    Text { text: "BETA · CLOUDFLARE D1"; color: "#9917211F"; font.pixelSize: 9; font.bold: true }
                }
            }

            Item {
                Layout.fillWidth: true; Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent; anchors.leftMargin: root.compactAuth ? 26 : 42; anchors.rightMargin: root.compactAuth ? 26 : 42; anchors.topMargin: 34; anchors.bottomMargin: 32; spacing: 11
                    Item { Layout.fillHeight: true }
                    Text { text: root.registerMode ? "创建 EvolveMusic 账号" : (root.resetMode ? "找回账户密码" : "欢迎回来"); color: theme.textColor; font.pixelSize: 25; font.bold: true }
                    Text { Layout.fillWidth: true; text: root.registerMode ? "验证邮箱后创建账号" : (root.resetMode ? "使用已绑定邮箱接收验证码" : "支持用户名或邮箱登录"); color: "#777780"; font.pixelSize: 9 }

                    Rectangle {
                        visible: !root.resetMode; Layout.fillWidth: true; Layout.preferredHeight: visible ? 48 : 0; radius: 12; color: theme.isDark ? "#35352F" : "#F0F1F4"; border.color: username.activeFocus ? app.accentColor : "transparent"
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
                            Text { text:"\uf007"; font.family:iconFont.name; color:"#8A8A92"; font.pixelSize:11 }
                            TextField { id: username; Layout.fillWidth:true; placeholderText:root.registerMode?"用户名":"用户名或邮箱"; color:theme.textColor; background:null; maximumLength:254; selectByMouse:true }
                        }
                    }
                    Rectangle {
                        visible: root.registerMode || root.resetMode; Layout.fillWidth: true; Layout.preferredHeight: visible ? 48 : 0; radius: 12; color: theme.isDark ? "#35352F" : "#F0F1F4"; border.color: email.activeFocus ? app.accentColor : "transparent"
                        RowLayout { anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; spacing: 10
                            Text { text:"@"; color:"#8A8A92"; font.pixelSize:12; font.bold:true }
                            TextField { id: email; Layout.fillWidth:true; placeholderText:"邮箱地址"; color:theme.textColor; background:null; maximumLength:254; selectByMouse:true; inputMethodHints:Qt.ImhEmailCharactersOnly }
                        }
                    }
                    Rectangle {
                        visible: root.registerMode || root.resetMode; Layout.fillWidth: true; Layout.preferredHeight: visible ? 48 : 0; radius: 12; color: theme.isDark ? "#35352F" : "#F0F1F4"; border.color: emailCode.activeFocus ? app.accentColor : "transparent"
                        RowLayout { anchors.fill: parent; anchors.leftMargin:14; anchors.rightMargin:8; spacing:8
                            Text { text:"\uf0e0"; font.family:iconFont.name; color:"#8A8A92"; font.pixelSize:10 }
                            TextField { id:emailCode; Layout.fillWidth:true; placeholderText:"6 位邮箱验证码"; color:theme.textColor; background:null; maximumLength:6; inputMethodHints:Qt.ImhDigitsOnly }
                            Rectangle {
                                Layout.preferredWidth: 92; Layout.preferredHeight: 34; radius: 10
                                color: root.codeCooldown > 0 || app.accountBusy ? (theme.isDark?"#41413B":"#D9DBDF") : app.accentColor
                                Text { anchors.centerIn:parent; text:root.codeCooldown>0?(root.codeCooldown+" 秒后重发"):(app.accountBusy?"发送中…":"发送验证码"); color:root.codeCooldown>0||app.accountBusy?"#777780":"#17211F"; font.pixelSize:8; font.bold:true }
                                MouseArea { anchors.fill:parent; enabled:root.codeCooldown===0&&!app.accountBusy; cursorShape:Qt.PointingHandCursor; onClicked:root.sendCode() }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 48; radius: 12; color: theme.isDark ? "#35352F" : "#F0F1F4"; border.color: password.activeFocus ? app.accentColor : "transparent"
                        RowLayout { anchors.fill: parent; anchors.leftMargin:14; anchors.rightMargin:14; spacing:10
                            Text { text:"\uf023"; font.family:iconFont.name; color:"#8A8A92"; font.pixelSize:11 }
                            TextField { id: password; Layout.fillWidth:true; placeholderText:root.resetMode?"新密码（至少 8 位）":"密码（至少 8 位）"; color:theme.textColor; background:null; echoMode:TextInput.Password; maximumLength:128; selectByMouse:true; onAccepted: root.submit() }
                        }
                    }
                    Rectangle {
                        visible: root.registerMode || root.resetMode; Layout.fillWidth:true; Layout.preferredHeight: visible ? 48 : 0; radius:12; color:theme.isDark?"#35352F":"#F0F1F4"; border.color:confirm.activeFocus?app.accentColor:"transparent"
                        RowLayout { anchors.fill:parent; anchors.leftMargin:14; anchors.rightMargin:14; spacing:10
                            Text { text:"\uf058"; font.family:iconFont.name; color:"#8A8A92"; font.pixelSize:11 }
                            TextField { id:confirm; Layout.fillWidth:true; placeholderText:root.resetMode?"再次输入新密码":"再次输入密码"; color:theme.textColor; background:null; echoMode:TextInput.Password; maximumLength:128 }
                        }
                        Behavior on Layout.preferredHeight { NumberAnimation { duration: app.animationLevel === "off" ? 0 : 170 } }
                    }
                    AppC.AppCheckBox { id:terms; theme:root.theme; visible:root.registerMode; text:"我同意测试版免责声明：仅将软件用于合法用途，并遵守音乐平台条款。"; checked:false }
                    Text { Layout.fillWidth:true; visible:app.accountError.length>0; text:app.accountError; color:"#E56A6A"; font.pixelSize:9; wrapMode:Text.Wrap }

                    Rectangle {
                        Layout.fillWidth:true; Layout.preferredHeight:48; radius:12
                        property bool canSubmit: root.resetMode
                            ? email.text.indexOf("@")>0 && emailCode.text.length===6 && password.text.length>=8 && confirm.text===password.text
                            : (root.registerMode
                               ? username.text.trim().length>=3 && email.text.indexOf("@")>0 && emailCode.text.length===6 && password.text.length>=8 && confirm.text===password.text && terms.checked
                               : username.text.trim().length>=3 && password.text.length>=8)
                        color: canSubmit ? app.accentColor : (theme.isDark?"#41413B":"#D9DBDF")
                        scale: submitMouse.pressed ? 0.985 : 1
                        Behavior on scale { NumberAnimation { duration:80 } }
                        Row { anchors.centerIn:parent; spacing:8
                            BusyIndicator { width:18; height:18; running:app.accountBusy; visible:running }
                            Text { text:app.accountBusy?"正在连接…":(root.registerMode?"验证并注册":(root.resetMode?"重置密码":"登录")); color:parent.parent.canSubmit?"#17211F":"#85858C"; font.pixelSize:11; font.bold:true }
                        }
                        MouseArea { id:submitMouse; anchors.fill:parent; enabled:parent.canSubmit&&!app.accountBusy; cursorShape:Qt.PointingHandCursor; onClicked:root.submit() }
                    }
                    RowLayout { Layout.fillWidth:true
                        Text { text:root.authMode===0?"还没有账号？":"返回账户登录"; color:"#8C8C94"; font.pixelSize:9 }
                        Text { text:root.authMode===0?"创建账号":"直接登录"; color:app.accentColor; font.pixelSize:9; font.bold:true; MouseArea{anchors.fill:parent;anchors.margins:-8;cursorShape:Qt.PointingHandCursor;onClicked:root.authMode=root.authMode===0?1:0} }
                        Item { Layout.fillWidth:true }
                        Text { visible:root.authMode===0; text:"忘记密码"; color:app.accentColor; font.pixelSize:9; font.bold:true; MouseArea{anchors.fill:parent;anchors.margins:-8;cursorShape:Qt.PointingHandCursor;onClicked:root.authMode=2} }
                    }
                    RowLayout { Layout.fillWidth:true
                        Rectangle { width:7;height:7;radius:4;color:app.cloudReady?"#44D49B":"#E0A85A" }
                        Text { Layout.fillWidth:true; text:"当前节点："+(app.cloudApiUrl.length?app.cloudApiUrl.replace("xn--fiqq40n","中转"):"正在选择…"); color:"#777780"; font.pixelSize:8; elide:Text.ElideMiddle }
                    }
                    Text { Layout.fillWidth:true; text:"登录成功后会以淡入过渡进入推荐页；账号资料和自建歌单同步到 EvolveMusic Cloud。"; color:"#777780"; font.pixelSize:8; wrapMode:Text.Wrap }
                    Item { Layout.fillHeight:true }
                }
            }
        }
    }

    Timer {
        interval: 1000; repeat: true; running: root.codeCooldown > 0
        onTriggered: root.codeCooldown = Math.max(0, root.codeCooldown - 1)
    }

    Connections {
        target: app
        function onEmailCodeSent(purpose, cooldownSeconds) {
            var expected = root.registerMode ? "register" : "password_reset"
            if (purpose === expected) root.codeCooldown = Math.max(1, Number(cooldownSeconds || 60))
        }
        function onPasswordResetCompleted() {
            root.authMode = 0
            username.text = email.text
            password.text = ""
            confirm.text = ""
            emailCode.text = ""
        }
    }

    function submit() {
        if (app.accountBusy) return
        if (root.registerMode) {
            if (password.text !== confirm.text) return
            app.registerAccount(username.text.trim(), password.text, email.text.trim(), emailCode.text.trim(), terms.checked)
        } else if (root.resetMode) {
            if (password.text !== confirm.text) return
            app.resetPassword(email.text.trim(), emailCode.text.trim(), password.text)
        } else app.loginAccount(username.text.trim(), password.text)
    }

    function sendCode() {
        var address = email.text.trim()
        if (address.indexOf("@") <= 0) return
        if (root.registerMode) app.sendRegistrationEmailCode(address)
        else if (root.resetMode) app.sendPasswordResetCode(address)
    }
}
