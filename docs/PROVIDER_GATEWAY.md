# Evolve Music Provider Gateway Contract (v0.3)

Evolve Music uses a normalized JSON gateway for platforms whose official/licensed backend is not bundled into the desktop client.
The desktop application does **not** hard-code private playback endpoints and does not bypass subscriptions, DRM, regions, or account entitlements.

## Providers in v0.3

| Provider | ID | Default URL | Runtime behavior |
|---|---|---:|---|
| 网易云音乐 | `netease` | `http://127.0.0.1:3000` | bundled local compatibility provider |
| QQ音乐 | `qq` | `http://127.0.0.1:3200` | external gateway; skipped until `/ping` succeeds |
| 酷狗音乐 | `kugou` | `http://127.0.0.1:3300` | external gateway; skipped until `/ping` succeeds |

A gateway that is offline is not included in search/home/playback hot paths. This avoids making every search wait for dead endpoints.

## Access hint

Tracks may expose `access`:

- `free`: no paid subscription required for the returned playback right.
- `account`: the current account must be logged in, but no paid membership is required.
- `unknown`: the gateway cannot know until the stream is resolved.
- `restricted`: region/account/catalog restrictions may apply.
- `vip`: a paid entitlement may be required.

Default routing order is `free -> account -> unknown -> restricted -> vip`, then provider priority.
A URL is considered usable only when the provider/gateway actually returns one for the current account.

## Health

### `GET /ping`

```json
{ "ok": true, "message": "QQ音乐连接正常" }
```

## Browser-session authentication

v0.3 can open the platform's official website inside Qt WebView. On Windows with Qt 6.10 this uses Edge WebView2. When the user presses **完成登录**, the app captures the web-visible session snapshot for that origin and transfers it to the matching Provider.

The snapshot may contain `document.cookie`, `localStorage`, and `sessionStorage`. Browser-only/HttpOnly values are not fabricated or bypassed. If a platform needs credentials that Qt WebView's public QML surface cannot expose, the authorized gateway should implement a supported login/OAuth/QR flow and report the resulting account state through this contract.

For JSON gateways, implement the following endpoints.

### `POST /auth/import`

Request:

```json
{
  "cookie": "the user's own web-visible cookie header",
  "userAgent": "browser user agent",
  "storage": "{\"localStorage\":{...},\"sessionStorage\":{...}}",
  "pageUrl": "https://platform.example/..."
}
```

Response:

```json
{
  "loggedIn": true,
  "displayName": "用户昵称",
  "message": "QQ音乐账号已同步"
}
```

The gateway is responsible for validating the session with the platform it is authorized to integrate with. Importing a browser session does not create playback rights that the user does not already have.

For security, Evolve Music only sends sensitive browser-session data to an HTTPS gateway or a loopback address (`localhost`, `127.0.0.1`, `::1`).

### `GET /auth/status`

```json
{
  "loggedIn": true,
  "displayName": "用户昵称",
  "message": "登录有效"
}
```

### `POST /auth/logout`

```json
{ "ok": true }
```

The gateway should clear the session it imported for this local/single-user client session.

## Catalog

### `GET /search?keywords=...&limit=40`

```json
{
  "tracks": [
    {
      "id": "platform-song-id",
      "title": "歌曲名",
      "artist": "歌手",
      "album": "专辑",
      "cover": "https://...",
      "duration": 243000,
      "access": "free",
      "playable": true
    }
  ]
}
```

### `GET /home?limit=14`

```json
{
  "playlists": [
    {
      "id": "playlist-id",
      "name": "歌单名",
      "cover": "https://...",
      "description": "...",
      "playCount": 120000,
      "trackCount": 50
    }
  ]
}
```

### `GET /playlist?id=...&limit=300`

```json
{
  "playlist": {
    "id": "playlist-id",
    "name": "歌单名",
    "cover": "https://...",
    "description": "...",
    "playCount": 120000,
    "trackCount": 50
  },
  "tracks": []
}
```

`tracks` uses the `/search` schema.

## Playback

### `GET /stream?id=...&quality=exhigh`

When the current user is entitled to play the track:

```json
{ "url": "https://authorized-cdn.example/audio/..." }
```

When it cannot legally play it:

```json
{ "url": "" }
```

Evolve Music then moves to the next source candidate.

### `GET /download?id=...&quality=exhigh`

Only return a URL when the platform/account explicitly allows the download.

```json
{ "url": "https://authorized-cdn.example/download/..." }
```

## Lyrics

### `GET /lyrics?id=...`

```json
{
  "lyric": "[00:01.000]...",
  "translatedLyric": "[00:01.000]..."
}
```

## Matching

The client merges results by normalized title + artist and accepts a duration difference of up to four seconds. The merged logical track contains `sources[]`; playback queues logical tracks rather than platform-specific IDs.


## Session persistence

On Windows, Evolve Music protects the extracted Provider session snapshot with Windows DPAPI before writing it to `runtime/client-data/sessions`. The encrypted record is bound to the current Windows user. The gateway must still validate the session on restore.

The browser/login bridge is not a mechanism for bypassing paid entitlements. A gateway should return only streams the current account is legitimately entitled to receive.
