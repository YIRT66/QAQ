# Multi-source playback logic

## Data model

A merged track is represented roughly as:

```json
{
  "id": "canonical-id",
  "title": "Song",
  "artist": "Artist",
  "duration": 200000,
  "sourceCount": 3,
  "sourceSummary": "QQ音乐 · 网易云音乐 · 酷狗音乐",
  "sources": [
    {"providerId":"qq","sourceId":"...","access":"free","playable":true},
    {"providerId":"netease","sourceId":"...","access":"restricted","playable":true},
    {"providerId":"kugou","sourceId":"...","access":"unknown","playable":true}
  ]
}
```

The queue stores the merged track, not a platform-specific track. That lets the player change source without changing the song or rebuilding the queue.

## Resolution pipeline

```text
User selects track
       |
PlayerController::streamRequested(track)
       |
MultiSourceManager::resolveBestStream(track)
       |
Sort candidates by access class + provider priority
       |
Try candidate #1
  | URL returned -> play and expose active source in UI
  | empty/error   -> candidate #2
       |
Continue until one succeeds or all sources are exhausted
```

## Important separation

- `PlayerController`: audio state, queue, seek, next/previous, modes.
- `MultiSourceManager`: deduplication, provider registry, fallback, source policy.
- `IMusicProvider`: one platform/authorized backend adapter.
- `AppController`: UI-facing state, favorites/history/settings/download orchestration.
- QML: presentation and user interaction only.

## Future extensions

The current architecture is ready for:

- per-provider login state and entitlement badges;
- explicit "play from this platform" source picker;
- source health score and latency score;
- bitrate/codec-aware routing (e.g. prefer free FLAC over free MP3);
- local-library candidate matching;
- cache reuse before network resolution;
- provider-specific playlist/account pages;
- account-level source priority profiles.
