import QtQuick
import QtQuick.Window
import QtWebView

Item {
    id: root
    width: 1
    height: 1

    property int requestId: -1
    property string providerId: ""
    property string purpose: ""
    property string mode: "capture"
    property string keyword: ""
    property int settleDelay: 850
    property bool formSubmitted: false
    property bool finishing: false

    function jsString(value) {
        return JSON.stringify(String(value || ""))
    }

    function captureDom() {
        if (root.requestId < 0 || root.finishing)
            return

        root.finishing = true

        var script = ""

        if (root.mode === "song-links") {
            // Return a tiny synthetic HTML page containing every browser-visible
            // song link. This deliberately avoids depending on the site's full
            // HTML structure/classes and works for both static and JS-rendered
            // search results.
            script =
                "(function(){" +
                "try{" +
                "var rows=[];" +
                "var seen=new Set();" +
                "var add=function(raw,text,title){" +
                " try{" +
                "  if(!raw)return;" +
                "  var u=new URL(raw,location.href);" +
                "  var m=u.pathname.match(/\\/song\\/([A-Za-z0-9_-]+)(?:\\.html)?(?:\\/)?$/i);" +
                "  if(!m)return;" +
                "  var key=u.origin+u.pathname;" +
                "  if(seen.has(key))return;" +
                "  seen.add(key);" +
                "  var label=(text||title||'').replace(/\\s+/g,' ').trim();" +
                "  var esc=function(v){return String(v).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/\"/g,'&quot;');};" +
                "  rows.push('<a href=\"'+esc(u.href)+'\" title=\"'+esc(title||label)+'\">'+esc(label)+'</a>');" +
                " }catch(e){}" +
                "};" +
                "document.querySelectorAll('a[href]').forEach(function(a){" +
                " add(a.getAttribute('href'),a.innerText||a.textContent||'',a.getAttribute('title')||a.getAttribute('aria-label')||'');" +
                "});" +
                "document.querySelectorAll('[data-href],[data-url],[onclick]').forEach(function(e){" +
                " ['data-href','data-url'].forEach(function(k){var v=e.getAttribute(k);if(v)add(v,e.innerText||e.textContent||'',e.getAttribute('title')||'');});" +
                " var oc=e.getAttribute('onclick')||'';" +
                " var mm=oc.match(/(?:https?:\\/\\/[^\\s\"']+)?\\/song\\/[A-Za-z0-9_-]+(?:\\.html)?/i);" +
                " if(mm)add(mm[0],e.innerText||e.textContent||'',e.getAttribute('title')||'');" +
                "});" +
                "return '<html><body>'+rows.join('\\n')+'</body></html>';" +
                "}catch(e){return '<html><body></body></html>';}" +
                "})()"
        } else {
            script =
                "(function(){" +
                "try{" +
                "var urls=[];" +
                "document.querySelectorAll('audio,source').forEach(function(e){" +
                " var s=e.currentSrc||e.src||e.getAttribute('src')||'';" +
                " if(s)urls.push(s);" +
                "});" +
                "try{" +
                " performance.getEntriesByType('resource').forEach(function(e){" +
                "  var u=e.name||'';" +
                "  if(/\\.(mp3|m4a|aac|flac|ogg|opus|wav)(\\?|$)/i.test(u)||/(play|player|audio|music)/i.test(u)) urls.push(u);" +
                " });" +
                "}catch(_e){}" +
                "var marker=document.getElementById('evolve-audio-candidates');" +
                "if(!marker){marker=document.createElement('meta');marker.id='evolve-audio-candidates';marker.name='evolve-audio-candidates';document.head.appendChild(marker);}" +
                "marker.content=Array.from(new Set(urls)).join('|');" +
                "return document.documentElement ? document.documentElement.outerHTML : '';" +
                "}catch(e){return document.documentElement ? document.documentElement.outerHTML : '';}" +
                "})()"
        }

        browser.runJavaScript(script, function(result) {
            if (root.requestId < 0)
                return

            var id = root.requestId
            root.requestId = -1
            root.finishing = false
            webScraper.complete(id,
                                result === undefined || result === null ? "" : String(result),
                                browser.url ? browser.url.toString() : "")
        })
    }

    function submitSearchForm() {
        if (root.requestId < 0 || root.formSubmitted)
            return

        var q = root.jsString(root.keyword)

        var script =
            "(function(q){" +
            "try{" +
            "var all=Array.from(document.querySelectorAll('input,textarea'));" +
            "var score=function(e){" +
            " var s=((e.name||'')+' '+(e.id||'')+' '+(e.placeholder||'')+' '+(e.type||'')).toLowerCase();" +
            " var n=0;" +
            " if(/search|keyword|query|\\bq\\b|\\bkey\\b|wd|word/.test(s)) n+=20;" +
            " if((e.type||'').toLowerCase()==='search') n+=15;" +
            " if((e.type||'').toLowerCase()==='text') n+=5;" +
            " if(e.offsetParent!==null) n+=3;" +
            " return n;" +
            "};" +
            "all.sort(function(a,b){return score(b)-score(a)});" +
            "var input=all.find(function(e){return score(e)>0});" +
            "if(!input) return JSON.stringify({ok:false,reason:'no-search-input'});" +
            "input.focus();" +
            "var setter=Object.getOwnPropertyDescriptor(window.HTMLInputElement&&HTMLInputElement.prototype,'value');" +
            "if(setter&&setter.set) setter.set.call(input,q); else input.value=q;" +
            "input.dispatchEvent(new Event('input',{bubbles:true}));" +
            "input.dispatchEvent(new Event('change',{bubbles:true}));" +
            "var form=input.form||input.closest('form');" +
            "if(form){" +
            " if(form.requestSubmit){form.requestSubmit();}" +
            " else{form.submit();}" +
            " return JSON.stringify({ok:true,method:'form'});" +
            "}" +
            "var buttons=Array.from(document.querySelectorAll('button,input[type=submit],a'));" +
            "var btn=buttons.find(function(b){" +
            " var t=((b.innerText||b.value||b.title||'')+' '+(b.className||'')).toLowerCase();" +
            " return /搜索|search|搜一下|查找/.test(t);" +
            "});" +
            "if(btn){btn.click();return JSON.stringify({ok:true,method:'button'});}" +
            "input.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',code:'Enter',keyCode:13,which:13,bubbles:true}));" +
            "input.dispatchEvent(new KeyboardEvent('keyup',{key:'Enter',code:'Enter',keyCode:13,which:13,bubbles:true}));" +
            "return JSON.stringify({ok:true,method:'enter'});" +
            "}catch(e){return JSON.stringify({ok:false,reason:String(e)});}" +
            "})(" + q + ")"

        browser.runJavaScript(script, function(result) {
            if (root.requestId < 0)
                return

            var ok = false
            try {
                var parsed = JSON.parse(String(result || "{}"))
                ok = !!parsed.ok
            } catch (e) {}

            if (!ok) {
                // Even if no obvious form was detected, return the rendered
                // page so the provider can still inspect it.
                settleTimer.interval = Math.max(250, root.settleDelay)
                settleTimer.restart()
                return
            }

            root.formSubmitted = true

            // POST/navigation forms trigger another loadingChanged. AJAX forms
            // don't, so keep a fallback capture timer as well.
            postSubmitTimer.interval = Math.max(650, root.settleDelay + 350)
            postSubmitTimer.restart()
        })
    }

    Window {
        id: renderWindow
        visible: true
        x: -30000
        y: -30000
        width: 1100
        height: 780
        flags: Qt.Tool | Qt.FramelessWindowHint
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
                if (root.requestId < 0)
                    return

                if (loadRequest.status === WebView.LoadSucceededStatus) {
                    if (root.mode === "form-search" && !root.formSubmitted) {
                        initialSettleTimer.interval = Math.max(250, Math.floor(root.settleDelay / 2))
                        initialSettleTimer.restart()
                    } else {
                        settleTimer.interval = root.settleDelay
                        settleTimer.restart()
                    }
                } else if (loadRequest.status === WebView.LoadFailedStatus) {
                    var id = root.requestId
                    root.requestId = -1
                    root.finishing = false
                    webScraper.fail(id, loadRequest.errorString || "WebView2 页面加载失败")
                }
            }
        }
    }

    Timer {
        id: initialSettleTimer
        repeat: false
        onTriggered: root.submitSearchForm()
    }

    Timer {
        id: postSubmitTimer
        repeat: false
        onTriggered: root.captureDom()
    }

    Timer {
        id: settleTimer
        repeat: false
        onTriggered: root.captureDom()
    }

    Connections {
        target: webScraper

        function onRenderRequested(id, providerId, purpose, url, mode, keyword, settleMs) {
            root.requestId = id
            root.providerId = providerId
            root.purpose = purpose
            root.mode = mode || "capture"
            root.keyword = keyword || ""
            root.settleDelay = settleMs
            root.formSubmitted = false
            root.finishing = false

            initialSettleTimer.stop()
            postSubmitTimer.stop()
            settleTimer.stop()

            var target = url ? url.toString() : ""
            if (browser.url && browser.url.toString() === target)
                browser.reload()
            else
                browser.url = url
        }
    }

    Component.onCompleted: webScraper.setRendererReady(true)
    Component.onDestruction: webScraper.setRendererReady(false)
}
