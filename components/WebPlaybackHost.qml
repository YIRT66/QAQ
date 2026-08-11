import QtQuick
import QtQuick.Window
import QtWebView

// Website-backed fallback playback.  The site keeps ownership of its page and
// media element; EvolveMusic only drives the visible play/pause/seek controls
// through the page that the user would otherwise open manually.
//
// Keep a real native WebView surface alive.  WebView2 can throttle a 1x1 or
// fully hidden native view, so this window is normal-sized, nearly transparent
// and non-interactive while remaining attached to the desktop compositor.
Window {
    id: host
    width: 480
    height: 320
    // Keep the native WebView on-screen so WebView2 does not classify it as an
    // off-screen/occluded surface and suspend media initialization. It is
    // effectively invisible and ignores input.
    x: 0
    y: 0
    visible: player.webPlaybackActive
    opacity: 0.001
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowTransparentForInput
    color: "transparent"

    property int attempts: 0
    property int navigationSerial: 0
    property bool readyReported: false
    property string targetUrl: ""

    function pageHelpers() {
        return "function allRoots(){"
             + "let roots=[];let seen=new Set();"
             + "function add(r,d){if(!r||seen.has(r)||d>4)return;seen.add(r);roots.push(r);"
             + "try{r.querySelectorAll('iframe').forEach(f=>{try{add(f.contentDocument,d+1)}catch(e){}})}catch(e){}"
             + "try{r.querySelectorAll('*').forEach(e=>{if(e.shadowRoot)add(e.shadowRoot,d+1)})}catch(e){}"
             + "}add(document,0);return roots;}"
             + "function mediaList(){let out=[];allRoots().forEach(r=>{try{r.querySelectorAll('audio,video').forEach(m=>out.push(m))}catch(e){}});return out;}"
    }

    function playScript(volume) {
        return "(async function(){" + pageHelpers()
             + "const txt=(document.body&&document.body.innerText)||'';"
             + "if(/由于.{0,12}(内容|版权).{0,20}(删除|下架)|链接已删除|资源已删除/i.test(txt))return JSON.stringify({ok:false,reason:'removed'});"
             + "function label(e){return String((e.innerText||e.title||(e.getAttribute&&e.getAttribute('aria-label'))||(e.getAttribute&&e.getAttribute('data-title'))||'')).trim();}"
             + "function norm(s){return String(s||'').toLowerCase().replace(/[\\s\\p{P}\\p{S}]+/gu,'');}"
             + "function maybeOpenMatchedSong(){let h='';try{h=String(location.hash||'').replace(/^#/,'')}catch(e){}if(!h)return false;let q;try{q=new URLSearchParams(h)}catch(e){return false;}let title=q.get('evolve_title')||'',artist=q.get('evolve_artist')||'';if(!title)return false;let nt=norm(title),na=norm(artist),best=null,bestScore=-999;for(let r of allRoots()){let links=[];try{links=[...r.querySelectorAll('a[href*=\"/song/\"],a[href*=\"/play/\"]')]}catch(e){}for(let a of links){let text=label(a),n=norm(text),score=0;if(!n)continue;if(n===nt)score+=100;else if(n.startsWith(nt)||n.includes(nt))score+=76;else if(nt.includes(n))score+=48;else continue;if(na&&n.includes(na))score+=34;let low=text.toLowerCase();let explicitDj=/dj|remix|混音/i.test(title),explicitCover=/翻唱|cover/i.test(title);if(/伴奏|纯音乐|ktv|instrumental/i.test(low))score-=80;if(/串烧|medley/i.test(low))score-=55;if(/dj|remix|混音/i.test(low))score+=explicitDj?55:8;if(/翻唱|cover/i.test(low))score+=explicitCover?55:-18;if(score>bestScore){bestScore=score;best=a;}}}if(best&&bestScore>=55){try{location.href=best.href;return true}catch(e){}}return false;}"
             + "if(maybeOpenMatchedSong())return JSON.stringify({ok:false,reason:'navigating'});"
             + "async function playMedia(){let list=mediaList();for(let m of list){"
             + "try{m.preload='auto';m.muted=false;m.volume=Math.max(0,Math.min(1," + Number(volume) + "));m.playbackRate=Math.max(0.5,Math.min(2," + Number(player.playbackRate) + "));if(m.readyState===0&&m.load)m.load();}catch(e){}"
             + "try{let r=m.play();if(r&&r.then)await r;if(!m.paused)return {ok:true,reason:'media',count:list.length};}catch(e){}"
             + "}return {ok:false,reason:list.length?'play-blocked':'no-media',count:list.length};}"
             + "function globalPlay(){let names=['ap','aplayer','player','audioPlayer','musicPlayer','jp','plyr'];for(let n of names){try{let x=window[n];if(x&&typeof x.play==='function'){x.play();return true}if(x&&x.audio&&typeof x.audio.play==='function'){x.audio.play();return true}}catch(e){}}"
             + "try{if(window.jQuery){let a=window.jQuery('audio').get(0);if(a){a.play();return true}}}catch(e){}return false;}"
             + "let direct=await playMedia();if(direct.ok)return JSON.stringify(direct);"
             + "let gp=globalPlay();if(gp){await new Promise(r=>setTimeout(r,180));direct=await playMedia();if(direct.ok)return JSON.stringify(direct);}"
             + "let selectors=['audio','video','.aplayer-play','.aplayer-button','.jp-play','.plyr__control--overlaid','.mejs__play button','.play-btn','.btn-play','.player-play','.music-play','.audio-play','.play_music','#play','#playBtn','#btn-play','[data-action=play]','[data-action=\"play\"]','[onclick*=play]','button[class*=play]','a[class*=play]','[role=button]'];"
             + "let clicked=0;for(let r of allRoots()){for(let sel of selectors){let es=[];try{es=[...r.querySelectorAll(sel)]}catch(e){}for(let e of es){let t=label(e);if(/下载|download|歌词/i.test(t))continue;if(sel==='[role=button]'&&!/播放|试听|收听|play|listen/i.test(t))continue;try{e.dispatchEvent(new MouseEvent('mousedown',{bubbles:true,cancelable:true,view:window}));e.click();e.dispatchEvent(new MouseEvent('mouseup',{bubbles:true,cancelable:true,view:window}));clicked++;}catch(e){}if(clicked>=3)break;}if(clicked>=3)break;}if(clicked>=3)break;}"
             + "if(!clicked){for(let r of allRoots()){let es=[];try{es=[...r.querySelectorAll('button,a,[role=button],div,span')]}catch(e){}for(let e of es){let t=label(e);if(t.length>24||/下载|download|歌词/i.test(t))continue;if(/^(播放|试听|立即播放|在线试听|play|listen)$/i.test(t)||/播放歌曲|开始播放/i.test(t)){try{e.click();clicked++;break}catch(e){}}}if(clicked)break;}}"
             + "await new Promise(r=>setTimeout(r,260));globalPlay();let after=await playMedia();after.clicked=clicked;return JSON.stringify(after);"
             + "})()"
    }

    function commandScript(command, value) {
        var prefix = "(function(){" + pageHelpers()
                   + "var list=mediaList();if(!list.length)return 'no-media';var m=list.find(x=>!x.paused&&!x.ended)||list[0];"
        if (command === "toggle")
            prefix += "if(m.paused){m.play().catch(()=>{});}else{m.pause();}return 'ok';"
        else if (command === "play")
            prefix += "m.play().catch(()=>{});return 'ok';"
        else if (command === "pause")
            prefix += "m.pause();return 'ok';"
        else if (command === "stop")
            prefix += "m.pause();try{m.currentTime=0}catch(e){};return 'ok';"
        else if (command === "seek")
            prefix += "try{m.currentTime=Math.max(0," + (Number(value) / 1000.0) + ")}catch(e){};return 'ok';"
        else if (command === "volume")
            prefix += "try{m.volume=Math.max(0,Math.min(1," + Number(value) + "))}catch(e){};return 'ok';"
        else if (command === "rate")
            prefix += "try{m.playbackRate=Math.max(0.5,Math.min(2," + Number(value) + "))}catch(e){};return 'ok';"
        return prefix + "})()"
    }

    function runCommand(command, value) {
        if (!player.webPlaybackActive) return
        browser.runJavaScript(commandScript(command, value))
    }

    WebView {
        id: browser
        anchors.fill: parent

        Component.onCompleted: {
            try {
                settings.javaScriptEnabled = true
                settings.localStorageEnabled = true
            } catch (e) {}
        }

        onLoadingChanged: function(req) {
            if (!player.webPlaybackActive) return
            if (req.status === WebView.LoadStartedStatus) {
                retryPlay.restart()
            } else if (req.status === WebView.LoadSucceededStatus) {
                retryPlay.restart()
                poll.restart()
            } else if (req.status === WebView.LoadFailedStatus) {
                var serial = host.navigationSerial
                Qt.callLater(function() {
                    if (serial === host.navigationSerial && player.webPlaybackActive)
                        player.reportWebPlaybackFailure("网页播放源加载失败：" + (req.errorString || "未知错误"))
                })
            }
        }
    }

    Timer {
        id: retryPlay
        interval: 240
        repeat: false
        onTriggered: {
            if (!player.webPlaybackActive) return
            var serial = host.navigationSerial
            host.attempts++
            browser.runJavaScript(host.playScript(player.volume), function(result) {
                if (serial !== host.navigationSerial || !player.webPlaybackActive) return
                var state = {}
                try { state = JSON.parse(String(result || "{}")) } catch (e) {}
                if (state.ok === true) {
                    host.readyReported = true
                    poll.restart()
                    return
                }
                if (state.reason === "removed") {
                    player.reportWebPlaybackFailure("该网页源的这首歌已下架，正在切换下一个音源。")
                    return
                }
                if (state.reason === "navigating") {
                    // A browser-search fallback selected the matching public song
                    // page. onLoadingChanged will restart playback after navigation.
                    return
                }
                if (host.attempts < 28) {
                    retryPlay.restart()
                } else {
                    player.reportWebPlaybackFailure(
                        "网页源页面已打开，但站点播放器没有成功开始。已自动切换其他音源。")
                }
            })
        }
    }

    Timer {
        id: poll
        interval: 360
        repeat: true
        running: player.webPlaybackActive
        onTriggered: {
            var serial = host.navigationSerial
            var script = "(function(){" + host.pageHelpers()
                       + "var list=mediaList();if(!list.length)return JSON.stringify({ready:false});"
                       + "var m=list.find(x=>!x.paused&&!x.ended)||list[0];"
                       + "return JSON.stringify({ready:true,playing:!m.paused&&!m.ended,pos:Math.round((m.currentTime||0)*1000),dur:Math.round((isFinite(m.duration)?m.duration:0)*1000),ended:!!m.ended});})()"
            browser.runJavaScript(script, function(result) {
                if (serial !== host.navigationSerial || !player.webPlaybackActive) return
                try {
                    var s = JSON.parse(String(result || "{}"))
                    player.reportWebPlaybackState(!!s.ready, !!s.playing,
                                                  Number(s.pos || 0), Number(s.dur || 0), !!s.ended)
                } catch (e) {}
            })
        }
    }

    Connections {
        target: player
        function onWebPlaybackRequested(url, volume) {
            host.navigationSerial++
            host.attempts = 0
            host.readyReported = false
            host.targetUrl = String(url || "")
            // QtWebView may not emit a fresh load when the same song page is
            // retried after another provider. Force a tiny blank navigation so
            // the website's player scripts are initialized again.
            if (String(browser.url) === host.targetUrl) {
                browser.url = "about:blank"
                Qt.callLater(function(){ browser.url = host.targetUrl })
            } else {
                browser.url = host.targetUrl
            }
            retryPlay.restart()
        }
        function onWebPlaybackCommand(command, value) {
            host.runCommand(command, value)
        }
    }
}
