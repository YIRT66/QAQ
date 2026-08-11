# EvolveMusic Cloud v0.10.8

Cloud-first backend used by **EvolveMusic v0.12.3**.

## Current source of truth

All three deployment copies are synchronized to v0.10.8:

- `dashboard/worker.js` - paste/deploy from Cloudflare Dashboard.
- `src/index.ts` - Wrangler deployment source.
- `worker-dashboard-v0.10.8.js` - current versioned Dashboard snapshot.
- `schema.sql` - D1 schema.

The bundled desktop build defaults to:

```text
https://evolvemusic-cloud.18048369193.workers.dev
```

## Cloud path

- Search -> Worker (`/v1/search`)
- Stream ticket -> Worker (`/v1/stream-ticket`)
- Actual audio -> signed Worker URL (`/v1/audio`)
- Favorites/history/settings -> D1 (`/v1/me/state`)
- Each cloud user's own NetEase provider session -> encrypted D1 row
- Authorized/licensed catalog audio -> optional R2 binding (`AUDIO`)

## v0.10.8 content + playback recovery

The stream-ticket endpoint resolves a NetEase CDN URL once and places it inside
its signed ticket. QMediaPlayer may then make multiple byte-range requests
without forcing the Worker to resolve `player/url` again for every range.

The `/v1/audio` proxy now forwards Range requests with identity encoding and
retries a failed/non-audio NetEase response once with minimal headers. Old
v0.10.6 tickets are still accepted; their upstream URL is resolved lazily.

The desktop v0.12.3 client probes only the first 64 KiB before starting normal
media playback, rather than downloading an entire song into cache first.

## Dashboard deployment

Open the existing Worker, replace its code with `dashboard/worker.js`, then
Deploy. Keep the existing bindings and secrets:

```text
D1 binding: DB -> evolvemusic-db
Secrets: SESSION_MASTER_KEY, STREAM_SIGNING_KEY, ADMIN_KEY
Optional R2 binding: AUDIO
Optional variable: NETEASE_API_ORIGIN
```

After deployment, `/health` should report `0.10.8`.

## Wrangler deployment (optional)

Configure the real D1/R2 IDs in `wrangler.jsonc` before deploying:

```bat
npm install
npx wrangler deploy
```

`NETEASE_API_ORIGIN` remains optional.


### v0.10.8 recovery changes

- Home discovery uses the same multi-attempt search path as normal search instead of one fragile GET endpoint.
- Search/home listings no longer disappear just because the player URL probe is temporarily unavailable.
- `/v1/artwork` proxies only approved NetEase artwork hosts and caches resized covers at the Cloudflare edge.
- Lyrics try gateway `/lyric/new`, `/lyric`, direct GET/POST, then the legacy media endpoint.
- Audio resolution keeps the CDN scheme returned by NetEase and adds the official outer-url resolver as a last fallback.
- Desktop playback receives both Worker and direct CDN candidates and switches automatically when one route fails or stalls.
