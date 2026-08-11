# v0.11.0 - Cloud Playback Client

- Promoted the validated Cloudflare Worker to the default desktop playback path.
- Desktop no longer starts the local NetEase Node service in normal cloud mode.
- Added bootstrap action queue, stale-request/profile generation protection, and stale-search protection.
- Added cloud-mode search caching and stronger per-profile cloud state isolation.
- Bound saved cloud auth tokens to the Worker base URL.
- Added cloud connection/user status in Settings and cloud-aware search status.
- Preserved backend playability metadata (`playable`, `bitrate`, `audioType`) in favorites/history.
- Blocked known-unplayable cloud tracks before ticket creation and skip them in cloud playback queues.
- Bundled the validated v0.10.5 Dashboard Worker snapshot and synchronized Wrangler source to it.

# EvolveMusic Changelog

## v0.10.0
- Added Cloud-first desktop mode: search, home, playlists, lyrics, stream resolution and actual audio delivery can all go through the configured Cloudflare Worker.
- Added `CloudMusicClient` with per-profile opaque cloud identity.
- Added D1-backed favorites, history and music-setting sync.
- Added per-user encrypted NetEase provider-session storage; a user's own NetEase session can only be used for that same cloud user.
- Added signed stream tickets and `/v1/audio` byte streaming.
- Added R2 audio-library streaming with byte-range support for audio you are authorized to distribute.
- Added Cloudflare D1/R2 Worker project under `cloudflare/evolvemusic-cloud`.
- Cloud mode disables desktop music downloading and is playback-only.
- Preserved the Windows linker lock fix that closes a running `EvolveMusic.exe` before linking.


## v0.9.1
- Locked NetEase to the highest provider priority.
- Playback now sorts provider priority before access class, so a local profile's own NetEase entitlements are attempted before fallback sources.
- Higher-priority NetEase metadata is promoted when merged search sources arrive asynchronously.
- Added an optional stateless Cloudflare Worker config API; provider credentials and personal library data never leave the device.
- Added `scripts/set_cloud_api.bat`.
- Fixed Windows linker `Permission denied` by closing a running `EvolveMusic.exe` before the build link step.


## v0.9.0
- Added local user profiles with isolated favorites, playback history, appearance, quality, and encrypted provider sessions.
- Added a Settings > 本地用户 page for creating, switching, and deleting local profiles.
- Profile switching clears the current playback queue before loading the next user's data.
- NetEase authenticated session is now attached only to account validation and stream-URL resolution.
- NetEase search/home/playlist/song metadata/lyrics remain anonymous.
- NetEase authenticated session is never used for download URL resolution.
- Existing v0.8.x personal data/session files migrate into the default local profile.
- Preserved the v0.8.8 Yueting/2t58 provider implementation.


## v0.8.8
- Aligned 2t58 search requests with the current maintained TwoT58MusicClient browser headers and site/session cookie behavior.
- Added runtime 2t58 cookie overrides via `EVOLVE_2T58_COOKIE` or `runtime/client-data/2t58-cookie.txt`.
- Extended curl HTTP fallback to accept provider-specific headers.
- Broadened public-index queries from `site:domain/path` to `site:domain`.
- Changed diagnostics to use a known-existing Yueting title (`晴天`) and `差一步` for 2t58.


## v0.8.7
- Added a public search-index discovery fallback when 悦听/2t58 return HTTP 200 search pages with zero `/song/...` links.
- The index is used only to discover the target site's own `/song/<id>` URL; playback still comes from the selected target provider.
- Added 2t58 session warm-up and browser-like navigation request headers.
- Switched the 2t58 mirror fallback to `http://music.2t58.com` for deployments without HTTPS on that host.
- Added 2t58 direct public media endpoint probing for `flac`, `wav`, then `320`, accepting only actual media responses.
- Updated diagnostics to compare direct-site search vs public-index discovery.


## v0.8.6
- Removed WebView/WebView2 from 悦听 and 2t58 search/playback lookup; WebView remains only for account login.
- Added asynchronous plain-HTTP curl fallback alongside QNetworkAccessManager.
- Added 5-minute per-provider search cache and concurrent `www`/`music` 2t58 requests.
- Added UTF-8/GBK/GB2312/GB18030 page decoding and more tolerant `/song/...` extraction from HTML/JS/JSON.
- Reduced visible multi-source search deadline to 650 ms while keeping late-source enrichment.
- Added HTTP-only web-source diagnostics and an upgrade cleanup script.


## v0.8.5
- Force-synchronized YuetingProvider and TwoT58Provider headers/implementations to prevent mixed v0.8.x incremental-overlay builds.
- Added a provider source verification script that detects stale callback signatures, missing pending-request structs, and obsolete browser fallback symbols.
- Kept visible UI, player thread, and v0.8.4 web-search behavior unchanged.


## v0.8.4
- Changed 悦听 browser fallback to the explicit `/so?nsid=4&q=...` music-search route.
- Changed 2t58 browser fallback to explicit `/so/<keyword>.html` routes on both `www` and `music` hosts.
- Added a WebView2 `song-links` extraction mode that returns only rendered `/song/...` links instead of relying on full-page HTML layout.
- Added `runtime/client-data/web-sources.log` diagnostics.
- Kept visible UI and player/audio threading unchanged.


## v0.8.3
- Fixed v0.8.2 compilation errors in YuetingProvider / TwoT58Provider.
- Added the missing WebScrapeBridge forward declaration.
- Corrected `htmlDecode` to `decodeHtml`.
- Removed all remaining BrowserDomFetcher / searchWithBrowser references.
- Routed song-page fallback and provider health checks through the in-app WebView2 bridge.
- Fixed providerId lambda-name shadowing.


## v0.8.2
- Kept the visible UI and player thread unchanged.
- Replaced external headless-browser fallback with an off-screen in-app Qt WebView2 renderer for 悦听/2t58.
- WebView2 now submits each site's own search form instead of guessing multiple search URL variants.
- Added `www.2t58.com` -> `music.2t58.com` browser fallback.
- Relaxed song-link parsing to accept `/song/<id>` and `/song/<id>.html`, plus `title`/`aria-label` labels.
- Added a ~950 ms visible search deadline: fast results show immediately while slow web sources enrich in the background.
- Added stale-search generation guards for both public webpage providers.


## v0.8.1
- Added asynchronous Edge/Chrome DOM fallback for 悦听 and 2t58 only.
- Raw QNetwork parsing remains the fast path; browser rendering is used only when it fails.
- 悦听 search/song MP3 resolution can now parse the browser-rendered page.
- 2t58 search/player resolution can now parse post-JavaScript DOM.
- Reworked the two provider health checks to test actual search capability.
- Expanded 2t58 JSON/JavaScript audio URL extraction.
- Updated web-source diagnostic script to compare raw HTTP vs browser DOM.


## v0.7.2
- Fixed loaded playlist tracks being invisible because the playlist header consumed all remaining vertical layout space.
- Removed the two `Layout.fillHeight` header spacers.
- Made the song list area the sole vertically expanding child.
- Added a minimum song-list height and vertical scrollbar.
- Playlist empty/loading state now keys off `ListView.count`.


## v0.7.1
- Fixed MinGW compile failure in `PublicWebProvider::probeCandidateLinks()` caused by C++ most-vexing-parse.
- Switched `QNetworkRequest` construction to brace initialization.
- Updated numeric HTML entity decoding to Qt 6.10's `char32_t` `QString::fromUcs4()` overload.


## v0.7.0
- Added built-in 悦听音乐 (`yueting.net`) public web provider.
- Added built-in 爱听音乐 2t58 (`2t58.com`) public web provider.
- Added HTML search-result parsing and cross-provider track merging.
- Added public audio-link extraction / fallback resolution.
- Added Yueting timestamped lyric extraction and 2t58 LRC-link support.
- Public web providers are marked `free`, so they automatically outrank restricted/VIP candidates while remaining below free NetEase by default priority.
- Provider logic deliberately ignores password/paywall/DRM-protected routes.


## v0.6.2
- Fixed playlist pages showing metadata/track count but an empty song list.
- Added progressive playlist tracks from `/playlist/detail`.
- Added tolerant `/playlist/track/all` response parsing.
- Added `trackIds -> /song/detail` fallback.
- Added playlist loading/empty state and a retry button.


## v0.6.1
- Disabled build-time QML cachegen/AOT by default to avoid repeated MinGW 13.1 internal compiler crashes in generated EvolveUI `*_qml.cpp` files.
- Build script automatically discards old AOT-enabled CMake caches.
- Redirected runtime QML disk cache beside the application to avoid consuming the system drive.
- Kept single-job and `-O1` low-memory MinGW build settings.


## v0.6.0
- Moved QMediaPlayer and QAudioOutput off the GUI thread into a dedicated audio thread.
- Forced the Qt FFmpeg multimedia backend by default on Windows unless the user explicitly overrides QT_MEDIA_BACKEND.
- Added generation guards so stale media callbacks cannot mutate the current track.
- Added a 9-second audio-switch watchdog; a slow backend no longer makes the whole window look dead.
- Throttled position updates before crossing the audio-thread/UI-thread boundary.
- Deferred old-media teardown/new-media setup into separate audio-thread event-loop turns.
- Added stale stream/lyrics response guards.
- Added tiny/compact/normal/wide responsive layout rules using both window width and height.
- Reduced minimum window size to 640x480 and made the mini player/queue fit narrow windows.


## v0.4.0
- Reworked the entire desktop visual system to a denser, more product-like music-client layout.
- Added animated favorite feedback, button press/hover states, queue transitions and a vertical volume popover.
- Added configurable accent colors, animation toggle, compact track rows and source-badge visibility.
- Rebuilt discovery/home, playlist, search, library, settings, sidebar, title bar and mini-player layouts.
- Added lightweight new-song discovery content from the bundled NetEase source while keeping playback routed through the multi-source resolver.
- Reduced the default MinGW build jobs to one for reliability on the current machine.


## v0.3.3
- Fixed NetEase QR login getting stuck after the phone reports successful login.
- Aligned QR creation/check parameters with api-enhanced's official qrlogin.html.
- Added automatic `noCookie=true` fallback for post-scan code 502.
- Added tolerant cookie/status response parsing and visible unexpected status codes.


## v0.3.2
- Replaced NetEase WebView HTML login with a native QML QR login driven by AccountManager.
- Added native QR key/create/check polling and automatic account verification.
- QR images are saved as local PNG files before display for maximum Qt compatibility.
- NetEase QR login no longer requires Qt WebView; WebView remains for providers that require web authorization.
- Updated account UI labels and login-state handling.

# Changelog

## v0.3.0

### Player UI
- Rebuilt the bottom MiniPlayer around the supplied compact reference: small cover/title block, centered favorite/previous/play/next/mode controls, volume/lyrics/queue actions, and a thin edge progress bar.
- Rebuilt the Now Playing page as an immersive warm-dark view: large artwork on the left, focused scrolling lyrics on the right, subdued surrounding lyrics, turquoise current-line marker, and integrated frameless window controls.
- Normal navigation chrome is hidden while Now Playing is open; Escape/down returns to the previous page.

### Accounts / embedded browser
- Added `AccountManager`.
- Switched the Windows embedded browser design from Qt WebEngine to **Qt WebView / Edge WebView2**, which is compatible with the current Qt 6.10 MinGW build.
- Added in-app web login entry points for NetEase, QQ Music and KuGou.
- `LoginBrowserPage.qml` captures the web-visible session snapshot when the user presses “完成登录” and hands it only to the matching Provider.
- Added Windows DPAPI encryption for saved extracted provider sessions.
- Sensitive sessions are only sent to HTTPS gateways or loopback endpoints.
- NetEase validates imported web state with `/login/status`.
- JSON gateway auth contract includes `/auth/import`, `/auth/status`, and `/auth/logout`.
- QQ/KuGou still require a real authorized/configured gateway; the login UI does not pretend an unavailable gateway can resolve audio.

### Performance / media artwork
- Added QML `QNetworkDiskCache` stored under `runtime/client-data/cache` (256 MB maximum), plus desktop image request headers for more reliable CDN artwork loading.
- Added reusable `CoverImage.qml` with async loading, placeholders, fade-in, and requested thumbnail sizing.
- NetEase cover URLs are normalized to HTTPS and use server-side image resizing.
- Added a 320 ms search debounce, a five-minute query cache, ListView reuse/cache buffers, progressive search results, and cancellation of stale in-flight searches.
- External gateways must pass health checks before entering search or playback hot paths.

### Source routing
- Provider UI distinguishes enabled, online, usable and authenticated states.
- Routing preference remains `free -> account -> unknown -> restricted -> vip`, then provider priority. Account/VIP candidates are skipped until that Provider has a validated login session.
- The bundled NetEase runtime starts with upstream general-unblock disabled.

### Build
- Qt WebView is optional at configure time.
- Added `scripts/check_webview.bat`.
- Kept `check_webengine.bat` only as a redirect so old instructions do not break.
- Build diagnostics now report whether Qt WebView / WebView2 login is enabled.
