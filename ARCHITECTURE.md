# Evolve Music v0.3 Architecture

```text
QML / EvolveUI
├─ Home / Search / Playlist / Library / Settings
├─ Immersive NowPlaying
├─ MiniPlayer / Queue
└─ Embedded Login Browser (optional Qt WebView / Edge WebView2)
            │
            ▼
AppController
├─ search cache / view models / favorites / history / lyrics
├─ cover URL normalization
├─ PlayerController
├─ MultiSourceManager
└─ AccountManager
       ├─ provider account state
       ├─ web-session handoff
       └─ Windows DPAPI session persistence
            │
            ▼
IMusicProvider
├─ NeteaseProvider ── local api-enhanced runtime
├─ JsonGatewayProvider(qq) ── authorized/configured gateway
└─ JsonGatewayProvider(kugou) ── authorized/configured gateway

QML network images
└─ CachedNetworkAccessManagerFactory
   └─ QNetworkDiskCache -> runtime/client-data/cache
```

## Core boundaries

### `PlayerController`
Owns `QMediaPlayer`, `QAudioOutput`, queue position, playback mode, seek and volume. It never calls platform APIs.

### `MultiSourceManager`
Owns provider health, merged logical tracks, source ordering and fallback. A logical track contains multiple source variants; the queue is platform-independent.

Only `enabled && online` providers enter search/home/resolution. External gateways are offline until `/ping` succeeds. Search results are surfaced progressively so a healthy fast provider can populate the UI before other healthy providers finish.

### `AccountManager`
Owns account presentation, the browser-login handoff, Provider validation and saved-session lifecycle. On Windows, extracted session snapshots are encrypted using DPAPI before persistence. Raw cookies/tokens are not included in ordinary UI status or toast text.

The native browser itself is QML `QtWebView.WebView`. On Windows Qt 6.10 it uses Edge WebView2. The browser page collects only information exposed to JavaScript by the provider page (`document.cookie`, localStorage and sessionStorage) and sends that snapshot into C++; the Provider must still validate it.

### Providers
Providers are the only layer that knows platform/gateway request formats. They decide whether the current account can obtain a stream. The source router does not fabricate playback rights or bypass entitlement checks.

### Image network layer
`CachedNetworkAccessManagerFactory` is installed on `QQmlApplicationEngine`. QML images share a disk HTTP cache. `CoverImage.qml` requests appropriately sized artwork and normalizes NetEase artwork to HTTPS.

## Optional Qt WebView

CMake probes `Qt6::WebView`. If found, `EVOLVEMUSIC_HAS_WEBVIEW=1` is defined and `LoginBrowserPage.qml` is compiled into the QML module. If absent, the rest of the application builds unchanged and `LoginBrowserFallback.qml` explains how to add Qt WebView to the current kit.

This deliberately avoids Qt WebEngine for the user's MinGW toolchain.


## v0.8: dedicated web-provider layer

```
IMusicProvider
├─ NeteaseProvider
├─ JsonGatewayProvider (QQ / Kugou gateway contract)
├─ YuetingProvider
└─ TwoT58Provider

WebProviderUtils
├─ HTML/entity decoding
├─ absolute URL resolution
├─ audio URL extraction
└─ canonical title/artist match scoring
```

`MultiSourceManager::resolveBestStream()` now has two stages:

1. Resolve the source IDs already attached to the canonical track.
2. If those fail, call `resolveTrack()` on providers that advertise
   `supportsOnDemandResolve()`.

This decouples playback fallback from the reliability of a provider's initial
search-result merge.
