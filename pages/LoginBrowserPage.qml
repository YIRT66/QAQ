import QtQuick
import QtQuick.Layouts
import QtWebView
import "../components" as AppC
import "../third_party/EvolveUI/components" as EUI

Rectangle {
    id: root
    property var theme
    property var iconFont
    property var window
    signal closeRequested()

    property string browserStatus: "准备登录页…"
    property string browserError: ""
    property bool pageSucceeded: false
    property bool pageLooksBlank: false
    property bool neteaseQrMode: accounts.activeProviderId === "netease"

    color: "#F40E0E11"

    function neteaseQrHtml() {
        return [
            "<!doctype html><html><head><meta charset='utf-8'>",
            "<meta name='viewport' content='width=device-width,initial-scale=1'>",
            "<style>",
            "*{box-sizing:border-box} body{margin:0;background:#fff;color:#202024;font-family:'Microsoft YaHei UI','Segoe UI',sans-serif;height:100vh;display:flex;align-items:center;justify-content:center}",
            ".card{width:440px;max-width:92vw;padding:34px 34px 30px;border:1px solid #ececf1;border-radius:22px;box-shadow:0 16px 50px rgba(30,30,40,.07);text-align:center}",
            ".logo{width:52px;height:52px;border-radius:16px;margin:0 auto 15px;background:linear-gradient(135deg,#ff3b52,#e91839);display:flex;align-items:center;justify-content:center;color:#fff;font-size:25px;font-weight:700}",
            "h2{margin:0 0 7px;font-size:21px}.sub{font-size:12px;color:#8a8a94;margin-bottom:20px}",
            ".qrbox{width:222px;height:222px;margin:0 auto 18px;border-radius:18px;background:#fafafd;border:1px solid #ededf2;display:flex;align-items:center;justify-content:center;overflow:hidden}",
            "#qr{width:198px;height:198px;object-fit:contain;display:none}.spinner{width:25px;height:25px;border:3px solid #ececf2;border-top-color:#7c62ff;border-radius:50%;animation:r 1s linear infinite}@keyframes r{to{transform:rotate(360deg)}}",
            "#status{font-size:13px;font-weight:600;color:#4a4a54}.hint{margin-top:8px;font-size:11px;color:#9999a2;line-height:1.7}",
            ".ok{color:#20b87a!important}.warn{color:#e39a32!important}.err{color:#d94b56!important}",
            "button{margin-top:16px;border:0;border-radius:12px;padding:9px 16px;background:#f1efff;color:#7058f5;font-weight:600;cursor:pointer}",
            "</style></head><body><div class='card'>",
            "<div class='logo'>♪</div><h2>网易云音乐扫码登录</h2>",
            "<div class='sub'>请使用网易云音乐 App 扫码并在手机上确认</div>",
            "<div class='qrbox'><div id='spin' class='spinner'></div><img id='qr'></div>",
            "<div id='status'>正在生成二维码…</div>",
            "<div class='hint'>登录成功后，回到 Evolve Music 点击右上角“完成登录”。<br>这里只取得你自己的登录会话，不改变平台会员或版权权限。</div>",
            "<button onclick='start()'>刷新二维码</button>",
            "</div><script>",
            "let timer=null,key='';",
            "function state(t,c){const e=document.getElementById('status');e.textContent=t;e.className=c||'';}",
            "async function start(){",
            " if(timer)clearTimeout(timer); localStorage.removeItem('evolve_netease_cookie');",
            " document.getElementById('qr').style.display='none';document.getElementById('spin').style.display='block';state('正在生成二维码…','');",
            " try{",
            "  let a=await fetch('/login/qr/key?timestamp='+Date.now(),{cache:'no-store'}).then(r=>r.json());",
            "  key=(a.data&&a.data.unikey)||(a.data&&a.data.data&&a.data.data.unikey)||'';",
            "  if(!key)throw new Error('没有取得二维码 key');",
            "  let b=await fetch('/login/qr/create?key='+encodeURIComponent(key)+'&qrimg=true&timestamp='+Date.now(),{cache:'no-store'}).then(r=>r.json());",
            "  let d=b.data||(b.body&&b.body.data)||{}; let img=d.qrimg||'';",
            "  if(!img)throw new Error('没有取得二维码图片');",
            "  const q=document.getElementById('qr');q.src=img;q.style.display='block';document.getElementById('spin').style.display='none';",
            "  state('等待扫码',''); poll();",
            " }catch(e){document.getElementById('spin').style.display='none';state('二维码生成失败：'+e.message,'err');}",
            "}",
            "async function poll(){",
            " try{",
            "  let j=await fetch('/login/qr/check?key='+encodeURIComponent(key)+'&timestamp='+Date.now(),{cache:'no-store'}).then(r=>r.json());",
            "  if(j.cookie){localStorage.setItem('evolve_netease_cookie',j.cookie);state('登录成功 · 请点击右上角“完成登录”','ok');return;}",
            "  let code=Number(j.code||0);",
            "  if(code===800){state('二维码已过期，请刷新','err');return;}",
            "  if(code===802){state('已扫码 · 请在手机上确认','warn');}",
            "  else{state(j.message||'等待扫码','');}",
            " }catch(e){state('连接本地网易云服务失败，正在重试…','warn');}",
            " timer=setTimeout(poll,1500);",
            "}",
            "start();",
            "</script></body></html>"
        ].join("")
    }

    function initializeBrowser() {
        // Qt WebView exposes these settings in Qt 6.10. Explicitly enable them
        // because the native backend's platform defaults may differ.
        try {
            browser.settings.javaScriptEnabled = true
            browser.settings.localStorageEnabled = true
        } catch (e) {}

        if (root.neteaseQrMode) {
            root.browserStatus = "网易云扫码登录"
            browser.loadHtml(root.neteaseQrHtml(), "http://127.0.0.1:3000/")
        } else {
            root.browserStatus = "正在打开 " + accounts.activeProviderName + "…"
            browser.url = accounts.activeLoginUrl
        }
    }

    function captureSession() {
        var script = "(function(){"
                   + "var out={cookie:(document.cookie||''),localStorage:{},sessionStorage:{}};"
                   + "try{for(var i=0;i<localStorage.length;i++){var k=localStorage.key(i);out.localStorage[k]=localStorage.getItem(k);}}catch(e){}"
                   + "try{for(var j=0;j<sessionStorage.length;j++){var s=sessionStorage.key(j);out.sessionStorage[s]=sessionStorage.getItem(s);}}catch(e){}"
                   + "return JSON.stringify(out);"
                   + "})()"
        browser.runJavaScript(script, function(result) {
            accounts.finishWebLoginPayload(
                accounts.activeProviderId,
                result ? String(result) : "{}",
                browser.httpUserAgent ? String(browser.httpUserAgent) : "",
                browser.url ? browser.url.toString() : ""
            )
        })
    }

    function probePage() {
        if (root.neteaseQrMode)
            return
        var script = "(function(){var b=document.body;"
                   + "return JSON.stringify({title:(document.title||''),text:(b&&b.innerText||'').trim().slice(0,160),nodes:(b?b.children.length:0)});"
                   + "})()"
        browser.runJavaScript(script, function(result) {
            try {
                var info = JSON.parse(String(result || "{}"))
                root.pageLooksBlank = !info.title && !info.text && Number(info.nodes || 0) === 0
                if (root.pageLooksBlank)
                    root.browserStatus = "网页已返回，但内容为空白 · 可尝试刷新"
            } catch (e) {}
        })
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(parent.width - 42, 1160)
        height: Math.min(parent.height - 42, 790)
        radius: 24
        color: theme.isDark ? "#17171C" : "#FFFFFF"
        border.color: theme.isDark ? "#20FFFFFF" : "#16000000"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                Layout.leftMargin: 14
                Layout.rightMargin: 12
                spacing: 7

                AppC.PlayerIconButton {
                    width: 36
                    height: 36
                    iconFont: root.iconFont
                    glyph: "\uf060"
                    glyphColor: theme.textColor
                    enabled: browser.canGoBack && !root.neteaseQrMode
                    onClicked: browser.goBack()
                }

                AppC.PlayerIconButton {
                    width: 36
                    height: 36
                    iconFont: root.iconFont
                    glyph: "\uf2f1"
                    glyphColor: theme.textColor
                    onClicked: {
                        root.browserError = ""
                        root.pageLooksBlank = false
                        if (root.neteaseQrMode)
                            browser.loadHtml(root.neteaseQrHtml(), "http://127.0.0.1:3000/")
                        else
                            browser.reload()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: "登录 " + accounts.activeProviderName
                        color: theme.textColor
                        font.pixelSize: 14
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.neteaseQrMode
                              ? "本地扫码登录 · 127.0.0.1:3000"
                              : (browser.url ? browser.url.toString() : accounts.activeLoginUrl.toString())
                        color: "#7E7E88"
                        font.pixelSize: 9
                        elide: Text.ElideMiddle
                    }
                }

                EUI.EButton {
                    text: "完成登录"
                    size: "xs"
                    containerColor: "#7C62FF"
                    hoverColor: "#8A72FF"
                    textColor: "white"
                    shadowEnabled: false
                    onClicked: root.captureSession()
                }

                AppC.PlayerIconButton {
                    width: 36
                    height: 36
                    iconFont: root.iconFont
                    glyph: "\uf00d"
                    glyphColor: theme.textColor
                    hoverColor: "#38D94848"
                    onClicked: root.closeRequested()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: theme.isDark ? "#18FFFFFF" : "#12000000"
            }

            // Keep the native WebView completely between the header and footer.
            // Qt documents that native WebView backends cannot be reliably
            // overlapped or clipped by arbitrary QML items.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 1
                Layout.rightMargin: 1

                WebView {
                    id: browser
                    anchors.fill: parent

                    onLoadingChanged: function(loadRequest) {
                        if (loadRequest.status === WebView.LoadStartedStatus) {
                            root.pageSucceeded = false
                            root.browserError = ""
                            root.browserStatus = root.neteaseQrMode ? "正在打开扫码登录…" : "网页加载中…"
                        } else if (loadRequest.status === WebView.LoadSucceededStatus) {
                            root.pageSucceeded = true
                            root.browserError = ""
                            root.browserStatus = root.neteaseQrMode ? "扫码登录页已就绪" : "网页加载完成"
                            pageProbe.restart()
                        } else if (loadRequest.status === WebView.LoadFailedStatus) {
                            root.pageSucceeded = false
                            root.browserError = loadRequest.errorString || "未知 WebView2 加载错误"
                            root.browserStatus = "网页加载失败"
                        }
                    }
                }

                Timer {
                    id: pageProbe
                    interval: 650
                    repeat: false
                    onTriggered: root.probePage()
                }
            }

            // The footer owns its own rounded bottom geometry. A normal
            // Rectangle radius on the parent does not round-clip child items.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 46

                Rectangle {
                    anchors.fill: parent
                    radius: 22
                    color: theme.isDark ? "#15151A" : "#F7F7F9"
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 22
                    color: theme.isDark ? "#15151A" : "#F7F7F9"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 8

                    Rectangle {
                        width: 7
                        height: 7
                        radius: 4
                        color: root.browserError
                               ? "#E05A63"
                               : (browser.loading ? "#F0B454" : "#45D483")
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.browserError
                              ? ("加载失败 · " + root.browserError)
                              : (browser.loading
                                 ? ("网页加载中 · " + browser.loadProgress + "%")
                                 : root.browserStatus)
                        color: root.browserError ? "#D94B56" : "#85858F"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: !root.neteaseQrMode && root.pageLooksBlank
                        text: "空白页"
                        color: "#E39A32"
                        font.pixelSize: 10
                        font.bold: true
                    }

                    Text {
                        text: "不绕过会员 / DRM"
                        color: "#8D75FF"
                        font.pixelSize: 10
                    }
                }
            }
        }

        Component.onCompleted: root.initializeBrowser()
    }
}
