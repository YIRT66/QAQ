import QtQuick
import QtWebView

Item {
    id: root
    width: 1
    height: 1

    property string requestId: ""
    property string songId: ""
    property bool download: false
    property int attempts: 0
    property bool started: false
    property string verifyKeyword: ""
    property bool resolving: false

    function jsString(value) { return JSON.stringify(String(value || "")) }

    function startPageResolve() {
        if (!root.requestId || root.started) return
        root.started = true
        root.resolving = true
        root.attempts = 0
        browser.runJavaScript(
            "(function(id){try{" +
            "if(typeof window.getMusicInfo==='function'){window.getMusicInfo(id,id);return 'started';}" +
            "return 'missing';}catch(e){return 'error:'+String(e);}})(" + jsString(root.songId) + ")",
            function(result) {
                if (String(result || "") === "missing") {
                    webPlayback.resolveFailure(root.requestId, "悦听网页解析脚本未加载")
                    root.finish()
                } else {
                    pollTimer.restart()
                    resolveTimeout.restart()
                }
            })
    }

    function pollResult() {
        if (!root.requestId) return
        browser.runJavaScript(
            "(function(){try{" +
            "var u='';" +
            "try{var d=document.createElement('div');d.innerHTML=window.aa||'';var a=d.querySelector('a[href]');u=a?a.href:'';}catch(e){}" +
            "if(!u||u==='#'){document.querySelectorAll('audio,source').forEach(function(e){if(!u)u=e.currentSrc||e.src||'';});}" +
            "return u||'';}catch(e){return '';}})()",
            function(result) {
                var url = String(result || "").trim()
                var host = ""
                try { host = new URL(url).hostname || "" } catch (e) {}
                var isHttp = /^https?:\/\//i.test(url)
                var isKnownAudioHost = /(^|\.)kw-lw\.kuwo\.cn$|(^|\.)kuwo\.cn$/i.test(host)
                var looksAudio = /\.(mp3|m4a|aac|flac|ogg|opus|wav)(\?|$)/i.test(url)
                if (isHttp && (looksAudio || isKnownAudioHost)) {
                    webPlayback.resolveSuccess(root.requestId, url)
                    root.finish()
                    return
                }
                root.attempts++
                if (root.attempts >= 24) {
                    webPlayback.resolveFailure(root.requestId, "悦听网页解析超时")
                    root.finish()
                }
            })
    }

    function finish() {
        pollTimer.stop()
        resolveTimeout.stop()
        root.requestId = ""
        root.songId = ""
        root.started = false
        root.resolving = false
        root.attempts = 0
    }

    Window {
        id: renderWindow
        // WebView2 may throttle JavaScript in an entirely off-screen window.
        // Keep the official Yueting page visible while it resolves so its
        // public JavaScript flow actually runs on all Windows configurations.
        visible: root.requestId.length > 0
        x: 48
        y: 48
        width: 980
        height: 720
        title: "悦听音源解析"
        flags: Qt.Tool
        color: "white"

        WebView {
            id: browser
            anchors.fill: parent

            Component.onCompleted: {
                try {
                    settings.javaScriptEnabled = true
                    settings.localStorageEnabled = true
                } catch (e) {}
            }

            onLoadingChanged: function(loadRequest) {
                if (loadRequest.status === WebView.LoadSucceededStatus)
                    settleTimer.restart()
                else if (loadRequest.status === WebView.LoadFailedStatus && root.requestId) {
                    webPlayback.resolveFailure(root.requestId, loadRequest.errorString || "悦听歌曲页加载失败")
                    root.finish()
                }
            }
        }
    }

    Timer { id: settleTimer; interval: 1200; repeat: false; onTriggered: root.startPageResolve() }
    Timer { id: pollTimer; interval: 350; repeat: true; onTriggered: root.pollResult() }
    Timer {
        id: resolveTimeout
        interval: 18000
        repeat: false
        onTriggered: {
            if (root.requestId) {
                webPlayback.resolveFailure(root.requestId, "悦听网页解析超时")
                root.finish()
            }
        }
    }

    Window {
        id: verifyWindow
        visible: root.verifyKeyword.length > 0
        width: 980
        height: 720
        title: "2t58 安全验证"
        flags: Qt.Window
        color: "#F4F5F7"

        Rectangle {
            anchors.fill: parent
            color: "#F4F5F7"
            Column {
                anchors.fill: parent
                spacing: 0
                Rectangle {
                    width: parent.width; height: 48; color: "#FFFFFF"
                    Text { anchors.centerIn: parent; text: "请在网页中完成 2t58 安全验证，完成后窗口会自动继续搜索"; color: "#33343A"; font.pixelSize: 13 }
                }
                WebView {
                    id: verifyBrowser
                    width: parent.width; height: parent.height - 48
                    Component.onCompleted: {
                        try { settings.javaScriptEnabled = true; settings.localStorageEnabled = true } catch (e) {}
                    }
                    onLoadingChanged: function(loadRequest) {
                        if (loadRequest.status === WebView.LoadSucceededStatus)
                            verifyTimer.restart()
                    }
                }
            }
        }

        onClosing: {
            webPlayback.verificationFailed("2t58 验证窗口已关闭")
            root.verifyKeyword = ""
        }
    }

    Timer {
        id: verifyTimer
        interval: 900
        repeat: true
        onTriggered: {
            if (!root.verifyKeyword.length) { stop(); return }
            verifyBrowser.runJavaScript(
                "(function(){try{return JSON.stringify({challenge:!!document.querySelector('#human_check'),title:document.title||'',cookie:document.cookie||''});}catch(e){return '{}';}})()",
                function(result) {
                    try {
                        var info = JSON.parse(String(result || "{}"))
                        if (!info.challenge && info.cookie && info.title !== "安全验证") {
                            verifyTimer.stop()
                            webPlayback.verificationCompleted(info.cookie)
                            root.verifyKeyword = ""
                        }
                    } catch (e) {}
                })
        }
    }

    Connections {
        target: webPlayback
        function onYuetingResolveRequested(id, song, isDownload) {
            root.finish()
            root.requestId = id
            root.songId = song
            root.download = isDownload
            browser.url = "https://www.yueting.net/tool/song/?song=" + encodeURIComponent(song)
        }
        function onTwoT58VerificationRequested(keyword) {
            root.verifyKeyword = keyword
            verifyBrowser.url = "https://www.2t58.com/so/" + encodeURIComponent(keyword) + ".html"
        }
    }

    Component.onCompleted: webPlayback.setRendererReady(true)
    Component.onDestruction: webPlayback.setRendererReady(false)
}
