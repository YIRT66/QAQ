const encoder = new TextEncoder();
const decoder = new TextDecoder();
function json(data, status = 200, extraHeaders = {}) {
    return new Response(JSON.stringify(data), {
        status,
        headers: {
            "Content-Type": "application/json; charset=utf-8",
            "Cache-Control": "no-store",
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Headers": "Authorization,Content-Type,X-Admin-Key",
            "Access-Control-Allow-Methods": "GET,POST,PUT,DELETE,OPTIONS",
            ...extraHeaders
        }
    });
}
function ok(data = {}) {
    return json({ ok: true, ...data });
}
function fail(error, message, status = 400) {
    return json({
        ok: false,
        error,
        message
    }, status);
}
function base64Url(bytes) {
    let binary = "";
    for (const byte of bytes)
        binary += String.fromCharCode(byte);
    return btoa(binary)
        .replace(/\+/g, "-")
        .replace(/\//g, "_")
        .replace(/=+$/g, "");
}
function fromBase64Url(value) {
    const padded = value.replace(/-/g, "+").replace(/_/g, "/")
        + "===".slice((value.length + 3) % 4);
    const binary = atob(padded);
    const out = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++)
        out[i] = binary.charCodeAt(i);
    return out;
}
async function sha256Hex(value) {
    const digest = await crypto.subtle.digest("SHA-256", encoder.encode(value));
    return [...new Uint8Array(digest)]
        .map((v) => v.toString(16).padStart(2, "0"))
        .join("");
}
function randomToken(bytes = 32) {
    const data = new Uint8Array(bytes);
    crypto.getRandomValues(data);
    return base64Url(data);
}
async function aesKey(secret) {
    const digest = await crypto.subtle.digest("SHA-256", encoder.encode(secret));
    return crypto.subtle.importKey("raw", digest, "AES-GCM", false, ["encrypt", "decrypt"]);
}
async function encryptText(value, secret) {
    const iv = new Uint8Array(12);
    crypto.getRandomValues(iv);
    const key = await aesKey(secret);
    const encrypted = await crypto.subtle.encrypt({
        name: "AES-GCM",
        iv
    }, key, encoder.encode(value));
    const combined = new Uint8Array(iv.byteLength
        + encrypted.byteLength);
    combined.set(iv, 0);
    combined.set(new Uint8Array(encrypted), iv.byteLength);
    return base64Url(combined);
}
async function decryptText(value, secret) {
    const combined = fromBase64Url(value);
    if (combined.byteLength < 13)
        throw new Error("invalid encrypted value");
    const iv = combined.slice(0, 12);
    const body = combined.slice(12);
    const key = await aesKey(secret);
    const decrypted = await crypto.subtle.decrypt({
        name: "AES-GCM",
        iv
    }, key, body);
    return decoder.decode(decrypted);
}
async function hmacSignature(payload, secret) {
    const key = await crypto.subtle.importKey("raw", encoder.encode(secret), {
        name: "HMAC",
        hash: "SHA-256"
    }, false, ["sign", "verify"]);
    const signature = await crypto.subtle.sign("HMAC", key, encoder.encode(payload));
    return base64Url(new Uint8Array(signature));
}
async function verifyHmac(payload, signature, secret) {
    const key = await crypto.subtle.importKey("raw", encoder.encode(secret), {
        name: "HMAC",
        hash: "SHA-256"
    }, false, ["verify"]);
    return crypto.subtle.verify("HMAC", key, fromBase64Url(signature), encoder.encode(payload));
}
async function authenticate(request, env) {
    const authorization = request.headers.get("Authorization") || "";
    if (!authorization.startsWith("Bearer "))
        return null;
    const token = authorization.slice(7).trim();
    if (!token)
        return null;
    const tokenHash = await sha256Hex(token);
    const row = await env.DB
        .prepare(`SELECT id
           FROM users
          WHERE token_hash = ?`)
        .bind(tokenHash)
        .first();
    if (!row)
        return null;
    await env.DB
        .prepare(`UPDATE users
          SET last_seen_at = ?
        WHERE id = ?`)
        .bind(Date.now(), row.id)
        .run();
    return { id: row.id };
}
function requireAdmin(request, env) {
    const supplied = request.headers.get("X-Admin-Key") || "";
    return Boolean(env.ADMIN_KEY
        && supplied
        && supplied === env.ADMIN_KEY);
}
function neteaseSongFee(song, playbackRow = null) {
    if (playbackRow
        && playbackRow.fee !== undefined
        && playbackRow.fee !== null) {
        return Number(playbackRow.fee);
    }

    if (song?.fee !== undefined
        && song?.fee !== null) {
        return Number(song.fee);
    }

    if (song?.privilege?.fee !== undefined
        && song?.privilege?.fee !== null) {
        return Number(song.privilege.fee);
    }

    return 0;
}

function normalizeNeteaseSong(
    song,
    playbackRow = null,
    hasOwnSession = false
) {
    const artists =
        song.ar
        || song.artists
        || [];

    const album =
        song.al
        || song.album
        || {};

    const sourceId =
        String(
            song.id
            ?? ""
        );

    const fee =
        neteaseSongFee(
            song,
            playbackRow
        );

    const probed =
        Boolean(playbackRow);

    const playable =
        probed
            ? Boolean(
                playbackRow?.url
                && Number(
                    playbackRow?.code
                    ?? 200
                ) === 200
            )
            : true;

    let access;

    if (playable) {
        access =
            fee === 0
                ? "free"
                : hasOwnSession
                    ? "account"
                    : "vip";
    } else {
        access =
            fee === 0
                ? "unavailable"
                : "vip";
    }

    return {
        id:
            `netease:${sourceId}`,
        sourceId,
        providerId:
            "netease",
        providerName:
            "网易云音乐",
        title:
            String(
                song.name
                ?? ""
            ),
        artist:
            artists
                .map(
                    (a) =>
                        String(
                            a.name
                            ?? ""
                        )
                )
                .filter(Boolean)
                .join(" / "),
        album:
            String(
                album.name
                ?? ""
            ),
        cover:
            String(
                album.picUrl
                || album.blurPicUrl
                || ""
            ),
        duration:
            Number(
                song.dt
                ?? song.duration
                ?? 0
            ),
        access,
        playable,
        bitrate:
            Number(
                playbackRow?.br
                ?? 0
            ),
        audioType:
            String(
                playbackRow?.type
                ?? ""
            ),
        sourceCount: 1,
        sourceSummary:
            "网易云音乐"
    };
}
function normalizeCloudTrack(row) {
    return {
        id: `cloud:${row.id}`,
        sourceId: String(row.id),
        providerId: "cloud",
        providerName: "Evolve Cloud",
        title: String(row.title ?? ""),
        artist: String(row.artist ?? ""),
        album: String(row.album ?? ""),
        cover: String(row.cover ?? ""),
        duration: Number(row.duration ?? 0),
        access: String(row.access ?? "cloud"),
        playable: true,
        sourceCount: 1,
        sourceSummary: "Evolve Cloud"
    };
}
function neteaseSearchHeaders() {
    return {
        "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
        "Referer": "https://music.163.com/",
        "Origin": "https://music.163.com",
        "Accept": "application/json,text/plain,*/*",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.6",
        "Cookie": "os=pc; appver=2.9.7"
    };
}

const NETEASE_WEB_DECODE_KEY =
    Uint8Array.from(
        atob(
            "ZnVja34jJCVeJiooNDU4ZnVja34jJCVeJiooNDU4ZnVja34jJCVeJiooNDU4ZnVja34jJCVeJiooNDU4ZnVjaw=="
        ),
        (ch) => ch.charCodeAt(0)
    );

const NETEASE_WEB_DECODE_TABLE =
    Uint8Array.from(
        atob(
            "SEBYtQgkKXb0cnHfkX4NYwFNWxHoTn3qKOZkhoic61n9A+ehxagyi5ldrZ72FDO2v8Px0+7lpj4PVxqks85Rwh7h1ozc9+L/GR2nJ428eXS40NRsIVPbep0vmNqJtHVPli0OUvB4qaMmrl42mktopWBC6XMQJftq3RvN1QmCzwWirOP4kgqTu4UEX+wXxvWPNarKlLAY2Wc7abLIxD/vRRYrQyzeTAfyBiCr12I0WkRVudHMx3+vVDefHwzBgEa6y8lvZpc4MS5WOXwVPTqDhJBwa7eO0lzz+iq9biISHGFHoEllvpV3/LE8/m1BUPnYhwswisACSiPg5JsA7XuBEw=="
        ),
        (ch) => ch.charCodeAt(0)
    );

function decodeNeteaseWebResult(hexText) {
    if (typeof hexText !== "string"
        || hexText.length < 16
        || (hexText.length % 2) !== 0
        || !/^[0-9a-f]+$/i.test(hexText)) {
        return null;
    }

    const cipherLength =
        hexText.length >> 1;

    const history =
        new Uint8Array(
            NETEASE_WEB_DECODE_KEY.length
            + cipherLength
        );

    history.set(
        NETEASE_WEB_DECODE_KEY,
        0
    );

    const plain =
        new Uint8Array(cipherLength);

    for (let i = 0; i < cipherLength; ++i) {
        const cipherByte =
            Number.parseInt(
                hexText.slice(
                    i * 2,
                    i * 2 + 2
                ),
                16
            );

        history[
            NETEASE_WEB_DECODE_KEY.length
            + i
        ] = cipherByte;

        const previous =
            history[i];

        plain[i] =
            (
                (
                    NETEASE_WEB_DECODE_TABLE[
                        cipherByte
                    ]
                    ^ previous
                )
                - previous
            )
            ^ NETEASE_WEB_DECODE_KEY[
                i
                & (
                    NETEASE_WEB_DECODE_KEY.length
                    - 1
                )
            ];
    }

    if (plain.length < 4)
        return null;

    const view =
        new DataView(
            plain.buffer,
            plain.byteOffset,
            plain.byteLength
        );

    const jsonLength =
        view.getInt32(
            plain.length - 4,
            false
        );

    if (jsonLength <= 0
        || jsonLength > plain.length - 4) {
        return null;
    }

    try {
        const jsonText =
            new TextDecoder("utf-8", {
                fatal: false
            }).decode(
                plain.slice(
                    0,
                    jsonLength
                )
            );

        return JSON.parse(jsonText);
    } catch {
        return null;
    }
}

function parseNeteaseSearchSongs(body) {
    if (!body
        || typeof body !== "object") {
        return {
            songs: [],
            decoded: false
        };
    }

    let resolved = body;
    let decoded = false;

    if (typeof body.result === "string") {
        const decrypted =
            decodeNeteaseWebResult(
                body.result
            );

        if (decrypted
            && typeof decrypted === "object") {
            resolved = {
                ...body,
                result: decrypted
            };
            decoded = true;
        }
    }

    let songs = [];

    if (Array.isArray(
            resolved?.result?.songs)) {
        songs =
            resolved.result.songs;
    } else if (Array.isArray(
            resolved?.result?.song)) {
        songs =
            resolved.result.song;
    } else if (Array.isArray(
            resolved?.songs)) {
        songs =
            resolved.songs;
    }

    return {
        songs,
        decoded
    };
}

function normalizedSearchText(value) {
    return String(value || "")
        .toLowerCase()
        .normalize("NFKC")
        .replace(
            /[\s\p{P}\p{S}]+/gu,
            ""
        );
}

function songSearchText(song) {
    const artists =
        song?.ar
        || song?.artists
        || [];

    return normalizedSearchText(
        [
            song?.name || "",
            ...artists.map(
                (artist) =>
                    artist?.name || ""
            ),
            song?.al?.name
                || song?.album?.name
                || ""
        ].join(" ")
    );
}

function searchResultLooksRelevant(
    songs,
    keywords
) {
    if (!Array.isArray(songs)
        || songs.length === 0) {
        return false;
    }

    const needle =
        normalizedSearchText(keywords);

    if (!needle)
        return true;

    return songs
        .slice(0, 12)
        .some((song) => {
            const haystack =
                songSearchText(song);

            if (haystack.includes(needle))
                return true;

            const tokens =
                String(keywords || "")
                    .trim()
                    .toLowerCase()
                    .split(/\s+/)
                    .map(normalizedSearchText)
                    .filter(Boolean);

            return tokens.length > 1
                && tokens.every(
                    (token) =>
                        haystack.includes(token)
                );
        });
}

async function runNeteaseSearchAttempt(kind, keywords, limit) {
    const headers = neteaseSearchHeaders();
    let response;

    const isPost =
        kind === "post-cloud-web"
        || kind === "post-web"
        || kind === "post-api";

    if (isPost) {
        let endpoint =
            "https://music.163.com/api/search/get";

        if (kind === "post-cloud-web") {
            endpoint =
                "https://music.163.com/api/cloudsearch/get/web";
        } else if (kind === "post-web") {
            endpoint =
                "https://music.163.com/api/search/get/web";
        }

        const form = new URLSearchParams();
        form.set("s", keywords);
        form.set("type", "1");
        form.set("offset", "0");
        form.set("total", "true");
        form.set("limit", String(limit));
        form.set("csrf_token", "");

        response = await fetch(endpoint, {
            method: "POST",
            headers: {
                ...headers,
                "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8"
            },
            body: form.toString(),
            redirect: "follow"
        });
    } else {
        const endpoint =
            kind === "get-cloud-web"
                ? "https://music.163.com/api/cloudsearch/get/web"
                : "https://music.163.com/api/search/get/web";

        const url = new URL(endpoint);

        url.searchParams.set("csrf_token", "");
        url.searchParams.set("s", keywords);
        url.searchParams.set("type", "1");
        url.searchParams.set("offset", "0");
        url.searchParams.set("total", "true");
        url.searchParams.set("limit", String(limit));

        response = await fetch(url, {
            method: "GET",
            headers,
            redirect: "follow"
        });
    }

    const contentType =
        response.headers.get("content-type") || "";

    const raw =
        await response.text();

    let body = null;

    try {
        body = JSON.parse(raw);
    } catch {
        body = null;
    }

    const parsed =
        parseNeteaseSearchSongs(body);

    return {
        kind,
        ok: response.ok,
        status: response.status,
        contentType,
        songs: parsed.songs,
        decoded: parsed.decoded,
        relevant:
            searchResultLooksRelevant(
                parsed.songs,
                keywords
            ),
        raw
    };
}

async function directNeteaseSearch(keywords, limit) {
    for (const kind of [
        "get-cloud-web",
        "post-cloud-web",
        "get-web",
        "post-web",
        "post-api"
    ]) {
        try {
            const result =
                await runNeteaseSearchAttempt(
                    kind,
                    keywords,
                    limit
                );

            if (result.songs.length > 0
                && result.relevant) {
                return result.songs;
            }
        } catch {
        }
    }

    return [];
}

async function debugNeteaseSearch(request) {
    const url = new URL(request.url);
    const keywords =
        (url.searchParams.get("q") || "晴天").trim();

    const attempts = [];

    for (const kind of [
        "get-cloud-web",
        "post-cloud-web",
        "get-web",
        "post-web",
        "post-api"
    ]) {
        try {
            const result =
                await runNeteaseSearchAttempt(
                    kind,
                    keywords,
                    5
                );

            attempts.push({
                kind: result.kind,
                ok: result.ok,
                status: result.status,
                contentType: result.contentType,
                songCount: result.songs.length,
                decoded: result.decoded,
                relevant: result.relevant,
                firstSongs:
                    result.songs
                        .slice(0, 5)
                        .map((song) => ({
                            id: song?.id,
                            name: song?.name,
                            artists:
                                (
                                    song?.ar
                                    || song?.artists
                                    || []
                                )
                                .map(
                                    (artist) =>
                                        artist?.name || ""
                                )
                                .filter(Boolean)
                        })),
                bodyPreview:
                    result.raw.slice(0, 500)
            });
        } catch (error) {
            attempts.push({
                kind,
                ok: false,
                status: 0,
                songCount: 0,
                error:
                    String(
                        error?.message
                        || error
                        || "unknown error"
                    )
            });
        }
    }

    return ok({
        keyword: keywords,
        environment: "cloudflare-worker",
        decoder: {
            fullKeyLength:
                NETEASE_WEB_DECODE_KEY.length,
            decodeTableLength:
                NETEASE_WEB_DECODE_TABLE.length,
            expectedFullKeyLength: 64,
            expectedDecodeTableLength: 256
        },
        attempts
    });
}

function neteaseSongRelevanceScore(song, keywords) {
    const rawQuery =
        String(
            keywords
            || ""
        ).trim();

    const query =
        normalizedSearchText(
            rawQuery
        );

    if (!query)
        return 0;

    const title =
        normalizedSearchText(
            song?.name
            || ""
        );

    const artists =
        normalizedSearchText(
            (
                song?.ar
                || song?.artists
                || []
            )
            .map(
                (artist) =>
                    artist?.name
                    || ""
            )
            .join(" ")
        );

    const album =
        normalizedSearchText(
            song?.al?.name
            || song?.album?.name
            || ""
        );

    const whole =
        `${title}${artists}${album}`;

    const tokens =
        rawQuery
            .split(/\s+/)
            .map(
                normalizedSearchText
            )
            .filter(Boolean);

    let score = 0;

    if (title === query)
        score += 120;
    if (title.startsWith(query))
        score += 80;
    if (title.includes(query))
        score += 60;
    if (artists.includes(query))
        score += 45;
    if (whole.includes(query))
        score += 30;

    for (const token of tokens) {
        if (title === token)
            score += 35;
        else if (title.includes(token))
            score += 25;

        if (artists.includes(token))
            score += 22;

        if (album.includes(token))
            score += 8;
    }

    if (tokens.length > 1
        && tokens.every(
            (token) =>
                whole.includes(token)
        )) {
        score += 80;
    }

    return score;
}

function isNeteaseSongRelevantToQuery(
    song,
    keywords
) {
    return neteaseSongRelevanceScore(
        song,
        keywords
    ) > 0;
}

async function probeNeteaseSongPlayback(
    env,
    userId,
    songs,
    bitrate = 320000
) {
    const rows =
        new Map();

    const ids =
        [
            ...new Set(
                songs
                    .map(
                        (song) =>
                            String(
                                song?.id
                                ?? ""
                            )
                    )
                    .filter(Boolean)
            )
        ]
        .slice(0, 40);

    if (ids.length === 0) {
        return {
            rows,
            hasOwnSession: false
        };
    }

    const session =
        userId
            ? await getUserProviderSession(
                env,
                userId,
                "netease"
            )
            : null;

    const cookie =
        String(
            session?.cookie
            || ""
        );

    const url =
        new URL(
            "https://music.163.com"
            + "/api/song/enhance/player/url"
        );

    url.searchParams.set(
        "csrf_token",
        ""
    );
    url.searchParams.set(
        "ids",
        JSON.stringify(ids)
    );
    url.searchParams.set(
        "br",
        String(bitrate)
    );

    try {
        const response =
            await fetch(
                url,
                {
                    method: "GET",
                    headers:
                        neteasePlayerHeaders(
                            cookie
                        ),
                    redirect: "follow"
                }
            );

        if (response.ok) {
            const body =
                await response.json()
                    .catch(
                        () => null
                    );

            for (const row of (
                Array.isArray(body?.data)
                    ? body.data
                    : []
            )) {
                const id =
                    String(
                        row?.id
                        ?? ""
                    );

                if (id) {
                    rows.set(
                        id,
                        row
                    );
                }
            }
        }
    } catch {
    }

    return {
        rows,
        hasOwnSession:
            Boolean(cookie)
    };
}

function normalizeNeteaseCdnUrl(value) {
    const raw =
        String(
            value
            || ""
        ).trim();

    if (!raw)
        return "";

    // Keep NetEase's original scheme. Some short-lived signed CDN URLs are
    // issued as http:// and changing the scheme can make the edge reject them.
    if (raw.startsWith("//"))
        return `https:${raw}`;

    return raw;
}

async function gatewayJson(env, path, query, cookie = "") {
    const origin = String(env.NETEASE_API_ORIGIN || "")
        .trim()
        .replace(/\/+$/, "");
    if (!origin)
        return null;
    const url = new URL(origin
        + path);
    for (const [key, value] of Object.entries(query))
        url.searchParams.set(key, value);
    if (cookie)
        url.searchParams.set("cookie", cookie);
    url.searchParams.set("timestamp", String(Date.now()));
    const response = await fetch(url, {
        headers: {
            "Accept": "application/json",
            "User-Agent": "EvolveMusic-Cloud/0.10"
        }
    });
    if (!response.ok)
        return null;
    return response.json();
}
async function searchNetease(env, keywords, limit) {
    const gateway = await gatewayJson(env, "/cloudsearch", {
        keywords,
        limit: String(limit)
    });
    if (Array.isArray(gateway?.result?.songs)) {
        return gateway.result.songs;
    }
    return directNeteaseSearch(keywords, limit);
}
function neteaseSongCover(song) {
    const album =
        song?.al
        || song?.album
        || {};
    return String(
        album?.picUrl
        || album?.blurPicUrl
        || ""
    ).trim();
}

function mergeNeteaseSongMetadata(song, detail) {
    if (!detail || typeof detail !== "object")
        return song;

    const merged = {
        ...detail,
        ...song
    };

    const originalArtists =
        song?.ar
        || song?.artists
        || [];
    if (!Array.isArray(originalArtists)
        || originalArtists.length === 0) {
        if (Array.isArray(detail?.ar))
            merged.ar = detail.ar;
        else if (Array.isArray(detail?.artists))
            merged.artists = detail.artists;
    }

    const originalAlbum =
        song?.al
        || song?.album
        || {};
    const detailAlbum =
        detail?.al
        || detail?.album
        || {};
    const mergedAlbum = {
        ...detailAlbum,
        ...originalAlbum
    };
    if (!originalAlbum?.picUrl
        && detailAlbum?.picUrl) {
        mergedAlbum.picUrl = detailAlbum.picUrl;
    }
    if (!originalAlbum?.blurPicUrl
        && detailAlbum?.blurPicUrl) {
        mergedAlbum.blurPicUrl = detailAlbum.blurPicUrl;
    }

    if (song?.al)
        merged.al = mergedAlbum;
    else
        merged.album = mergedAlbum;

    if (!song?.dt && !song?.duration) {
        if (detail?.dt)
            merged.dt = detail.dt;
        else if (detail?.duration)
            merged.duration = detail.duration;
    }

    return merged;
}

async function enrichNeteaseSongs(env, songs) {
    if (!Array.isArray(songs) || songs.length === 0)
        return [];

    const missingIds =
        songs
            .filter(
                (song) =>
                    !neteaseSongCover(song)
            )
            .map(
                (song) =>
                    String(song?.id ?? "")
            )
            .filter(Boolean)
            .slice(0, 50);

    if (missingIds.length === 0)
        return songs;

    let details = [];

    if (env.NETEASE_API_ORIGIN) {
        try {
            const gateway =
                await gatewayJson(
                    env,
                    "/song/detail",
                    { ids: missingIds.join(",") }
                );
            if (Array.isArray(gateway?.songs))
                details = gateway.songs;
        } catch {
        }
    }

    if (details.length === 0) {
        try {
            const url =
                new URL("https://music.163.com/api/song/detail");
            url.searchParams.set(
                "ids",
                JSON.stringify(missingIds)
            );
            const response = await fetch(url, {
                headers: neteaseSearchHeaders(),
                redirect: "follow"
            });
            if (response.ok) {
                const body =
                    await response.json().catch(() => null);
                if (Array.isArray(body?.songs))
                    details = body.songs;
            }
        } catch {
        }
    }

    if (details.length === 0)
        return songs;

    const byId =
        new Map(
            details
                .map(
                    (detail) => [
                        String(detail?.id ?? ""),
                        detail
                    ]
                )
                .filter(([id]) => Boolean(id))
        );

    return songs.map(
        (song) =>
            mergeNeteaseSongMetadata(
                song,
                byId.get(String(song?.id ?? ""))
            )
    );
}

async function cloudCatalogSearch(env, keywords, limit) {
    const term = `%${keywords.replace(/%/g, "")}%`;
    const result = await env.DB
        .prepare(`SELECT id,
                title,
                artist,
                album,
                cover,
                duration,
                access
           FROM track_catalog
          WHERE title LIKE ?
             OR artist LIKE ?
          ORDER BY created_at DESC
          LIMIT ?`)
        .bind(term, term, limit)
        .all();
    return result.results || [];
}
async function getUserProviderSession(env, userId, provider) {
    const row = await env.DB
        .prepare(`SELECT encrypted_session
           FROM provider_sessions
          WHERE user_id = ?
            AND provider = ?`)
        .bind(userId, provider)
        .first();
    if (!row)
        return null;
    try {
        const plain = await decryptText(row.encrypted_session, env.SESSION_MASTER_KEY);
        return JSON.parse(plain);
    }
    catch {
        return null;
    }
}
function qualityToBitrate(quality) {
    switch (String(quality || "").toLowerCase()) {
        case "standard":
            return 128000;
        case "higher":
            return 192000;
        case "exhigh":
            return 320000;
        case "lossless":
        case "hires":
        case "jyeffect":
        case "sky":
        case "jymaster":
            return 999000;
        default:
            return 320000;
    }
}

function neteasePlayerHeaders(cookie = "") {
    const headers = {
        "User-Agent":
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            + "AppleWebKit/537.36 (KHTML, like Gecko) "
            + "Chrome/131.0.0.0 Safari/537.36",
        "Referer": "https://music.163.com/",
        "Origin": "https://music.163.com",
        "Accept": "application/json,text/plain,*/*",
        "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.6"
    };

    if (cookie) {
        headers.Cookie = cookie;
    } else {
        headers.Cookie =
            "os=pc; appver=2.10.12";
    }

    return headers;
}

async function runNeteasePlayerUrlAttempt(
    kind,
    sourceId,
    bitrate,
    cookie = ""
) {
    const headers =
        neteasePlayerHeaders(cookie);

    const base =
        "https://music.163.com"
        + "/api/song/enhance/player/url";

    let response;

    if (kind === "post") {
        const form =
            new URLSearchParams();

        form.set(
            "ids",
            JSON.stringify([
                String(sourceId)
            ])
        );
        form.set(
            "br",
            String(bitrate)
        );
        form.set(
            "csrf_token",
            ""
        );

        response = await fetch(
            base,
            {
                method: "POST",
                headers: {
                    ...headers,
                    "Content-Type":
                        "application/x-www-form-urlencoded; charset=UTF-8"
                },
                body: form.toString(),
                redirect: "follow"
            }
        );
    } else {
        const url =
            new URL(base);

        url.searchParams.set(
            "csrf_token",
            ""
        );
        url.searchParams.set(
            "ids",
            JSON.stringify([
                String(sourceId)
            ])
        );
        url.searchParams.set(
            "br",
            String(bitrate)
        );

        response = await fetch(
            url,
            {
                method: "GET",
                headers,
                redirect: "follow"
            }
        );
    }

    const contentType =
        response.headers.get(
            "content-type"
        ) || "";

    const raw =
        await response.text();

    let body = null;

    try {
        body = JSON.parse(raw);
    } catch {
        body = null;
    }

    const row =
        Array.isArray(body?.data)
            ? body.data[0]
            : null;

    const audioUrl =
        typeof row?.url === "string"
            ? normalizeNeteaseCdnUrl(
                row.url
            )
            : "";

    return {
        kind,
        ok: response.ok,
        status: response.status,
        contentType,
        body,
        row,
        audioUrl,
        raw
    };
}

async function resolveNeteaseOuterUrl(sourceId) {
    const outerUrl =
        `https://music.163.com/song/media/outer/url?id=${encodeURIComponent(sourceId)}.mp3`;

    try {
        const response = await fetch(
            outerUrl,
            {
                method: "GET",
                headers: {
                    ...neteasePlayerHeaders(""),
                    "Range": "bytes=0-0",
                    "Accept-Encoding": "identity"
                },
                redirect: "follow"
            }
        );

        const contentType =
            response.headers.get("content-type") || "";

        const usable =
            (response.ok || response.status === 206)
            && !isClearlyNonAudioContentType(contentType);

        const finalUrl =
            normalizeNeteaseCdnUrl(response.url || "");

        try {
            await response.body?.cancel();
        } catch {
        }

        if (usable && finalUrl)
            return finalUrl;
    } catch {
    }

    return "";
}

async function resolveNeteaseAudioUrl(
    env,
    userId,
    sourceId,
    quality
) {
    const session =
        await getUserProviderSession(
            env,
            userId,
            "netease"
        );

    // Only this user's own stored provider session is used.
    const cookie =
        String(
            session?.cookie
            || ""
        );

    // Optional external gateway remains supported.
    if (env.NETEASE_API_ORIGIN) {
        const body =
            await gatewayJson(
                env,
                "/song/url/v1",
                {
                    id: sourceId,
                    level:
                        quality
                        || "exhigh"
                },
                cookie
            );

        const row =
            Array.isArray(body?.data)
                ? body.data[0]
                : null;

        const audioUrl =
            normalizeNeteaseCdnUrl(
                row?.url
                || ""
            );

        if (audioUrl) {
            return {
                url: audioUrl,
                access:
                    cookie
                        ? "account"
                        : "free",
                resolver: "gateway-v1"
            };
        }
    }

    const bitrate =
        qualityToBitrate(
            quality
        );

    for (const kind of [
        "post",
        "get"
    ]) {
        try {
            const result =
                await runNeteasePlayerUrlAttempt(
                    kind,
                    sourceId,
                    bitrate,
                    cookie
                );

            if (result.audioUrl) {
                return {
                    url: result.audioUrl,
                    access:
                        cookie
                            ? "account"
                            : "free",
                    resolver:
                        `direct-player-url-${kind}`,
                    providerRow:
                        result.row
                };
            }
        } catch {
        }
    }

    // Last-resort official outer-url endpoint. It frequently keeps working
    // when player/url temporarily returns a null URL to Cloudflare egress.
    const outerUrl =
        await resolveNeteaseOuterUrl(sourceId);

    if (outerUrl) {
        return {
            url: outerUrl,
            access:
                cookie
                    ? "account"
                    : "free",
            resolver: "outer-url"
        };
    }

    return null;
}

async function debugNeteaseAudio(request) {
    const url =
        new URL(request.url);

    const sourceId =
        String(
            url.searchParams.get("id")
            || "2730151393"
        ).trim();

    const bitrate =
        Number(
            url.searchParams.get("br")
            || "320000"
        );

    const attempts = [];

    for (const kind of [
        "post",
        "get"
    ]) {
        try {
            const result =
                await runNeteasePlayerUrlAttempt(
                    kind,
                    sourceId,
                    bitrate,
                    ""
                );

            let host = "";
            if (result.audioUrl) {
                try {
                    host =
                        new URL(
                            result.audioUrl
                        ).host;
                } catch {
                }
            }

            attempts.push({
                kind,
                ok: result.ok,
                status: result.status,
                contentType:
                    result.contentType,
                code:
                    result.body?.code
                    ?? null,
                hasUrl:
                    Boolean(
                        result.audioUrl
                    ),
                audioHost:
                    host,
                bitrate:
                    result.row?.br
                    ?? null,
                type:
                    result.row?.type
                    ?? null,
                fee:
                    result.row?.fee
                    ?? null,
                freeTrial:
                    result.row?.freeTrialInfo
                    ?? null,
                bodyPreview:
                    result.raw.slice(
                        0,
                        700
                    )
            });
        } catch (error) {
            attempts.push({
                kind,
                ok: false,
                status: 0,
                hasUrl: false,
                error:
                    String(
                        error?.message
                        || error
                        || "unknown error"
                    )
            });
        }
    }

    // Inspect outer-url behavior without downloading the HTML body.
    let outer = null;

    try {
        const outerUrl =
            `https://music.163.com/song/media/outer/url?id=${
                encodeURIComponent(
                    sourceId
                )
            }.mp3`;

        const response =
            await fetch(
                outerUrl,
                {
                    method: "GET",
                    headers:
                        neteasePlayerHeaders(
                            ""
                        ),
                    redirect: "manual"
                }
            );

        let locationHost = "";

        const location =
            response.headers.get(
                "location"
            ) || "";

        if (location) {
            try {
                locationHost =
                    new URL(
                        location,
                        outerUrl
                    ).host;
            } catch {
            }
        }

        outer = {
            status:
                response.status,
            contentType:
                response.headers.get(
                    "content-type"
                ) || "",
            hasLocation:
                Boolean(location),
            locationHost
        };
    } catch (error) {
        outer = {
            error:
                String(
                    error?.message
                    || error
                    || "unknown error"
                )
        };
    }

    return ok({
        sourceId,
        bitrate,
        attempts,
        outer
    });
}
function extractNeteaseLyrics(body) {
    if (!body || typeof body !== "object") {
        return {
            lyric: "",
            translatedLyric: ""
        };
    }

    return {
        lyric: String(
            body?.lrc?.lyric
            || body?.lyric
            || ""
        ),
        translatedLyric: String(
            body?.tlyric?.lyric
            || body?.transUser?.lyric
            || ""
        )
    };
}

function hasUsefulLyrics(result) {
    return Boolean(
        String(result?.lyric || "").trim()
        || String(result?.translatedLyric || "").trim()
    );
}

async function lyricsNetease(env, sourceId) {
    // Prefer the optional gateway when configured, but never make lyrics depend
    // on it. Different API builds expose either /lyric/new or /lyric.
    for (const path of ["/lyric/new", "/lyric"]) {
        try {
            const gateway =
                await gatewayJson(
                    env,
                    path,
                    { id: sourceId }
                );
            const parsed =
                extractNeteaseLyrics(gateway);
            if (hasUsefulLyrics(parsed))
                return parsed;
        } catch {
        }
    }

    const headers = {
        ...neteasePlayerHeaders(""),
        "Accept": "application/json,text/plain,*/*"
    };

    // NetEase has served this endpoint through both GET and form POST over
    // time. Try both before falling back to the old song/media endpoint.
    for (const method of ["GET", "POST"]) {
        try {
            let response;
            if (method === "GET") {
                const url =
                    new URL("https://music.163.com/api/song/lyric");
                url.searchParams.set("os", "pc");
                url.searchParams.set("id", sourceId);
                url.searchParams.set("lv", "-1");
                url.searchParams.set("kv", "-1");
                url.searchParams.set("tv", "-1");
                response = await fetch(url, {
                    method: "GET",
                    headers,
                    redirect: "follow"
                });
            } else {
                const form = new URLSearchParams();
                form.set("os", "pc");
                form.set("id", sourceId);
                form.set("lv", "-1");
                form.set("kv", "-1");
                form.set("tv", "-1");
                response = await fetch(
                    "https://music.163.com/api/song/lyric",
                    {
                        method: "POST",
                        headers: {
                            ...headers,
                            "Content-Type":
                                "application/x-www-form-urlencoded; charset=UTF-8"
                        },
                        body: form.toString(),
                        redirect: "follow"
                    }
                );
            }

            if (response.ok) {
                const body =
                    await response.json().catch(() => null);
                const parsed =
                    extractNeteaseLyrics(body);
                if (hasUsefulLyrics(parsed))
                    return parsed;
            }
        } catch {
        }
    }

    try {
        const mediaUrl =
            new URL("https://music.163.com/api/song/media");
        mediaUrl.searchParams.set("id", sourceId);
        const response = await fetch(mediaUrl, {
            headers,
            redirect: "follow"
        });
        if (response.ok) {
            const body =
                await response.json().catch(() => null);
            const parsed =
                extractNeteaseLyrics(body);
            if (hasUsefulLyrics(parsed))
                return parsed;
        }
    } catch {
    }

    return {
        lyric: "",
        translatedLyric: ""
    };
}

function safeArray(value) {
    return Array.isArray(value)
        ? value
        : [];
}
function safeObject(value) {
    return value
        && typeof value === "object"
        && !Array.isArray(value)
        ? value
        : {};
}
async function streamR2(request, env, r2Key) {
    const object = await env.AUDIO.get(r2Key, {
        onlyIf: request.headers,
        range: request.headers
    });
    if (!object)
        return fail("audio_not_found", "R2 音频不存在", 404);
    if (!("body" in object)
        || !object.body) {
        return new Response(null, {
            status: 412,
            headers: {
                ETag: object.httpEtag
            }
        });
    }
    const headers = new Headers();
    object.writeHttpMetadata(headers);
    headers.set("ETag", object.httpEtag);
    headers.set("Accept-Ranges", "bytes");
    headers.set("Cache-Control", "private, max-age=3600");
    let status = 200;
    if (object.range) {
        status = 206;
        const offset = object.range.offset || 0;
        const length = object.range.length
            ?? object.size;
        headers.set("Content-Range", `bytes ${offset}-${offset + length - 1}/${object.size}`);
        headers.set("Content-Length", String(length));
    }
    else {
        headers.set("Content-Length", String(object.size));
    }
    return new Response(object.body, {
        status,
        headers
    });
}
function isClearlyNonAudioContentType(contentType) {
    const value =
        String(
            contentType
            || ""
        )
        .toLowerCase();

    return value.includes("text/html")
        || value.includes("text/plain")
        || value.includes("application/json")
        || value.includes("application/xml")
        || value.includes("text/xml");
}

async function proxyStream(request, upstreamUrl) {
    const headers =
        new Headers();

    headers.set(
        "User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        + "AppleWebKit/537.36 (KHTML, like Gecko) "
        + "Chrome/131 Safari/537.36"
    );

    headers.set(
        "Referer",
        "https://music.163.com/"
    );
    headers.set(
        "Accept",
        "audio/*,application/octet-stream;q=0.9,*/*;q=0.2"
    );
    headers.set(
        "Accept-Encoding",
        "identity"
    );

    const range =
        request.headers.get(
            "Range"
        );

    if (range) {
        headers.set(
            "Range",
            range
        );
    }

    const requestMethod =
        request.method === "HEAD"
            ? "HEAD"
            : "GET";

    let upstream =
        await fetch(
            upstreamUrl,
            {
                method: requestMethod,
                headers,
                redirect: "follow"
            }
        );

    // Cloudflare egress and individual NetEase CDN hosts occasionally disagree
    // about Referer handling. Retry a failed/non-audio GET once with a minimal
    // header set before reporting a proxy failure.
    if (requestMethod === "GET"
        && (!upstream.ok
            || isClearlyNonAudioContentType(
                upstream.headers.get(
                    "Content-Type"
                ) || ""
            ))) {
        try {
            await upstream.body?.cancel();
        } catch {
        }

        const retryHeaders =
            new Headers();

        retryHeaders.set(
            "User-Agent",
            headers.get("User-Agent") || "Mozilla/5.0"
        );
        retryHeaders.set(
            "Accept",
            headers.get("Accept") || "*/*"
        );
        retryHeaders.set(
            "Accept-Encoding",
            "identity"
        );

        if (range) {
            retryHeaders.set(
                "Range",
                range
            );
        }

        upstream =
            await fetch(
                upstreamUrl,
                {
                    method: "GET",
                    headers: retryHeaders,
                    redirect: "follow"
                }
            );
    }

    // Some NetEase CDN hosts do not implement HEAD consistently.
    // Probe one byte with GET and return only headers to the media engine.
    if (requestMethod === "HEAD"
        && (!upstream.ok
            || isClearlyNonAudioContentType(
                upstream.headers.get(
                    "Content-Type"
                ) || ""
            ))) {
        const probeHeaders =
            new Headers(headers);

        probeHeaders.set(
            "Range",
            "bytes=0-0"
        );

        upstream =
            await fetch(
                upstreamUrl,
                {
                    method: "GET",
                    headers:
                        probeHeaders,
                    redirect:
                        "follow"
                }
            );
    }

    if (!upstream.ok
        && upstream.status !== 206) {
        return fail(
            "upstream_audio_failed",
            `上游音频请求失败：${upstream.status}`,
            502
        );
    }

    const contentType =
        upstream.headers.get(
            "Content-Type"
        ) || "";

    if (isClearlyNonAudioContentType(
            contentType
        )) {
        try {
            await upstream.body?.cancel();
        } catch {
        }

        return fail(
            "upstream_not_audio",
            `网易上游返回的不是音频：${contentType || "unknown"}`,
            502
        );
    }

    const outputHeaders =
        new Headers();

    for (const name of [
        "Content-Type",
        "Content-Length",
        "Content-Range",
        "Accept-Ranges",
        "ETag",
        "Last-Modified"
    ]) {
        const value =
            upstream.headers.get(
                name
            );

        if (value) {
            outputHeaders.set(
                name,
                value
            );
        }
    }

    if (!outputHeaders.has(
            "Content-Type")) {
        outputHeaders.set(
            "Content-Type",
            "application/octet-stream"
        );
    }

    outputHeaders.set(
        "Cache-Control",
        "private, max-age=300"
    );

    if (!outputHeaders.has(
            "Accept-Ranges")) {
        outputHeaders.set(
            "Accept-Ranges",
            "bytes"
        );
    }

    return new Response(
        request.method === "HEAD"
            ? null
            : upstream.body,
        {
            status:
                upstream.status,
            headers:
                outputHeaders
        }
    );
}
function allowedArtworkHost(hostname) {
    const host =
        String(hostname || "")
            .toLowerCase();

    return host === "music.126.net"
        || host.endsWith(".music.126.net")
        || host === "126.net"
        || host.endsWith(".126.net")
        || host === "163.com"
        || host.endsWith(".163.com");
}

async function handleArtwork(request) {
    const requestUrl =
        new URL(request.url);
    const raw =
        String(requestUrl.searchParams.get("url") || "")
            .trim();
    const size =
        Math.max(
            48,
            Math.min(
                1200,
                Number(requestUrl.searchParams.get("size") || 320)
            )
        );

    let upstreamUrl;
    try {
        upstreamUrl = new URL(raw);
    } catch {
        return fail("invalid_artwork", "封面地址无效", 400);
    }

    if (![
            "http:",
            "https:"
        ].includes(upstreamUrl.protocol)
        || !allowedArtworkHost(upstreamUrl.hostname)) {
        return fail("invalid_artwork_host", "不允许代理这个封面来源", 403);
    }

    if (upstreamUrl.hostname.endsWith("126.net")) {
        upstreamUrl.searchParams.set(
            "param",
            `${Math.round(size)}y${Math.round(size)}`
        );
    }

    const cache = caches.default;
    const cacheKey =
        new Request(requestUrl.toString(), {
            method: "GET"
        });

    if (request.method === "GET") {
        const cached = await cache.match(cacheKey);
        if (cached)
            return cached;
    }

    let response;
    try {
        response = await fetch(upstreamUrl, {
            method: request.method === "HEAD" ? "HEAD" : "GET",
            headers: {
                "User-Agent":
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                    + "AppleWebKit/537.36 Chrome/131 Safari/537.36",
                "Referer": "https://music.163.com/",
                "Accept": "image/avif,image/webp,image/apng,image/*,*/*;q=0.8"
            },
            redirect: "follow",
            cf: {
                cacheTtl: 86400,
                cacheEverything: true
            }
        });
    } catch (error) {
        return fail(
            "artwork_upstream_failed",
            String(error?.message || error || "封面请求失败"),
            502
        );
    }

    const contentType =
        response.headers.get("content-type") || "";

    if (!response.ok
        || !contentType.toLowerCase().startsWith("image/")) {
        try {
            await response.body?.cancel();
        } catch {
        }
        return fail(
            "artwork_upstream_failed",
            `封面上游返回异常：${response.status}`,
            502
        );
    }

    const headers = new Headers();
    headers.set("Content-Type", contentType);
    headers.set("Cache-Control", "public, max-age=86400, stale-while-revalidate=604800");
    headers.set("Access-Control-Allow-Origin", "*");
    const length = response.headers.get("content-length");
    if (length)
        headers.set("Content-Length", length);
    const etag = response.headers.get("etag");
    if (etag)
        headers.set("ETag", etag);

    const output = new Response(
        request.method === "HEAD" ? null : response.body,
        {
            status: 200,
            headers
        }
    );

    if (request.method === "GET") {
        try {
            await cache.put(cacheKey, output.clone());
        } catch {
        }
    }

    return output;
}

async function bootstrapUser(env) {
    const id = crypto.randomUUID();
    const token = randomToken(32);
    const hash = await sha256Hex(token);
    const now = Date.now();
    await env.DB.batch([
        env.DB
            .prepare(`INSERT INTO users(
           id,
           token_hash,
           created_at,
           last_seen_at
         ) VALUES (?, ?, ?, ?)`)
            .bind(id, hash, now, now),
        env.DB
            .prepare(`INSERT INTO user_state(
           user_id,
           favorites_json,
           history_json,
           settings_json,
           updated_at
         ) VALUES (?, '[]', '[]', '{}', ?)`)
            .bind(id, now)
    ]);
    return ok({
        user: { id },
        token
    });
}
async function handleSearch(
    request,
    env,
    user
) {
    const url =
        new URL(
            request.url
        );

    const q =
        (
            url.searchParams.get("q")
            || ""
        ).trim();

    const limit =
        Math.min(
            60,
            Math.max(
                1,
                Number(
                    url.searchParams.get(
                        "limit"
                    )
                    || 40
                )
            )
        );

    if (!q) {
        return ok({
            tracks: []
        });
    }

    const rawNeteaseLimit =
        Math.min(
            60,
            Math.max(
                limit,
                24
            )
        );

    const [
        neteaseSongsRaw,
        cloudRows
    ] =
        await Promise.all([
            searchNetease(
                env,
                q,
                rawNeteaseLimit
            ).catch(
                () => []
            ),
            cloudCatalogSearch(
                env,
                q,
                Math.min(
                    limit,
                    20
                )
            ).catch(
                () => []
            )
        ]);

    const neteaseSongs =
        neteaseSongsRaw
            .filter(
                (song) =>
                    isNeteaseSongRelevantToQuery(
                        song,
                        q
                    )
            )
            .sort(
                (a, b) =>
                    neteaseSongRelevanceScore(
                        b,
                        q
                    )
                    - neteaseSongRelevanceScore(
                        a,
                        q
                    )
            )
            .slice(
                0,
                Math.min(
                    40,
                    Math.max(
                        limit,
                        20
                    )
                )
            );

    const enrichedNeteaseSongs =
        await enrichNeteaseSongs(
            env,
            neteaseSongs
        );

    // Search is a metadata operation. Do not make the result list depend on
    // player/url probing: that endpoint can temporarily return null URLs while
    // search itself is healthy. Actual playability is resolved only when the
    // user presses Play.
    const ownSession =
        await getUserProviderSession(
            env,
            user?.id || "",
            "netease"
        ).catch(() => null);

    const neteaseTracks =
        enrichedNeteaseSongs
            .map(
                (song) =>
                    normalizeNeteaseSong(
                        song,
                        null,
                        Boolean(ownSession?.cookie)
                    )
            );

    const tracks = [
        ...neteaseTracks,
        ...cloudRows
            .map(
                normalizeCloudTrack
            )
    ];

    const seen =
        new Set();

    const unique =
        tracks.filter(
            (track) => {
                const key =
                    `${track.providerId}:${track.sourceId}`;

                if (seen.has(key))
                    return false;

                seen.add(key);
                return true;
            }
        );

    return ok({
        tracks:
            unique.slice(
                0,
                limit
            )
    });
}
async function buildNeteaseDiscoveryTracks(
    env,
    user,
    queries,
    maxTracks = 24
) {
    const groups =
        await Promise.all(
            queries.map(
                (query) =>
                    searchNetease(
                        env,
                        query,
                        12
                    )
                    .catch(
                        () => []
                    )
            )
        );

    const songs = [];
    const seen = new Set();

    for (const group of groups) {
        for (const song of group) {
            const id =
                String(
                    song?.id
                    ?? ""
                );

            if (!id
                || seen.has(id)) {
                continue;
            }

            seen.add(id);
            songs.push(song);

            if (songs.length >= 40)
                break;
        }

        if (songs.length >= 40)
            break;
    }

    const enrichedSongs =
        await enrichNeteaseSongs(
            env,
            songs
        );

    const ownSession =
        await getUserProviderSession(
            env,
            user?.id || "",
            "netease"
        ).catch(() => null);

    return enrichedSongs
        .map(
            (song) =>
                normalizeNeteaseSong(
                    song,
                    null,
                    Boolean(ownSession?.cookie)
                )
        )
        .slice(
            0,
            maxTracks
        );
}

async function handleHome(env, user) {
    const cloudRows = await env.DB
        .prepare(`SELECT id,
                title,
                artist,
                album,
                cover,
                duration,
                access
           FROM track_catalog
          ORDER BY created_at DESC
          LIMIT 24`)
        .all();

    let tracks =
        (cloudRows.results || [])
            .map(
                normalizeCloudTrack
            );

    const discoveryQueries = [
        "周杰伦",
        "林俊杰",
        "陈奕迅",
        "孙燕姿"
    ];

    let playlists =
        discoveryQueries.map(
            (query, index) => ({
                id:
                    `search:${query}`,
                sourceId:
                    query,
                providerId:
                    "search",
                title:
                    [
                        "今日精选",
                        "流行精选",
                        "经典回放",
                        "女声精选"
                    ][index],
                name:
                    [
                        "今日精选",
                        "流行精选",
                        "经典回放",
                        "女声精选"
                    ][index],
                cover: "",
                playCount: 0
            })
        );

    if (tracks.length < 12) {
        const discovery =
            await buildNeteaseDiscoveryTracks(
                env,
                user,
                discoveryQueries,
                24
            );

        const dedupe =
            new Set(
                tracks.map(
                    (track) =>
                        `${track.providerId}:${track.sourceId}`
                )
            );

        for (const track of discovery) {
            const key =
                `${track.providerId}:${track.sourceId}`;

            if (dedupe.has(key))
                continue;

            dedupe.add(key);
            tracks.push(track);
        }

        playlists =
            playlists.map(
                (playlist, index) => {
                    const cover =
                        tracks[index]?.cover
                        || tracks.find(
                            (track) =>
                                track.cover
                        )?.cover
                        || "";

                    return {
                        ...playlist,
                        cover
                    };
                }
            );
    }

    return ok({
        playlists,
        tracks:
            tracks.slice(0, 24)
    });
}

async function handlePlaylist(
    request,
    env,
    user
) {
    const url =
        new URL(
            request.url
        );

    const provider =
        url.searchParams.get(
            "provider"
        )
        || "netease";

    const id =
        url.searchParams.get(
            "id"
        )
        || "";

    if (!id) {
        return fail(
            "missing_id",
            "缺少歌单 ID"
        );
    }

    if (provider === "search") {
        const songs =
            await searchNetease(
                env,
                id,
                40
            )
            .catch(
                () => []
            );

        const enrichedSongs =
            await enrichNeteaseSongs(
                env,
                songs
            );

        const ownSession =
            await getUserProviderSession(
                env,
                user?.id || "",
                "netease"
            ).catch(() => null);

        const tracks =
            enrichedSongs
                .map(
                    (song) =>
                        normalizeNeteaseSong(
                            song,
                            null,
                            Boolean(ownSession?.cookie)
                        )
                )
                .slice(
                    0,
                    40
                );

        return ok({
            playlist: {
                id:
                    `search:${id}`,
                sourceId:
                    id,
                providerId:
                    "search",
                name:
                    `${id} · 云端精选`,
                title:
                    `${id} · 云端精选`,
                cover:
                    tracks.find(
                        (track) =>
                            track.cover
                    )?.cover
                    || "",
                description:
                    "由 Evolve Cloud 实时生成的发现列表"
            },
            tracks
        });
    }

    if (provider !== "netease"
        || !env.NETEASE_API_ORIGIN) {
        return ok({
            playlist: {},
            tracks: []
        });
    }

    const body =
        await gatewayJson(
            env,
            "/playlist/detail",
            { id }
        );

    const playlist =
        body?.playlist || {};

    return ok({
        playlist: {
            id:
                `netease:${playlist.id ?? id}`,
            sourceId:
                String(
                    playlist.id
                    ?? id
                ),
            providerId:
                "netease",
            name:
                String(
                    playlist.name
                    ?? ""
                ),
            title:
                String(
                    playlist.name
                    ?? ""
                ),
            cover:
                String(
                    playlist.coverImgUrl
                    ?? ""
                ),
            description:
                String(
                    playlist.description
                    ?? ""
                )
        },
        tracks:
            Array.isArray(
                playlist.tracks
            )
                ? playlist.tracks
                    .map(
                        normalizeNeteaseSong
                    )
                : []
    });
}

async function handleLyrics(request, env) {
    const url = new URL(request.url);
    const provider = url.searchParams.get("provider")
        || "netease";
    const sourceId = url.searchParams.get("id")
        || "";
    if (!sourceId)
        return fail("missing_id", "缺少歌曲 ID");
    if (provider !== "netease") {
        return ok({
            lyric: "",
            translatedLyric: ""
        });
    }
    const result = await lyricsNetease(env, sourceId);
    return ok(result);
}
async function createStreamTicket(request, env, user) {
    const body = safeObject(await request.json()
        .catch(() => ({})));
    const providerId = String(body.providerId || "");
    const sourceId = String(body.sourceId || "");
    const trackId = String(body.trackId || "");
    const quality = String(body.quality || "exhigh");
    if (!providerId
        || !sourceId) {
        return fail("missing_track", "缺少播放歌曲标识");
    }

    // Resolve NetEase once while the authenticated request is still here.
    // The signed ticket carries the resulting CDN URL so /v1/audio no longer
    // has to call player/url again for every range request made by QMediaPlayer.
    // This also lets the desktop use the short-lived CDN URL as a last-resort
    // direct fallback when Cloudflare proxy streaming is interrupted.
    let resolved = null;
    if (providerId === "netease") {
        resolved =
            await resolveNeteaseAudioUrl(
                env,
                user.id,
                sourceId,
                quality
            );

        if (!resolved?.url) {
            return fail(
                "netease_audio_unavailable",
                "网易未返回可播放音频地址",
                502
            );
        }
    }

    const payload = {
        uid: user.id,
        providerId,
        sourceId,
        trackId,
        quality,
        upstreamUrl:
            providerId === "netease"
                ? String(resolved?.url || "")
                : "",
        exp: Date.now()
            + 6 * 60 * 60 * 1000
    };
    const encoded = base64Url(encoder.encode(JSON.stringify(payload)));
    const signature = await hmacSignature(encoded, env.STREAM_SIGNING_KEY);
    const origin = new URL(request.url).origin;
    const streamUrl = `${origin}/v1/audio?ticket=${encodeURIComponent(encoded)}&sig=${encodeURIComponent(signature)}`;
    return ok({
        url: streamUrl,
        directUrl:
            providerId === "netease"
                ? String(resolved?.url || "")
                : "",
        providerName: providerId === "netease"
            ? "网易云音乐 · Cloudflare"
            : "Evolve Cloud",
        access:
            providerId === "netease"
                ? String(resolved?.access || "cloud")
                : "cloud"
    });
}
async function handleAudio(request, env) {
    const url = new URL(request.url);
    const ticket = url.searchParams.get("ticket")
        || "";
    const signature = url.searchParams.get("sig")
        || "";
    if (!ticket
        || !signature) {
        return fail("invalid_ticket", "播放票据缺失", 401);
    }
    const valid = await verifyHmac(ticket, signature, env.STREAM_SIGNING_KEY);
    if (!valid) {
        return fail("invalid_ticket", "播放票据签名无效", 401);
    }
    let payload;
    try {
        payload =
            JSON.parse(decoder.decode(fromBase64Url(ticket)));
    }
    catch {
        return fail("invalid_ticket", "播放票据解析失败", 401);
    }
    if (Number(payload.exp || 0)
        < Date.now()) {
        return fail("expired_ticket", "播放票据已过期", 401);
    }
    const user = await env.DB
        .prepare(`SELECT id
           FROM users
          WHERE id = ?`)
        .bind(String(payload.uid || ""))
        .first();
    if (!user)
        return fail("invalid_user", "云端用户不存在", 401);
    const providerId = String(payload.providerId || "");
    const sourceId = String(payload.sourceId || "");
    if (providerId === "cloud") {
        const row = await env.DB
            .prepare(`SELECT r2_key
             FROM track_catalog
            WHERE id = ?`)
            .bind(sourceId)
            .first();
        if (!row?.r2_key)
            return fail("audio_not_found", "云端曲库没有对应 R2 音频", 404);
        return streamR2(request, env, row.r2_key);
    }
    if (providerId === "netease") {
        let upstreamUrl =
            normalizeNeteaseCdnUrl(
                payload.upstreamUrl
                || ""
            );

        // Backward compatibility with tickets issued by v0.10.6 and older.
        if (!upstreamUrl) {
            const resolved =
                await resolveNeteaseAudioUrl(
                    env,
                    user.id,
                    sourceId,
                    String(
                        payload.quality
                        || "exhigh"
                    )
                );
            upstreamUrl =
                normalizeNeteaseCdnUrl(
                    resolved?.url
                    || ""
                );
        }

        if (!upstreamUrl) {
            return fail(
                "netease_audio_unavailable",
                "网易未返回可播放音频地址",
                502
            );
        }

        return proxyStream(
            request,
            upstreamUrl
        );
    }
    return fail("unsupported_provider", "云端暂不支持该播放来源", 400);
}
async function handleGetState(env, user) {
    const row = await env.DB
        .prepare(`SELECT favorites_json,
                history_json,
                settings_json
           FROM user_state
          WHERE user_id = ?`)
        .bind(user.id)
        .first();
    return ok({
        state: {
            favorites: JSON.parse(row?.favorites_json
                || "[]"),
            history: JSON.parse(row?.history_json
                || "[]"),
            settings: JSON.parse(row?.settings_json
                || "{}")
        }
    });
}
async function handlePutState(request, env, user) {
    const body = await request.json()
        .catch(() => ({}));
    const favorites = safeArray(body?.favorites);
    const history = safeArray(body?.history)
        .slice(0, 100);
    const settings = safeObject(body?.settings);
    await env.DB
        .prepare(`INSERT INTO user_state(
         user_id,
         favorites_json,
         history_json,
         settings_json,
         updated_at
       ) VALUES (?, ?, ?, ?, ?)
       ON CONFLICT(user_id)
       DO UPDATE SET
         favorites_json = excluded.favorites_json,
         history_json = excluded.history_json,
         settings_json = excluded.settings_json,
         updated_at = excluded.updated_at`)
        .bind(user.id, JSON.stringify(favorites), JSON.stringify(history), JSON.stringify(settings), Date.now())
        .run();
    return ok();
}
async function putProviderSession(request, env, user, provider) {
    if (provider !== "netease") {
        return fail("unsupported_provider", "目前云端会话仅支持网易云", 400);
    }
    const body = await request.json()
        .catch(() => ({}));
    const cookie = String(body?.cookie || "")
        .trim();
    if (!cookie) {
        return fail("missing_cookie", "没有收到当前用户自己的网易云会话");
    }
    const encrypted = await encryptText(JSON.stringify({
        cookie,
        userAgent: String(body?.userAgent || ""),
        pageUrl: String(body?.pageUrl || "")
    }), env.SESSION_MASTER_KEY);
    await env.DB
        .prepare(`INSERT INTO provider_sessions(
         user_id,
         provider,
         encrypted_session,
         updated_at
       ) VALUES (?, ?, ?, ?)
       ON CONFLICT(user_id, provider)
       DO UPDATE SET
         encrypted_session = excluded.encrypted_session,
         updated_at = excluded.updated_at`)
        .bind(user.id, provider, encrypted, Date.now())
        .run();
    return ok({
        provider,
        playbackOnly: true
    });
}
async function deleteProviderSession(env, user, provider) {
    await env.DB
        .prepare(`DELETE FROM provider_sessions
        WHERE user_id = ?
          AND provider = ?`)
        .bind(user.id, provider)
        .run();
    return ok({
        provider
    });
}
async function adminCatalogUpsert(request, env) {
    if (!requireAdmin(request, env)) {
        return fail("forbidden", "管理员密钥错误", 403);
    }
    const body = await request.json()
        .catch(() => ({}));
    const id = String(body?.id
        || crypto.randomUUID());
    const title = String(body?.title || "")
        .trim();
    const r2Key = String(body?.r2Key || "")
        .trim();
    if (!title
        || !r2Key) {
        return fail("invalid_track", "title 和 r2Key 必填");
    }
    await env.DB
        .prepare(`INSERT INTO track_catalog(
         id,
         provider,
         source_id,
         title,
         artist,
         album,
         cover,
         duration,
         access,
         r2_key,
         created_at
       ) VALUES (?, 'cloud', ?, ?, ?, ?, ?, ?, 'cloud', ?, ?)
       ON CONFLICT(id)
       DO UPDATE SET
         title = excluded.title,
         artist = excluded.artist,
         album = excluded.album,
         cover = excluded.cover,
         duration = excluded.duration,
         r2_key = excluded.r2_key`)
        .bind(id, id, title, String(body?.artist || ""), String(body?.album || ""), String(body?.cover || ""), Number(body?.duration || 0), r2Key, Date.now())
        .run();
    return ok({
        track: {
            id,
            providerId: "cloud",
            r2Key
        }
    });
}
export default {
    async fetch(request, env) {
        if (request.method === "OPTIONS") {
            return new Response(null, {
                status: 204,
                headers: {
                    "Access-Control-Allow-Origin": "*",
                    "Access-Control-Allow-Headers": "Authorization,Content-Type,X-Admin-Key",
                    "Access-Control-Allow-Methods": "GET,POST,PUT,DELETE,OPTIONS",
                    "Access-Control-Max-Age": "86400"
                }
            });
        }
        const url = new URL(request.url);
        if (url.pathname === "/health") {
            return ok({
                service: "evolvemusic-cloud",
                version: "0.10.8",
                cloudFirst: true
            });
        }
        if (url.pathname === "/debug/netease-search"
            && request.method === "GET") {
            return debugNeteaseSearch(request);
        }
        if (url.pathname === "/debug/netease-audio"
            && request.method === "GET") {
            return debugNeteaseAudio(request);
        }
        if (url.pathname === "/v1/auth/bootstrap"
            && request.method === "POST") {
            return bootstrapUser(env);
        }
        if (url.pathname === "/v1/artwork"
            && (
                request.method === "GET"
                || request.method === "HEAD"
            )) {
            return handleArtwork(request);
        }
        if (url.pathname === "/v1/audio"
            && (
                request.method === "GET"
                || request.method === "HEAD"
            )) {
            return handleAudio(request, env);
        }
        if (url.pathname === "/v1/admin/catalog"
            && request.method === "POST") {
            return adminCatalogUpsert(request, env);
        }
        const user = await authenticate(request, env);
        if (!user) {
            return fail("unauthorized", "需要有效的 EvolveMusic 云端用户令牌", 401);
        }
        if (url.pathname === "/v1/search"
            && request.method === "GET") {
            return handleSearch(
                request,
                env,
                user
            );
        }
        if (url.pathname === "/v1/home"
            && request.method === "GET") {
            return handleHome(env, user);
        }
        if (url.pathname === "/v1/playlist"
            && request.method === "GET") {
            return handlePlaylist(request, env, user);
        }
        if (url.pathname === "/v1/lyrics"
            && request.method === "GET") {
            return handleLyrics(request, env);
        }
        if (url.pathname === "/v1/stream-ticket"
            && request.method === "POST") {
            return createStreamTicket(request, env, user);
        }
        if (url.pathname === "/v1/me/state"
            && request.method === "GET") {
            return handleGetState(env, user);
        }
        if (url.pathname === "/v1/me/state"
            && request.method === "PUT") {
            return handlePutState(request, env, user);
        }
        if (url.pathname === "/v1/me/provider-session/netease"
            && request.method === "PUT") {
            return putProviderSession(request, env, user, "netease");
        }
        if (url.pathname === "/v1/me/provider-session/netease"
            && request.method === "DELETE") {
            return deleteProviderSession(env, user, "netease");
        }
        return fail("not_found", "接口不存在", 404);
    }
};
