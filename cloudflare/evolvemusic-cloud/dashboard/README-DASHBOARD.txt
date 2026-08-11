EvolveMusic Cloudflare backend snapshot v0.10.8

The desktop v0.12.3 client is paired with Worker v0.10.8.
Current configured Worker URL:
https://evolvemusic-cloud.18048369193.workers.dev

If redeploying in Cloudflare Dashboard:
1. Open evolvemusic-cloud -> Edit code.
2. Replace the Worker source with this folder's worker.js.
3. Deploy.
4. Keep D1 binding name DB and the existing secrets:
   SESSION_MASTER_KEY, STREAM_SIGNING_KEY, ADMIN_KEY.
5. Keep optional AUDIO R2 / NETEASE_API_ORIGIN bindings if you already use them.
6. Open /health and confirm it reports version 0.10.8.

v0.10.8 keeps the v0.10.7 Range proxy improvements and adds content recovery; it embeds the resolved upstream
CDN URL in the signed ticket, avoiding a repeated player/url resolution for
every QMediaPlayer byte-range request.

Use worker-dashboard-v0.10.8.js for this build.
