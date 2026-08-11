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
let betaSchemaReady = false;

async function ensureBetaSchema(env) {
    if (betaSchemaReady)
        return;
    if (!env?.DB || typeof env.DB.prepare !== "function")
        throw new Error("D1 binding DB is missing");

    await env.DB.prepare(`CREATE TABLE IF NOT EXISTS users(
        id TEXT PRIMARY KEY,
        token_hash TEXT NOT NULL,
        username TEXT,
        username_key TEXT,
        email TEXT,
        email_key TEXT,
        email_verified_at INTEGER,
        password_salt TEXT,
        password_hash TEXT,
        password_iterations INTEGER,
        password_scheme TEXT,
        created_at INTEGER NOT NULL,
        last_seen_at INTEGER NOT NULL
    )`).run();

    const columnsResult = await env.DB.prepare(`PRAGMA table_info(users)`).all();
    const columns = new Set((columnsResult?.results || []).map((row) => String(row.name || "")));
    const additions = [
        ["username", "TEXT"],
        ["username_key", "TEXT"],
        ["email", "TEXT"],
        ["email_key", "TEXT"],
        ["email_verified_at", "INTEGER"],
        ["password_salt", "TEXT"],
        ["password_hash", "TEXT"],
        ["password_iterations", "INTEGER"],
        ["password_scheme", "TEXT"],
        ["display_name", "TEXT"],
        ["bio", "TEXT"],
        ["avatar_data", "TEXT"]
    ];
    for (const [name, type] of additions) {
        if (!columns.has(name))
            await env.DB.prepare(`ALTER TABLE users ADD COLUMN ${name} ${type}`).run();
    }

    await env.DB.batch([
        env.DB.prepare(`CREATE UNIQUE INDEX IF NOT EXISTS idx_users_username_key
            ON users(username_key) WHERE username_key IS NOT NULL`),
        env.DB.prepare(`CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email_key
            ON users(email_key) WHERE email_key IS NOT NULL`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS email_verifications(
            email TEXT NOT NULL COLLATE NOCASE,
            purpose TEXT NOT NULL,
            code_hash TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            resend_after INTEGER NOT NULL,
            attempts INTEGER NOT NULL DEFAULT 0,
            verified_at INTEGER,
            consumed_at INTEGER,
            request_ip_hash TEXT,
            provider_message_id TEXT,
            PRIMARY KEY(email, purpose)
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_email_verifications_expires
            ON email_verifications(expires_at)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS auth_rate_limits(
            bucket_key TEXT PRIMARY KEY,
            window_start INTEGER NOT NULL,
            count INTEGER NOT NULL DEFAULT 0,
            expires_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_auth_rate_limits_expires
            ON auth_rate_limits(expires_at)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS account_sessions(
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            token_hash TEXT NOT NULL UNIQUE,
            created_at INTEGER NOT NULL,
            last_seen_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_account_sessions_user
            ON account_sessions(user_id)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS custom_playlists(
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT '',
            tracks_json TEXT NOT NULL DEFAULT '[]',
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_custom_playlists_user
            ON custom_playlists(user_id, updated_at DESC)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS user_state(
            user_id TEXT PRIMARY KEY,
            favorites_json TEXT NOT NULL DEFAULT '[]',
            history_json TEXT NOT NULL DEFAULT '[]',
            settings_json TEXT NOT NULL DEFAULT '{}',
            updated_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS provider_sessions(
            user_id TEXT NOT NULL,
            provider TEXT NOT NULL,
            encrypted_session TEXT NOT NULL,
            updated_at INTEGER NOT NULL,
            PRIMARY KEY(user_id, provider)
        )`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS track_catalog(
            id TEXT PRIMARY KEY,
            provider TEXT NOT NULL,
            source_id TEXT NOT NULL,
            title TEXT NOT NULL,
            artist TEXT NOT NULL DEFAULT '',
            album TEXT NOT NULL DEFAULT '',
            cover TEXT NOT NULL DEFAULT '',
            duration INTEGER NOT NULL DEFAULT 0,
            access TEXT NOT NULL DEFAULT 'unknown',
            r2_key TEXT NOT NULL DEFAULT '',
            created_at INTEGER NOT NULL
        )`)
    ]);

    const playlistColumnsResult = await env.DB.prepare(`PRAGMA table_info(custom_playlists)`).all();
    const playlistColumns = new Set((playlistColumnsResult?.results || []).map((row) => String(row.name || "")));
    const playlistAdditions = [
        ["cover_data", "TEXT"],
        ["is_public", "INTEGER NOT NULL DEFAULT 0"],
        ["published_at", "INTEGER"]
    ];
    for (const [name, type] of playlistAdditions) {
        if (!playlistColumns.has(name))
            await env.DB.prepare(`ALTER TABLE custom_playlists ADD COLUMN ${name} ${type}`).run();
    }

    await env.DB.batch([
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS user_follows(
            follower_id TEXT NOT NULL,
            followed_id TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            PRIMARY KEY(follower_id, followed_id)
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_user_follows_followed ON user_follows(followed_id, created_at DESC)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS together_rooms(
            code TEXT PRIMARY KEY,
            host_user_id TEXT NOT NULL,
            state_json TEXT NOT NULL DEFAULT '{}',
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS together_members(
            room_code TEXT NOT NULL,
            user_id TEXT NOT NULL,
            joined_at INTEGER NOT NULL,
            last_seen_at INTEGER NOT NULL,
            PRIMARY KEY(room_code, user_id)
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_together_members_user ON together_members(user_id, last_seen_at DESC)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS together_invites(
            id TEXT PRIMARY KEY,
            room_code TEXT NOT NULL,
            from_user_id TEXT NOT NULL,
            to_user_id TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_together_invites_to ON together_invites(to_user_id, created_at DESC)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS together_messages(
            seq INTEGER PRIMARY KEY AUTOINCREMENT,
            id TEXT NOT NULL UNIQUE,
            room_code TEXT NOT NULL,
            user_id TEXT NOT NULL,
            kind TEXT NOT NULL,
            body TEXT NOT NULL DEFAULT '',
            attachment_id TEXT NOT NULL DEFAULT '',
            created_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_together_messages_room ON together_messages(room_code, seq)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS together_attachments(
            id TEXT PRIMARY KEY,
            room_code TEXT NOT NULL,
            user_id TEXT NOT NULL,
            file_name TEXT NOT NULL,
            mime_type TEXT NOT NULL,
            size_bytes INTEGER NOT NULL,
            storage_key TEXT NOT NULL DEFAULT '',
            inline_data TEXT NOT NULL DEFAULT '',
            created_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_together_attachments_room ON together_attachments(room_code, created_at)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS together_voice_chunks(
            seq INTEGER PRIMARY KEY AUTOINCREMENT,
            room_code TEXT NOT NULL,
            user_id TEXT NOT NULL,
            payload TEXT NOT NULL,
            created_at INTEGER NOT NULL
        )`),
        env.DB.prepare(`CREATE INDEX IF NOT EXISTS idx_together_voice_room ON together_voice_chunks(room_code, seq)`)
    ]);

    betaSchemaReady = true;
}

function normalizeUsername(value) {
    return String(value || "").trim().normalize("NFKC");
}

function usernameKey(value) {
    return normalizeUsername(value).toLocaleLowerCase("en-US");
}

function validUsername(value) {
    const name = normalizeUsername(value);
    if (name.length < 3 || name.length > 32)
        return false;
    return /^[\p{L}\p{N}_.-]+$/u.test(name);
}

function validPassword(value) {
    const password = String(value || "");
    return password.length >= 8 && password.length <= 128;
}

async function derivePasswordHash(password, salt, iterations) {
    const keyMaterial = await crypto.subtle.importKey(
        "raw",
        encoder.encode(String(password)),
        "PBKDF2",
        false,
        ["deriveBits"]
    );
    const bits = await crypto.subtle.deriveBits({
        name: "PBKDF2",
        salt: fromBase64Url(salt),
        iterations,
        hash: "SHA-256"
    }, keyMaterial, 256);
    return base64Url(new Uint8Array(bits));
}

function accountPasswordSecret(env) {
    return String(
        env?.PASSWORD_PEPPER
        || env?.SESSION_MASTER_KEY
        || env?.STREAM_SIGNING_KEY
        || ""
    ).trim();
}

async function derivePasswordHashV2(password, salt, usernameKeyValue, env) {
    // Legacy v0.15.1 account scheme. Kept only so existing users can log in
    // once and be transparently upgraded to the slower PBKDF2 scheme below.
    const secret = accountPasswordSecret(env);
    if (!secret)
        throw new Error("auth password secret missing");
    const key = await crypto.subtle.importKey(
        "raw",
        encoder.encode(secret),
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign"]
    );
    const payload = `${String(usernameKeyValue || "")}\n${String(salt || "")}\n${String(password || "")}`;
    const signature = await crypto.subtle.sign(
        "HMAC",
        key,
        encoder.encode(payload)
    );
    return base64Url(new Uint8Array(signature));
}

// Cloudflare Workers Free only has a very small per-request CPU allowance.
// A 210k-round PBKDF2 inside /register can therefore be terminated after the
// email code has already succeeded. New accounts use a server-peppered HMAC
// with a per-account salt; the old PBKDF2 scheme remains readable below so
// existing accounts are not invalidated.
const PASSWORD_ITERATIONS = 1;
const PASSWORD_SCHEME = "hmac-sha256-pepper-v3";
const LEGACY_PBKDF2_SCHEME = "pbkdf2-sha256-pepper-v2";

async function derivePasswordHashV3(password, salt, env, iterations = PASSWORD_ITERATIONS) {
    const secret = accountPasswordSecret(env);
    if (!secret)
        throw new Error("auth password secret missing");
    const key = await crypto.subtle.importKey(
        "raw",
        encoder.encode(secret),
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign"]
    );
    const signature = await crypto.subtle.sign(
        "HMAC",
        key,
        encoder.encode(`${String(salt || "")}\n${String(password || "")}`)
    );
    return base64Url(new Uint8Array(signature));
}

async function deriveLegacyPbkdf2PasswordHash(password, salt, env, iterations = 210000) {
    const secret = accountPasswordSecret(env);
    if (!secret)
        throw new Error("auth password secret missing");
    const pepperKey = await crypto.subtle.importKey(
        "raw",
        encoder.encode(secret),
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign"]
    );
    const peppered = await crypto.subtle.sign(
        "HMAC",
        pepperKey,
        encoder.encode(String(password || ""))
    );
    const keyMaterial = await crypto.subtle.importKey(
        "raw",
        peppered,
        "PBKDF2",
        false,
        ["deriveBits"]
    );
    const bits = await crypto.subtle.deriveBits({
        name: "PBKDF2",
        salt: fromBase64Url(salt),
        iterations: Math.max(100000, Number(iterations || 210000)),
        hash: "SHA-256"
    }, keyMaterial, 256);
    return base64Url(new Uint8Array(bits));
}

function safeEqual(a, b) {
    const left = encoder.encode(String(a || ""));
    const right = encoder.encode(String(b || ""));
    if (left.length !== right.length)
        return false;
    let diff = 0;
    for (let i = 0; i < left.length; ++i)
        diff |= left[i] ^ right[i];
    return diff === 0;
}

async function createAccountSession(env, userId) {
    const token = randomToken(32);
    const tokenHash = await sha256Hex(token);
    const now = Date.now();
    const expiresAt = now + 90 * 24 * 60 * 60 * 1000;
    await env.DB.prepare(`INSERT INTO account_sessions(
        id, user_id, token_hash, created_at, last_seen_at, expires_at
    ) VALUES (?, ?, ?, ?, ?, ?)`)
        .bind(crypto.randomUUID(), userId, tokenHash, now, now, expiresAt)
        .run();
    return { token, expiresAt };
}

async function authenticate(request, env) {
    await ensureBetaSchema(env);
    const authorization = request.headers.get("Authorization") || "";
    if (!authorization.startsWith("Bearer "))
        return null;
    const token = authorization.slice(7).trim();
    if (!token)
        return null;
    const tokenHash = await sha256Hex(token);
    const now = Date.now();
    const row = await env.DB
        .prepare(`SELECT s.id AS session_id, s.user_id AS id, u.username AS username,
                         u.email AS email, u.email_verified_at AS email_verified_at
           FROM account_sessions s
           JOIN users u ON u.id = s.user_id
          WHERE s.token_hash = ?
            AND s.expires_at > ?
            AND u.username_key IS NOT NULL`)
        .bind(tokenHash, now)
        .first();
    if (!row)
        return null;
    await env.DB.batch([
        env.DB.prepare(`UPDATE account_sessions SET last_seen_at = ? WHERE id = ?`)
            .bind(now, row.session_id),
        env.DB.prepare(`UPDATE users SET last_seen_at = ? WHERE id = ?`)
            .bind(now, row.id)
    ]);
    return {
        id: row.id,
        username: String(row.username || ""),
        email: String(row.email || ""),
        emailVerified: Number(row.email_verified_at || 0) > 0
    };
}

async function parseJsonBody(request) {
    try {
        return await request.json();
    } catch {
        return {};
    }
}

function normalizeEmail(value) {
    return String(value || "").trim().normalize("NFKC").toLowerCase();
}

function emailKey(value) {
    return normalizeEmail(value);
}

function validEmail(value) {
    const email = normalizeEmail(value);
    if (!email || email.length > 254 || email.includes(".."))
        return false;
    const at = email.lastIndexOf("@");
    if (at <= 0 || at >= email.length - 3)
        return false;
    const local = email.slice(0, at);
    const domain = email.slice(at + 1);
    if (local.length > 64 || domain.length > 253 || !domain.includes("."))
        return false;
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/u.test(email);
}

function emailRegistrationRequired(env) {
    // Leave disabled until the desktop registration UI is updated. Turning this
    // variable to "1" makes verified email mandatory at the backend boundary.
    return String(env?.REQUIRE_EMAIL_VERIFICATION || "0") === "1";
}

function emailServiceReady(env) {
    return Boolean(
        String(env?.RESEND_API_KEY || "").trim()
        && String(env?.EMAIL_HASH_SECRET || "").trim()
        && String(env?.EMAIL_FROM || "").trim()
    );
}

function emailHashSecret(env) {
    return String(env?.EMAIL_HASH_SECRET || "").trim();
}

async function keyedDigest(value, secret) {
    const key = await crypto.subtle.importKey(
        "raw",
        encoder.encode(String(secret || "")),
        { name: "HMAC", hash: "SHA-256" },
        false,
        ["sign"]
    );
    const signature = await crypto.subtle.sign("HMAC", key, encoder.encode(String(value || "")));
    return base64Url(new Uint8Array(signature));
}

function generateSixDigitCode() {
    // Rejection sampling avoids modulo bias while staying entirely on WebCrypto.
    const max = 0x100000000;
    const ceiling = max - (max % 1000000);
    const word = new Uint32Array(1);
    do {
        crypto.getRandomValues(word);
    } while (word[0] >= ceiling);
    return String(word[0] % 1000000).padStart(6, "0");
}

async function verificationCodeHash(env, email, purpose, code) {
    const secret = emailHashSecret(env);
    if (!secret)
        throw new Error("email hash secret missing");
    return keyedDigest(`${emailKey(email)}\n${String(purpose || "")}\n${String(code || "")}`, secret);
}

async function requestIpHash(request, env) {
    const secret = emailHashSecret(env);
    if (!secret)
        return "";
    const ip = String(
        request.headers.get("CF-Connecting-IP")
        || request.headers.get("X-Forwarded-For")
        || "unknown"
    ).split(",")[0].trim();
    return keyedDigest(`ip:${ip}`, secret);
}

function purposeLabel(purpose) {
    const value = String(purpose || "");
    if (value === "register")
        return "创建 EvolveMusic 账户";
    if (value === "password_reset")
        return "重置账户密码";
    if (value.startsWith("email_change:"))
        return "绑定或更换账户邮箱";
    return "验证邮箱";
}

function escapeEmailHtml(value) {
    return String(value ?? "")
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/\"/g, "&quot;")
        .replace(/'/g, "&#39;");
}

function emailRequestTime() {
    try {
        return new Intl.DateTimeFormat("zh-CN", {
            timeZone: "Asia/Shanghai",
            year: "numeric", month: "2-digit", day: "2-digit",
            hour: "2-digit", minute: "2-digit", second: "2-digit",
            hour12: false
        }).format(new Date()) + "（北京时间）";
    } catch {
        return new Date().toISOString();
    }
}

function renderVerificationEmail(code, purpose) {
    const safePurpose = escapeEmailHtml(purposeLabel(purpose));
    const safeTime = escapeEmailHtml(emailRequestTime());
    const safeCode = escapeEmailHtml(code);
    return `<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <meta name="color-scheme" content="dark light">
  <title>EvolveMusic 邮箱验证码</title>
</head>
<body style="margin:0;padding:0;background:#090B10;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','Microsoft YaHei',Arial,sans-serif;">
<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="width:100%;background:#090B10;padding:36px 14px;">
<tr><td align="center">
<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="width:100%;max-width:620px;border-collapse:separate;background:#11141B;border:1px solid #262B36;border-radius:26px;overflow:hidden;box-shadow:0 24px 70px rgba(0,0,0,.32);">
<tr><td style="height:5px;background:linear-gradient(90deg,#7C5CFC,#22C7D8,#6EE7B7);"></td></tr>
<tr><td style="padding:30px 38px 22px;background:#151923;">
  <table role="presentation" cellpadding="0" cellspacing="0"><tr>
    <td style="width:44px;height:44px;border-radius:14px;background:#F5F7FA;color:#0C0E13;font-size:24px;font-weight:800;text-align:center;vertical-align:middle;">♪</td>
    <td style="padding-left:14px;color:#F8FAFC;font-size:22px;font-weight:760;letter-spacing:-.4px;">EvolveMusic</td>
  </tr></table>
</td></tr>
<tr><td style="padding:36px 38px 10px;">
  <div style="color:#FFFFFF;font-size:29px;line-height:1.35;font-weight:760;">验证你的邮箱</div>
  <div style="color:#9CA3B2;font-size:15px;line-height:1.8;margin-top:12px;">这是你在 EvolveMusic 发起的身份验证。请在应用中输入下面的 6 位验证码。</div>
</td></tr>
<tr><td style="padding:20px 38px;">
  <div style="padding:29px 18px 27px;text-align:center;border-radius:20px;border:1px solid #303747;background:#0C0F15;">
    <div style="color:#747D90;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:2.4px;margin-bottom:14px;">Verification Code</div>
    <div style="color:#FFFFFF;font-family:Consolas,'SFMono-Regular',monospace;font-size:40px;line-height:1.2;font-weight:760;letter-spacing:12px;padding-left:12px;">${safeCode}</div>
    <div style="color:#8F97A7;font-size:13px;margin-top:16px;">验证码将在 <strong style="color:#DCE1EA;">10 分钟</strong> 后失效</div>
  </div>
</td></tr>
<tr><td style="padding:4px 38px 24px;">
  <table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="background:#171B24;border-radius:16px;padding:17px 18px;">
    <tr><td style="color:#7F8796;font-size:13px;padding-bottom:10px;">请求用途</td><td align="right" style="color:#D8DCE4;font-size:13px;padding-bottom:10px;">${safePurpose}</td></tr>
    <tr><td style="color:#7F8796;font-size:13px;">请求时间</td><td align="right" style="color:#D8DCE4;font-size:13px;">${safeTime}</td></tr>
  </table>
</td></tr>
<tr><td style="padding:0 38px 32px;">
  <div style="padding:16px 17px;border-radius:15px;background:#171A21;border:1px solid #252A35;color:#AEB5C1;font-size:13px;line-height:1.7;">🔒 请勿把验证码发送给任何人。EvolveMusic 工作人员不会向你索要验证码。</div>
</td></tr>
<tr><td style="border-top:1px solid #242833;padding:24px 38px 30px;color:#707887;font-size:12px;line-height:1.75;">如果这不是你的操作，可以直接忽略本邮件。<br>此邮件由 EvolveMusic 自动发送，请勿直接回复。<div style="margin-top:15px;color:#505764;">© ${new Date().getUTCFullYear()} EvolveMusic</div></td></tr>
</table>
</td></tr></table>
</body></html>`;
}

function renderVerificationText(code, purpose) {
    return [
        "EvolveMusic 邮箱验证",
        "",
        `用途：${purposeLabel(purpose)}`,
        `验证码：${code}`,
        "有效期：10 分钟",
        "",
        "请勿将验证码提供给任何人。",
        "如果这不是你的操作，请忽略本邮件。"
    ].join("\n");
}

async function sendResendVerificationEmail(env, email, code, purpose) {
    const apiKey = String(env?.RESEND_API_KEY || "").trim();
    const from = String(env?.EMAIL_FROM || "").trim();
    if (!apiKey || !from || !emailHashSecret(env))
        throw new Error("email service secret missing");

    const response = await fetch("https://api.resend.com/emails", {
        method: "POST",
        headers: {
            "Authorization": `Bearer ${apiKey}`,
            "Content-Type": "application/json; charset=utf-8"
        },
        body: JSON.stringify({
            from,
            to: [normalizeEmail(email)],
            subject: `${code} · EvolveMusic 邮箱验证码`,
            html: renderVerificationEmail(code, purpose),
            text: renderVerificationText(code, purpose),
            headers: {
                "X-Entity-Ref-ID": crypto.randomUUID()
            }
        })
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
        console.error("[EvolveMusic email] Resend failed", response.status, String(payload?.message || payload?.name || ""));
        throw new Error(`resend send failed (${response.status})`);
    }
    return String(payload?.id || "");
}

async function consumeRateLimit(env, bucketKey, limit, windowMs) {
    const now = Date.now();
    const key = String(bucketKey || "");
    const existing = await env.DB.prepare(`SELECT window_start, count, expires_at FROM auth_rate_limits WHERE bucket_key = ?`)
        .bind(key).first();
    if (!existing || Number(existing.expires_at || 0) <= now || now - Number(existing.window_start || 0) >= windowMs) {
        await env.DB.prepare(`INSERT INTO auth_rate_limits(bucket_key, window_start, count, expires_at)
            VALUES (?, ?, 1, ?)
            ON CONFLICT(bucket_key) DO UPDATE SET window_start=excluded.window_start, count=1, expires_at=excluded.expires_at`)
            .bind(key, now, now + windowMs).run();
        return { allowed: true, remaining: Math.max(0, limit - 1), retryAfter: 0 };
    }
    const count = Number(existing.count || 0);
    if (count >= limit) {
        return {
            allowed: false,
            remaining: 0,
            retryAfter: Math.max(1, Math.ceil((Number(existing.expires_at || now) - now) / 1000))
        };
    }
    await env.DB.prepare(`UPDATE auth_rate_limits SET count = count + 1 WHERE bucket_key = ?`).bind(key).run();
    return { allowed: true, remaining: Math.max(0, limit - count - 1), retryAfter: 0 };
}

async function enforceEmailSendRateLimit(request, env, email, purpose) {
    const now = Date.now();
    const old = await env.DB.prepare(`SELECT resend_after FROM email_verifications WHERE email = ? AND purpose = ?`)
        .bind(emailKey(email), String(purpose)).first();
    if (old && Number(old.resend_after || 0) > now) {
        const retry = Math.max(1, Math.ceil((Number(old.resend_after) - now) / 1000));
        return json({ ok: false, error: "email_cooldown", message: `请等待 ${retry} 秒后再重新发送`, retryAfter: retry }, 429);
    }

    const ipHash = await requestIpHash(request, env);
    const eHash = await keyedDigest(`email:${emailKey(email)}`, emailHashSecret(env));
    const emailLimit = await consumeRateLimit(env, `mail:e:${eHash}`, 5, 60 * 60 * 1000);
    if (!emailLimit.allowed)
        return fail("email_rate_limited", "验证码发送过于频繁，请稍后再试", 429);
    const ipLimit = await consumeRateLimit(env, `mail:ip:${ipHash}`, 20, 60 * 60 * 1000);
    if (!ipLimit.allowed)
        return fail("email_rate_limited", "当前网络发送验证码过于频繁，请稍后再试", 429);
    return null;
}

async function issueEmailVerification(request, env, email, purpose) {
    await ensureBetaSchema(env);
    if (!emailServiceReady(env))
        return fail("email_service_unavailable", "邮箱验证服务尚未配置完成", 503);
    const normalized = normalizeEmail(email);
    if (!validEmail(normalized))
        return fail("invalid_email", "请输入有效的邮箱地址", 400);

    const limited = await enforceEmailSendRateLimit(request, env, normalized, purpose);
    if (limited)
        return limited;

    const code = generateSixDigitCode();
    const codeHash = await verificationCodeHash(env, normalized, purpose, code);
    const ipHash = await requestIpHash(request, env);
    const now = Date.now();
    const expiresAt = now + 10 * 60 * 1000;
    const resendAfter = now + 60 * 1000;

    await env.DB.prepare(`INSERT INTO email_verifications(
        email,purpose,code_hash,created_at,updated_at,expires_at,resend_after,attempts,verified_at,consumed_at,request_ip_hash,provider_message_id
    ) VALUES (?,?,?,?,?,?,?,0,NULL,NULL,?,NULL)
    ON CONFLICT(email,purpose) DO UPDATE SET
        code_hash=excluded.code_hash,
        updated_at=excluded.updated_at,
        expires_at=excluded.expires_at,
        resend_after=excluded.resend_after,
        attempts=0,
        verified_at=NULL,
        consumed_at=NULL,
        request_ip_hash=excluded.request_ip_hash,
        provider_message_id=NULL`)
        .bind(normalized, String(purpose), codeHash, now, now, expiresAt, resendAfter, ipHash)
        .run();

    try {
        const messageId = await sendResendVerificationEmail(env, normalized, code, purpose);
        await env.DB.prepare(`UPDATE email_verifications SET provider_message_id = ? WHERE email = ? AND purpose = ? AND code_hash = ?`)
            .bind(messageId, normalized, String(purpose), codeHash).run();
    } catch (error) {
        await env.DB.prepare(`DELETE FROM email_verifications WHERE email = ? AND purpose = ? AND code_hash = ?`)
            .bind(normalized, String(purpose), codeHash).run();
        console.error("[EvolveMusic email] verification delivery error", String(error?.message || error));
        return fail("email_send_failed", "验证码邮件暂时发送失败，请稍后重试", 502);
    }

    return ok({ cooldown: 60, expiresIn: 600 });
}

async function verifyEmailCode(env, email, purpose, code, consume = false) {
    const normalized = normalizeEmail(email);
    const cleanCode = String(code || "").trim();
    if (!validEmail(normalized) || !/^\d{6}$/.test(cleanCode))
        return { ok: false, response: fail("invalid_verification_code", "验证码不正确", 400) };

    const row = await env.DB.prepare(`SELECT code_hash,expires_at,attempts,verified_at,consumed_at FROM email_verifications
        WHERE email = ? AND purpose = ?`).bind(normalized, String(purpose)).first();
    const now = Date.now();
    if (!row || Number(row.consumed_at || 0) > 0 || Number(row.expires_at || 0) <= now) {
        return { ok: false, response: fail("verification_expired", "验证码已失效，请重新获取", 400) };
    }
    if (Number(row.attempts || 0) >= 5) {
        return { ok: false, response: fail("verification_locked", "验证码错误次数过多，请重新获取", 429) };
    }

    const expected = await verificationCodeHash(env, normalized, purpose, cleanCode);
    if (!safeEqual(expected, String(row.code_hash || ""))) {
        const attempts = Number(row.attempts || 0) + 1;
        await env.DB.prepare(`UPDATE email_verifications SET attempts = ?, updated_at = ? WHERE email = ? AND purpose = ?`)
            .bind(attempts, now, normalized, String(purpose)).run();
        const remaining = Math.max(0, 5 - attempts);
        return {
            ok: false,
            response: json({ ok: false, error: "invalid_verification_code", message: "验证码不正确", attemptsRemaining: remaining }, 400)
        };
    }

    await env.DB.prepare(`UPDATE email_verifications SET verified_at = COALESCE(verified_at, ?), consumed_at = ?, updated_at = ?
        WHERE email = ? AND purpose = ?`)
        .bind(now, consume ? now : null, now, normalized, String(purpose)).run();
    return { ok: true, email: normalized, verifiedAt: Number(row.verified_at || now) || now };
}

async function consumePreviouslyVerifiedEmail(env, email, purpose) {
    const normalized = normalizeEmail(email);
    const row = await env.DB.prepare(`SELECT verified_at,expires_at,consumed_at FROM email_verifications WHERE email = ? AND purpose = ?`)
        .bind(normalized, String(purpose)).first();
    const now = Date.now();
    if (!row || !Number(row.verified_at || 0) || Number(row.expires_at || 0) <= now || Number(row.consumed_at || 0) > 0)
        return false;
    await env.DB.prepare(`UPDATE email_verifications SET consumed_at = ?, updated_at = ? WHERE email = ? AND purpose = ?`)
        .bind(now, now, normalized, String(purpose)).run();
    return true;
}

async function verifyStoredPassword(row, password, env) {
    if (!row || !row.password_salt || !row.password_hash)
        return { valid: false, upgrade: false };
    const scheme = String(row.password_scheme || "").trim();
    let calculated = "";
    if (scheme === PASSWORD_SCHEME) {
        calculated = await derivePasswordHashV3(password, String(row.password_salt), env, Number(row.password_iterations || PASSWORD_ITERATIONS));
    } else if (scheme === LEGACY_PBKDF2_SCHEME) {
        calculated = await deriveLegacyPbkdf2PasswordHash(
            password,
            String(row.password_salt),
            env,
            Number(row.password_iterations || 210000)
        );
    } else if (scheme === "hmac-sha256-pepper-v1") {
        calculated = await derivePasswordHashV2(
            password,
            String(row.password_salt),
            String(row.username_key || usernameKey(row.username || "")),
            env
        );
    } else {
        // Compatibility for the short-lived unpeppered PBKDF2 beta format.
        const iterations = Math.max(100000, Number(row.password_iterations || 210000));
        calculated = await derivePasswordHash(password, String(row.password_salt), iterations);
    }
    const valid = safeEqual(calculated, String(row.password_hash || ""));
    return { valid, upgrade: valid && scheme !== PASSWORD_SCHEME };
}

async function upgradePasswordHashIfNeeded(env, row, password) {
    const checked = await verifyStoredPassword(row, password, env);
    if (!checked.valid)
        return false;
    if (checked.upgrade) {
        const salt = randomToken(16);
        const hash = await derivePasswordHashV3(password, salt, env, PASSWORD_ITERATIONS);
        await env.DB.prepare(`UPDATE users SET password_salt=?, password_hash=?, password_iterations=?, password_scheme=? WHERE id=?`)
            .bind(salt, hash, PASSWORD_ITERATIONS, PASSWORD_SCHEME, row.id).run();
    }
    return true;
}

async function sendRegisterEmailCode(request, env) {
    await ensureBetaSchema(env);
    if (!emailServiceReady(env))
        return fail("email_service_unavailable", "邮箱验证服务尚未配置完成", 503);
    const body = await parseJsonBody(request);
    const email = normalizeEmail(body?.email);
    if (!validEmail(email))
        return fail("invalid_email", "请输入有效的邮箱地址", 400);
    const exists = await env.DB.prepare(`SELECT id FROM users WHERE email_key = ?`).bind(emailKey(email)).first();
    if (exists)
        return fail("email_taken", "这个邮箱已经绑定 EvolveMusic 账户", 409);
    return issueEmailVerification(request, env, email, "register");
}

async function verifyEmailRoute(request, env) {
    await ensureBetaSchema(env);
    const body = await parseJsonBody(request);
    const purpose = String(body?.purpose || "register");
    if (purpose !== "register" && purpose !== "password_reset")
        return fail("invalid_verification_purpose", "不支持的邮箱验证用途", 400);
    const result = await verifyEmailCode(env, body?.email, purpose, body?.code, false);
    if (!result.ok)
        return result.response;
    return ok({ verified: true, email: result.email });
}

async function sendPasswordResetCode(request, env) {
    await ensureBetaSchema(env);
    if (!emailServiceReady(env))
        return fail("email_service_unavailable", "邮箱验证服务尚未配置完成", 503);
    const body = await parseJsonBody(request);
    const email = normalizeEmail(body?.email);
    if (!validEmail(email))
        return fail("invalid_email", "请输入有效的邮箱地址", 400);

    // Always return the same success shape when an account is absent to avoid
    // turning this endpoint into an account-enumeration oracle.
    const account = await env.DB.prepare(`SELECT id FROM users WHERE email_key = ? AND email_verified_at IS NOT NULL`)
        .bind(emailKey(email)).first();
    if (!account) {
        const limited = await enforceEmailSendRateLimit(request, env, email, "password_reset");
        if (limited)
            return limited;
        return ok({ cooldown: 60, expiresIn: 600 });
    }
    return issueEmailVerification(request, env, email, "password_reset");
}

async function resetPasswordWithEmail(request, env) {
    await ensureBetaSchema(env);
    const body = await parseJsonBody(request);
    const email = normalizeEmail(body?.email);
    const code = String(body?.code || body?.emailCode || "").trim();
    const password = String(body?.password || body?.newPassword || "");
    if (!validPassword(password))
        return fail("invalid_password", "密码长度需为 8-128 位", 400);

    const verification = await verifyEmailCode(env, email, "password_reset", code, false);
    if (!verification.ok)
        return verification.response;
    const row = await env.DB.prepare(`SELECT id FROM users WHERE email_key = ? AND email_verified_at IS NOT NULL`)
        .bind(emailKey(email)).first();
    if (!row)
        return fail("verification_expired", "验证码已失效，请重新获取", 400);

    const salt = randomToken(16);
    const hash = await derivePasswordHashV3(password, salt, env, PASSWORD_ITERATIONS);
    const now = Date.now();
    await env.DB.batch([
        env.DB.prepare(`UPDATE users SET password_salt=?,password_hash=?,password_iterations=?,password_scheme=?,last_seen_at=? WHERE id=?`)
            .bind(salt, hash, PASSWORD_ITERATIONS, PASSWORD_SCHEME, now, row.id),
        env.DB.prepare(`DELETE FROM account_sessions WHERE user_id = ?`).bind(row.id),
        env.DB.prepare(`UPDATE email_verifications SET consumed_at=?,updated_at=? WHERE email=? AND purpose='password_reset'`)
            .bind(now, now, email)
    ]);
    return ok({ reset: true });
}

async function sendEmailChangeCode(request, env, user) {
    await ensureBetaSchema(env);
    const body = await parseJsonBody(request);
    const newEmail = normalizeEmail(body?.newEmail || body?.email);
    const password = String(body?.password || "");
    if (!validEmail(newEmail))
        return fail("invalid_email", "请输入有效的邮箱地址", 400);
    if (!validPassword(password))
        return fail("password_required", "绑定或更换邮箱前需要确认当前密码", 401);

    const account = await env.DB.prepare(`SELECT id,username,username_key,password_salt,password_hash,password_iterations,password_scheme,email_key FROM users WHERE id=?`)
        .bind(user.id).first();
    if (!(await upgradePasswordHashIfNeeded(env, account, password)))
        return fail("invalid_credentials", "当前密码不正确", 401);
    if (String(account?.email_key || "") === emailKey(newEmail))
        return fail("email_unchanged", "这已经是当前账户邮箱", 400);
    const exists = await env.DB.prepare(`SELECT id FROM users WHERE email_key=? AND id<>?`).bind(emailKey(newEmail), user.id).first();
    if (exists)
        return fail("email_taken", "这个邮箱已经绑定其他 EvolveMusic 账户", 409);
    return issueEmailVerification(request, env, newEmail, `email_change:${user.id}`);
}

async function applyEmailChange(request, env, user) {
    await ensureBetaSchema(env);
    const body = await parseJsonBody(request);
    const newEmail = normalizeEmail(body?.newEmail || body?.email);
    const code = String(body?.code || body?.emailCode || "").trim();
    const password = String(body?.password || "");
    if (!validEmail(newEmail))
        return fail("invalid_email", "请输入有效的邮箱地址", 400);
    if (!validPassword(password))
        return fail("password_required", "绑定或更换邮箱前需要确认当前密码", 401);

    const account = await env.DB.prepare(`SELECT id,username,username_key,password_salt,password_hash,password_iterations,password_scheme FROM users WHERE id=?`)
        .bind(user.id).first();
    if (!(await upgradePasswordHashIfNeeded(env, account, password)))
        return fail("invalid_credentials", "当前密码不正确", 401);
    const exists = await env.DB.prepare(`SELECT id FROM users WHERE email_key=? AND id<>?`).bind(emailKey(newEmail), user.id).first();
    if (exists)
        return fail("email_taken", "这个邮箱已经绑定其他 EvolveMusic 账户", 409);

    const verification = await verifyEmailCode(env, newEmail, `email_change:${user.id}`, code, false);
    if (!verification.ok)
        return verification.response;
    const now = Date.now();
    await env.DB.batch([
        env.DB.prepare(`UPDATE users SET email=?,email_key=?,email_verified_at=? WHERE id=?`)
            .bind(newEmail, emailKey(newEmail), now, user.id),
        env.DB.prepare(`UPDATE email_verifications SET consumed_at=?,updated_at=? WHERE email=? AND purpose=?`)
            .bind(now, now, newEmail, `email_change:${user.id}`)
    ]);
    return ok({ email: newEmail, emailVerified: true });
}

function accountBackendFailure(error, env) {
    const detail = String(error?.message || error || "").trim();
    if (!env?.DB || typeof env.DB.prepare !== "function" || /binding DB is missing/i.test(detail)) {
        return fail("account_db_missing", "账户服务未绑定 Cloudflare D1。请在 Worker 的 Bindings 中把 D1 数据库绑定为 DB。", 503);
    }
    if (/D1|database|no such table|no such column|constraint|SQL/i.test(detail)) {
        return fail("account_db_error", "账户数据库初始化失败。请确认两个云端节点绑定同一个 D1（变量名 DB），然后重新部署 Worker。", 503);
    }
    if (/auth password secret missing/i.test(detail)) {
        return fail("account_secret_missing", "账户服务缺少密码保护密钥。请在 Worker 中配置 PASSWORD_PEPPER，或保留现有 SESSION_MASTER_KEY。", 503);
    }
    if (/email hash secret missing|email service secret missing/i.test(detail)) {
        return fail("email_service_unavailable", "邮箱验证服务尚未配置完成", 503);
    }
    return fail("account_backend_error", "账户服务暂时不可用，请稍后重试。", 503);
}

async function registerAccount(request, env) {
    await ensureBetaSchema(env);
    const body = await parseJsonBody(request);
    const username = normalizeUsername(body?.username);
    const password = String(body?.password || "");
    const email = normalizeEmail(body?.email);
    const emailCode = String(body?.emailCode || body?.code || "").trim();
    const requireEmail = emailRegistrationRequired(env);

    if (body?.acceptTerms !== true)
        return fail("terms_required", "注册前需要同意测试版免责声明和使用条款", 400);
    if (!validUsername(username))
        return fail("invalid_username", "用户名需为 3-32 位中文、字母、数字、下划线、短横线或点", 400);
    if (!validPassword(password))
        return fail("invalid_password", "密码长度需为 8-128 位", 400);
    if (requireEmail && !email)
        return fail("email_required", "注册需要先验证邮箱", 400);
    if (email && !validEmail(email))
        return fail("invalid_email", "请输入有效的邮箱地址", 400);

    const key = usernameKey(username);
    const exists = await env.DB.prepare(`SELECT id FROM users WHERE username_key = ?`).bind(key).first();
    if (exists)
        return fail("username_taken", "这个用户名已经被注册", 409);
    if (email) {
        const emailExists = await env.DB.prepare(`SELECT id FROM users WHERE email_key = ?`).bind(emailKey(email)).first();
        if (emailExists)
            return fail("email_taken", "这个邮箱已经绑定 EvolveMusic 账户", 409);
        if (!/^\d{6}$/.test(emailCode))
            return fail("verification_required", "请输入邮箱收到的 6 位验证码", 400);
        const verification = await verifyEmailCode(env, email, "register", emailCode, false);
        if (!verification.ok)
            return verification.response;
    }

    const id = crypto.randomUUID();
    const salt = randomToken(16);
    const passwordHash = await derivePasswordHashV3(password, salt, env, PASSWORD_ITERATIONS);
    const now = Date.now();
    const legacyTokenHash = await sha256Hex(randomToken(32));
    const verifiedAt = email ? now : null;

    try {
        await env.DB.batch([
            env.DB.prepare(`INSERT INTO users(
                id, token_hash, username, username_key, email, email_key, email_verified_at,
                password_salt, password_hash, password_iterations, password_scheme, created_at, last_seen_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)` )
                .bind(
                    id, legacyTokenHash, username, key,
                    email || null, email ? emailKey(email) : null, verifiedAt,
                    salt, passwordHash, PASSWORD_ITERATIONS, PASSWORD_SCHEME, now, now
                ),
            env.DB.prepare(`INSERT OR IGNORE INTO user_state(
                user_id, favorites_json, history_json, settings_json, updated_at
            ) VALUES (?, '[]', '[]', '{}', ?)` )
                .bind(id, now)
        ]);
    } catch (error) {
        if (String(error?.message || error).toLowerCase().includes("unique")) {
            const emailConflict = email && await env.DB.prepare(`SELECT id FROM users WHERE email_key=?`).bind(emailKey(email)).first();
            if (emailConflict)
                return fail("email_taken", "这个邮箱已经绑定 EvolveMusic 账户", 409);
            return fail("username_taken", "这个用户名已经被注册", 409);
        }
        throw error;
    }

    if (email)
        await consumePreviouslyVerifiedEmail(env, email, "register");

    const session = await createAccountSession(env, id);
    return ok({
        user: { id, username, email: email || "", emailVerified: Boolean(email) },
        token: session.token,
        expiresAt: session.expiresAt
    });
}

async function loginAccount(request, env) {
    await ensureBetaSchema(env);
    const body = await parseJsonBody(request);
    const identifier = String(body?.identifier ?? body?.username ?? body?.email ?? "").trim();
    const password = String(body?.password || "");
    if (!identifier || !validPassword(password))
        return fail("invalid_credentials", "用户名/邮箱或密码错误", 401);

    let row = null;
    if (identifier.includes("@") && validEmail(identifier)) {
        row = await env.DB.prepare(`SELECT id,username,username_key,email,email_key,email_verified_at,password_salt,password_hash,password_iterations,password_scheme
            FROM users WHERE email_key=?`).bind(emailKey(identifier)).first();
    } else {
        const normalizedUsername = normalizeUsername(identifier);
        if (!validUsername(normalizedUsername))
            return fail("invalid_credentials", "用户名/邮箱或密码错误", 401);
        row = await env.DB.prepare(`SELECT id,username,username_key,email,email_key,email_verified_at,password_salt,password_hash,password_iterations,password_scheme
            FROM users WHERE username_key=?`).bind(usernameKey(normalizedUsername)).first();
    }
    if (!row)
        return fail("invalid_credentials", "用户名/邮箱或密码错误", 401);

    if (!(await upgradePasswordHashIfNeeded(env, row, password)))
        return fail("invalid_credentials", "用户名/邮箱或密码错误", 401);

    const session = await createAccountSession(env, row.id);
    await env.DB.prepare(`DELETE FROM account_sessions WHERE user_id = ? AND expires_at <= ?`)
        .bind(row.id, Date.now()).run();
    return ok({
        user: {
            id: row.id,
            username: String(row.username || identifier),
            email: String(row.email || ""),
            emailVerified: Number(row.email_verified_at || 0) > 0
        },
        token: session.token,
        expiresAt: session.expiresAt
    });
}

async function logoutAccount(request, env, user) {
    const authorization = request.headers.get("Authorization") || "";
    const token = authorization.startsWith("Bearer ") ? authorization.slice(7).trim() : "";
    if (token) {
        const tokenHash = await sha256Hex(token);
        await env.DB.prepare(`DELETE FROM account_sessions WHERE user_id = ? AND token_hash = ?`)
            .bind(user.id, tokenHash).run();
    }
    return ok({ loggedOut: true });
}

function normalizeStoredTrack(value) {
    const track = value && typeof value === "object" ? value : {};
    const keys = ["id", "sourceId", "providerId", "providerName", "title", "artist", "album", "cover", "duration", "access", "playable", "sourceCount", "sourceSummary", "sources"];
    const out = {};
    for (const key of keys) {
        if (track[key] !== undefined)
            out[key] = track[key];
    }
    return out;
}

function playlistRow(row, includeTracks = true, viewerId = "") {
    let tracks = [];
    if (includeTracks) {
        try { tracks = JSON.parse(String(row.tracks_json || "[]")); } catch { tracks = []; }
        if (!Array.isArray(tracks)) tracks = [];
    }
    const ownerId = String(row.owner_id || row.user_id || "");
    const editable = Boolean(viewerId && ownerId && String(viewerId) === ownerId);
    return {
        id: String(row.id || ""),
        name: String(row.name || "未命名歌单"),
        description: String(row.description || ""),
        tracks,
        trackCount: includeTracks ? tracks.length : Number(row.track_count || 0),
        createdAt: Number(row.created_at || 0),
        updatedAt: Number(row.updated_at || 0),
        cover: String(row.cover_data || ""),
        isPublic: Number(row.is_public || 0) === 1,
        publishedAt: Number(row.published_at || 0),
        ownerId,
        ownerUsername: String(row.owner_username || ""),
        ownerName: String(row.owner_name || row.owner_username || ""),
        ownerAvatar: String(row.owner_avatar || ""),
        providerId: "evolve",
        providerName: "EvolveMusic",
        custom: editable,
        editable
    };
}

async function listCustomPlaylists(env, user) {
    const rows = await env.DB.prepare(`SELECT id, user_id, name, description, tracks_json, cover_data, is_public, published_at, created_at, updated_at
        FROM custom_playlists WHERE user_id = ? ORDER BY updated_at DESC`).bind(user.id).all();
    return ok({ playlists: (rows?.results || []).map((row) => playlistRow(row, true, user.id)) });
}

async function createCustomPlaylist(request, env, user) {
    const body = await parseJsonBody(request);
    let name = String(body?.name || "").trim();
    if (!name) name = "新建歌单";
    name = name.slice(0, 48);
    const description = String(body?.description || "").trim().slice(0, 240);
    const id = `pl_${crypto.randomUUID()}`;
    const now = Date.now();
    await env.DB.prepare(`INSERT INTO custom_playlists(
        id, user_id, name, description, tracks_json, created_at, updated_at
    ) VALUES (?, ?, ?, ?, '[]', ?, ?)`)
        .bind(id, user.id, name, description, now, now).run();
    return ok({ playlist: { id, name, description, tracks: [], trackCount: 0, createdAt: now, updatedAt: now, ownerId: user.id, ownerUsername: user.username, providerId: "evolve", providerName: "EvolveMusic", custom: true, editable: true } });
}

async function updateCustomPlaylist(request, env, user, playlistId) {
    const existing = await env.DB.prepare(`SELECT * FROM custom_playlists WHERE id = ? AND user_id = ?`)
        .bind(playlistId, user.id).first();
    if (!existing)
        return fail("playlist_not_found", "歌单不存在", 404);
    const body = await parseJsonBody(request);
    let name = body?.name !== undefined ? String(body.name).trim().slice(0, 48) : String(existing.name || "新建歌单");
    if (!name) name = "新建歌单";
    const description = body?.description !== undefined ? String(body.description).trim().slice(0, 500) : String(existing.description || "");
    let tracks;
    try { tracks = JSON.parse(String(existing.tracks_json || "[]")); } catch { tracks = []; }
    if (!Array.isArray(tracks)) tracks = [];
    if (Array.isArray(body?.tracks)) {
        tracks = body.tracks.slice(0, 500).map(normalizeStoredTrack);
        const encoded = JSON.stringify(tracks);
        if (encoded.length > 900000)
            return fail("playlist_too_large", "歌单数据过大", 413);
    }
    let cover = body?.cover !== undefined ? String(body.cover || "") : String(existing.cover_data || "");
    if (cover.length > 700000)
        return fail("cover_too_large", "歌单封面过大，请换一张更小的图片", 413);
    const isPublic = body?.isPublic !== undefined ? Boolean(body.isPublic) : Number(existing.is_public || 0) === 1;
    const now = Date.now();
    const publishedAt = isPublic ? Number(existing.published_at || now) : null;
    await env.DB.prepare(`UPDATE custom_playlists
        SET name = ?, description = ?, tracks_json = ?, cover_data = ?, is_public = ?, published_at = ?, updated_at = ?
        WHERE id = ? AND user_id = ?`)
        .bind(name, description, JSON.stringify(tracks), cover, isPublic ? 1 : 0, publishedAt, now, playlistId, user.id).run();
    return ok({ playlist: playlistRow({ ...existing, id: playlistId, user_id: user.id, name, description, tracks_json: JSON.stringify(tracks), cover_data: cover, is_public: isPublic ? 1 : 0, published_at: publishedAt, updated_at: now }, true, user.id) });
}

async function deleteCustomPlaylist(env, user, playlistId) {
    const result = await env.DB.prepare(`DELETE FROM custom_playlists WHERE id = ? AND user_id = ?`)
        .bind(playlistId, user.id).run();
    return ok({ deleted: Boolean(result?.meta?.changes) });
}


function safeJsonObject(value, fallback = {}) {
    try {
        const parsed = JSON.parse(String(value || "{}"));
        return parsed && typeof parsed === "object" && !Array.isArray(parsed) ? parsed : fallback;
    } catch { return fallback; }
}

async function profileForUser(env, targetId, viewerId = "") {
    const row = await env.DB.prepare(`SELECT id, username, display_name, bio, avatar_data, created_at FROM users WHERE id = ?`)
        .bind(targetId).first();
    if (!row) return null;
    const counts = await env.DB.batch([
        env.DB.prepare(`SELECT COUNT(*) AS n FROM user_follows WHERE followed_id = ?`).bind(targetId),
        env.DB.prepare(`SELECT COUNT(*) AS n FROM user_follows WHERE follower_id = ?`).bind(targetId)
    ]);
    let following = false;
    if (viewerId && viewerId !== targetId) {
        following = Boolean(await env.DB.prepare(`SELECT 1 AS ok FROM user_follows WHERE follower_id = ? AND followed_id = ? LIMIT 1`).bind(viewerId, targetId).first());
    }
    return {
        id: String(row.id || ""),
        username: String(row.username || ""),
        displayName: String(row.display_name || row.username || "Evolve 用户"),
        bio: String(row.bio || ""),
        avatar: String(row.avatar_data || ""),
        followers: Number(counts?.[0]?.results?.[0]?.n || 0),
        followingCount: Number(counts?.[1]?.results?.[0]?.n || 0),
        following,
        createdAt: Number(row.created_at || 0)
    };
}

async function getMyProfile(env, user) {
    return ok({ profile: await profileForUser(env, user.id, user.id) });
}

async function updateMyProfile(request, env, user) {
    const body = await parseJsonBody(request);
    const displayName = String(body?.displayName || user.username || "").trim().slice(0, 32);
    const bio = String(body?.bio || "").trim().slice(0, 180);
    let avatar = body?.avatar !== undefined ? String(body.avatar || "") : null;
    if (avatar !== null && avatar.length > 520000)
        return fail("avatar_too_large", "头像过大，请选择更小的图片", 413);
    if (avatar === null) {
        await env.DB.prepare(`UPDATE users SET display_name = ?, bio = ? WHERE id = ?`).bind(displayName, bio, user.id).run();
    } else {
        await env.DB.prepare(`UPDATE users SET display_name = ?, bio = ?, avatar_data = ? WHERE id = ?`).bind(displayName, bio, avatar, user.id).run();
    }
    return ok({ profile: await profileForUser(env, user.id, user.id) });
}

async function searchUsersRoute(request, env, user) {
    const url = new URL(request.url);
    const q = String(url.searchParams.get("q") || "").trim().toLowerCase().slice(0, 40);
    if (!q) return ok({ users: [] });
    const like = `%${q}%`;
    const rows = await env.DB.prepare(`SELECT u.id, u.username, u.display_name, u.bio, u.avatar_data,
        CASE WHEN f.followed_id IS NULL THEN 0 ELSE 1 END AS is_following
        FROM users u
        LEFT JOIN user_follows f ON f.follower_id = ? AND f.followed_id = u.id
        WHERE u.id <> ? AND (lower(COALESCE(u.username,'')) LIKE ? OR lower(COALESCE(u.display_name,'')) LIKE ?)
        ORDER BY CASE WHEN lower(COALESCE(u.username,'')) = ? THEN 0 ELSE 1 END, u.last_seen_at DESC
        LIMIT 20`).bind(user.id, user.id, like, like, q).all();
    return ok({ users: (rows?.results || []).map((row) => ({
        id: String(row.id || ""), username: String(row.username || ""),
        displayName: String(row.display_name || row.username || "Evolve 用户"),
        bio: String(row.bio || ""), avatar: String(row.avatar_data || ""),
        following: Number(row.is_following || 0) === 1
    })) });
}

async function setFollowRoute(request, env, user, targetId) {
    if (!targetId || targetId === user.id) return fail("invalid_user", "不能关注自己", 400);
    if (!(await env.DB.prepare(`SELECT id FROM users WHERE id = ?`).bind(targetId).first()))
        return fail("user_not_found", "用户不存在", 404);
    if (request.method === "POST") {
        await env.DB.prepare(`INSERT OR IGNORE INTO user_follows(follower_id, followed_id, created_at) VALUES (?, ?, ?)`)
            .bind(user.id, targetId, Date.now()).run();
    } else {
        await env.DB.prepare(`DELETE FROM user_follows WHERE follower_id = ? AND followed_id = ?`).bind(user.id, targetId).run();
    }
    return ok({ following: request.method === "POST" });
}

async function listFollowingRoute(env, user) {
    const rows = await env.DB.prepare(`SELECT u.id, u.username, u.display_name, u.bio, u.avatar_data
        FROM user_follows f JOIN users u ON u.id = f.followed_id
        WHERE f.follower_id = ? ORDER BY f.created_at DESC LIMIT 80`).bind(user.id).all();
    return ok({ users: (rows?.results || []).map((row) => ({
        id:String(row.id||""), username:String(row.username||""), displayName:String(row.display_name||row.username||"Evolve 用户"),
        bio:String(row.bio||""), avatar:String(row.avatar_data||""), following:true
    })) });
}

async function listCommunityPlaylistsRoute(request, env, user) {
    const limit = Math.min(40, Math.max(6, Number(new URL(request.url).searchParams.get("limit") || 24)));
    const rows = await env.DB.prepare(`SELECT p.*, u.id AS owner_id, u.username AS owner_username,
        u.display_name AS owner_name, u.avatar_data AS owner_avatar
        FROM custom_playlists p JOIN users u ON u.id = p.user_id
        WHERE p.is_public = 1
        ORDER BY COALESCE(p.published_at,p.updated_at) DESC, p.updated_at DESC LIMIT ?`).bind(limit).all();
    return ok({ playlists: (rows?.results || []).map((row) => playlistRow(row, true, user.id)) });
}

async function getCommunityPlaylistRoute(env, user, playlistId) {
    const row = await env.DB.prepare(`SELECT p.*, u.id AS owner_id, u.username AS owner_username,
        u.display_name AS owner_name, u.avatar_data AS owner_avatar
        FROM custom_playlists p JOIN users u ON u.id = p.user_id
        WHERE p.id = ? AND (p.is_public = 1 OR p.user_id = ?)`).bind(playlistId, user.id).first();
    if (!row) return fail("playlist_not_found", "社区歌单不存在或未公开", 404);
    const playlist = playlistRow(row, true, user.id);
    return ok({ playlist, tracks: playlist.tracks || [] });
}

function roomCode() {
    const alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    const bytes = crypto.getRandomValues(new Uint8Array(6));
    return Array.from(bytes, (b) => alphabet[b % alphabet.length]).join("");
}


function bytesToBase64(bytes) {
    const view = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes || 0);
    let binary = "";
    for (let i = 0; i < view.length; i += 0x8000)
        binary += String.fromCharCode(...view.subarray(i, Math.min(view.length, i + 0x8000)));
    return btoa(binary);
}

function base64ToBytes(value) {
    const binary = atob(String(value || ""));
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; ++i) bytes[i] = binary.charCodeAt(i);
    return bytes;
}

function safeRoomFileName(value) {
    let name = String(value || "file").normalize("NFKC").replace(/[\\/:*?"<>|\x00-\x1f]/g, "_").trim();
    if (!name) name = "file";
    return name.slice(0, 140);
}

async function destroyTogetherRoom(env, code) {
    const attachments = await env.DB.prepare(`SELECT storage_key FROM together_attachments WHERE room_code = ? AND storage_key <> ''`).bind(code).all();
    if (env.ROOM_FILES && typeof env.ROOM_FILES.delete === "function") {
        for (const row of (attachments?.results || [])) {
            const key = String(row.storage_key || "");
            if (!key) continue;
            try { await env.ROOM_FILES.delete(key); } catch (_) {}
        }
    }
    await env.DB.batch([
        env.DB.prepare(`DELETE FROM together_voice_chunks WHERE room_code = ?`).bind(code),
        env.DB.prepare(`DELETE FROM together_messages WHERE room_code = ?`).bind(code),
        env.DB.prepare(`DELETE FROM together_attachments WHERE room_code = ?`).bind(code),
        env.DB.prepare(`DELETE FROM together_members WHERE room_code = ?`).bind(code),
        env.DB.prepare(`DELETE FROM together_invites WHERE room_code = ?`).bind(code),
        env.DB.prepare(`DELETE FROM together_rooms WHERE code = ?`).bind(code)
    ]);
}

async function togetherRoomAccess(env, user, code) {
    const row = await env.DB.prepare(`SELECT r.host_user_id,r.expires_at,m.user_id AS member_id
        FROM together_rooms r LEFT JOIN together_members m
        ON m.room_code=r.code AND m.user_id=? WHERE r.code=?`).bind(user.id,code).first();
    if (!row) return null;
    if (Number(row.expires_at || 0) <= Date.now()) {
        await destroyTogetherRoom(env, code);
        return null;
    }
    if (String(row.host_user_id || "") !== String(user.id || "") && !row.member_id) return null;
    return row;
}

async function cleanupStaleTogetherRooms(env, now = Date.now()) {
    const rows = await env.DB.prepare(`SELECT r.code FROM together_rooms r
        LEFT JOIN together_members h ON h.room_code=r.code AND h.user_id=r.host_user_id
        WHERE r.expires_at<=? OR h.last_seen_at IS NULL OR h.last_seen_at<? LIMIT 8`)
        .bind(now, now - 6500).all();
    for (const row of (rows?.results || []))
        await destroyTogetherRoom(env, String(row.code || ""));
}

async function listTogetherMessagesRoute(request, env, user, code) {
    const room = await togetherRoomAccess(env, user, code);
    if (!room) return fail("room_not_found", "房间不存在、已结束或你不在房间中", 404);
    const after = Math.max(0, Number(new URL(request.url).searchParams.get("after") || 0));
    let rows;
    if (after <= 0) {
        rows = await env.DB.prepare(`SELECT m.seq,m.id,m.user_id,m.kind,m.body,m.attachment_id,m.created_at,
            u.username,u.display_name,a.file_name,a.mime_type,a.size_bytes
            FROM together_messages m JOIN users u ON u.id=m.user_id
            LEFT JOIN together_attachments a ON a.id=m.attachment_id
            WHERE m.room_code=? ORDER BY m.seq DESC LIMIT 80`).bind(code).all();
        rows = { results: (rows?.results || []).reverse() };
    } else {
        rows = await env.DB.prepare(`SELECT m.seq,m.id,m.user_id,m.kind,m.body,m.attachment_id,m.created_at,
            u.username,u.display_name,a.file_name,a.mime_type,a.size_bytes
            FROM together_messages m JOIN users u ON u.id=m.user_id
            LEFT JOIN together_attachments a ON a.id=m.attachment_id
            WHERE m.room_code=? AND m.seq>? ORDER BY m.seq ASC LIMIT 120`).bind(code, after).all();
    }
    const result = (rows?.results || []).map((r) => ({
        seq:Number(r.seq||0), id:String(r.id||""), kind:String(r.kind||"text"), text:String(r.body||""),
        senderId:String(r.user_id||""), senderUsername:String(r.username||""),
        senderName:String(r.display_name||r.username||"Evolve 用户"), mine:String(r.user_id||"")===String(user.id||""),
        attachmentId:String(r.attachment_id||""), fileName:String(r.file_name||""),
        mimeType:String(r.mime_type||""), size:Number(r.size_bytes||0), createdAt:Number(r.created_at||0)
    }));
    const cursor = result.length ? Number(result[result.length - 1].seq || after) : after;
    return ok({ messages: result, cursor });
}

async function sendTogetherMessageRoute(request, env, user, code) {
    const room = await togetherRoomAccess(env, user, code);
    if (!room) return fail("room_not_found", "房间不存在、已结束或你不在房间中", 404);
    const body = await parseJsonBody(request);
    const text = String(body?.text || "").trim().slice(0, 2000);
    if (!text) return fail("message_empty", "消息不能为空", 400);
    const now = Date.now(), id = `msg_${crypto.randomUUID()}`;
    await env.DB.prepare(`INSERT INTO together_messages(id,room_code,user_id,kind,body,attachment_id,created_at)
        VALUES (?,?,?,'text',?,'',?)`).bind(id, code, user.id, text, now).run();
    return ok({ sent:true, id });
}

async function uploadTogetherAttachmentRoute(request, env, user, code) {
    const room = await togetherRoomAccess(env, user, code);
    if (!room) return fail("room_not_found", "房间不存在、已结束或你不在房间中", 404);
    const url = new URL(request.url);
    const kind = String(url.searchParams.get("kind") || "file") === "image" ? "image" : "file";
    const fileName = safeRoomFileName(url.searchParams.get("name") || (kind === "image" ? "image.jpg" : "file"));
    const mimeType = String(request.headers.get("content-type") || "application/octet-stream").slice(0, 120);
    const buffer = await request.arrayBuffer();
    const size = buffer.byteLength;
    if (!size) return fail("file_empty", "文件为空", 400);
    if (size > 20 * 1024 * 1024) return fail("file_too_large", "房间文件最大 20 MB", 413);

    const attachmentId = `att_${crypto.randomUUID()}`;
    const messageId = `msg_${crypto.randomUUID()}`;
    const now = Date.now();
    let storageKey = "", inlineData = "";
    if (env.ROOM_FILES && typeof env.ROOM_FILES.put === "function") {
        storageKey = `together/${code}/${attachmentId}`;
        await env.ROOM_FILES.put(storageKey, buffer, {
            httpMetadata: { contentType: mimeType },
            customMetadata: { room: code }
        });
    } else {
        // Fully functional zero-extra-binding fallback for screenshots/small files.
        // Larger files use the optional ROOM_FILES R2 binding so D1 never becomes
        // a long-lived blob store.
        if (size > 650 * 1024)
            return fail("room_files_binding_required", "该文件超过 650 KB；请给 Worker 绑定名为 ROOM_FILES 的 R2 存储桶后再发送大文件", 413);
        inlineData = bytesToBase64(new Uint8Array(buffer));
    }
    await env.DB.batch([
        env.DB.prepare(`INSERT INTO together_attachments(id,room_code,user_id,file_name,mime_type,size_bytes,storage_key,inline_data,created_at)
            VALUES (?,?,?,?,?,?,?,?,?)`).bind(attachmentId,code,user.id,fileName,mimeType,size,storageKey,inlineData,now),
        env.DB.prepare(`INSERT INTO together_messages(id,room_code,user_id,kind,body,attachment_id,created_at)
            VALUES (?,?,?,?,?,?,?)`).bind(messageId,code,user.id,kind,"",attachmentId,now)
    ]);
    return ok({ sent:true, attachmentId, messageId });
}

async function downloadTogetherAttachmentRoute(env, user, code, attachmentId) {
    const room = await togetherRoomAccess(env, user, code);
    if (!room) return fail("room_not_found", "房间不存在、已结束或你不在房间中", 404);
    const row = await env.DB.prepare(`SELECT * FROM together_attachments WHERE id=? AND room_code=?`).bind(attachmentId,code).first();
    if (!row) return fail("attachment_not_found", "临时文件已不存在", 404);
    const headers = new Headers();
    headers.set("Content-Type", String(row.mime_type || "application/octet-stream"));
    headers.set("Cache-Control", "no-store");
    headers.set("X-Evolve-File-Name", encodeURIComponent(String(row.file_name || "file")));
    headers.set("X-Evolve-Ephemeral", "room");
    if (String(row.storage_key || "")) {
        if (!env.ROOM_FILES || typeof env.ROOM_FILES.get !== "function")
            return fail("room_storage_unavailable", "临时文件存储当前不可用", 503);
        const object = await env.ROOM_FILES.get(String(row.storage_key));
        if (!object) return fail("attachment_not_found", "临时文件已被清理", 404);
        return new Response(object.body, { status:200, headers });
    }
    const bytes = base64ToBytes(String(row.inline_data || ""));
    return new Response(bytes, { status:200, headers });
}

async function togetherVoiceRoute(request, env, user, code) {
    const room = await togetherRoomAccess(env, user, code);
    if (!room) return fail("room_not_found", "房间不存在、已结束或你不在房间中", 404);
    const now = Date.now();
    await env.DB.prepare(`DELETE FROM together_voice_chunks WHERE room_code=? AND created_at<?`).bind(code, now - 10000).run();
    if (request.method === "POST") {
        const buffer = await request.arrayBuffer();
        if (!buffer.byteLength || buffer.byteLength > 32768)
            return fail("voice_chunk_invalid", "语音数据块无效", 400);
        const payload = bytesToBase64(new Uint8Array(buffer));
        await env.DB.prepare(`INSERT INTO together_voice_chunks(room_code,user_id,payload,created_at) VALUES (?,?,?,?)`)
            .bind(code,user.id,payload,now).run();
        return ok({ accepted:true });
    }
    const after = Math.max(0, Number(new URL(request.url).searchParams.get("after") || 0));
    const maxRow = await env.DB.prepare(`SELECT COALESCE(MAX(seq),0) AS n FROM together_voice_chunks WHERE room_code=?`).bind(code).first();
    const cursor = Number(maxRow?.n || 0);
    if (after <= 0) return ok({ chunks:[], cursor });
    const rows = await env.DB.prepare(`SELECT v.seq,v.user_id,v.payload,v.created_at,u.username,u.display_name
        FROM together_voice_chunks v JOIN users u ON u.id=v.user_id
        WHERE v.room_code=? AND v.seq>? AND v.user_id<>? AND v.created_at>=?
        ORDER BY v.seq ASC LIMIT 48`).bind(code,after,user.id,now-8000).all();
    return ok({ chunks:(rows?.results||[]).map((r)=>({
        seq:Number(r.seq||0), senderId:String(r.user_id||""), senderName:String(r.display_name||r.username||"用户"),
        payload:String(r.payload||""), createdAt:Number(r.created_at||0)
    })), cursor });
}

async function roomPayload(env, user, code) {
    const now = Date.now();
    const room = await env.DB.prepare(`SELECT * FROM together_rooms WHERE code = ?`).bind(code).first();
    if (!room) return null;
    if (Number(room.expires_at || 0) <= now) {
        await destroyTogetherRoom(env, code);
        return null;
    }
    const member = await env.DB.prepare(`SELECT 1 AS ok FROM together_members WHERE room_code = ? AND user_id = ?`).bind(code, user.id).first();
    if (!member && room.host_user_id !== user.id) return null;

    // A host that is still polling the room refreshes first. Guests use the
    // host heartbeat to detect an abandoned/disconnected room quickly.
    if (room.host_user_id === user.id)
        await env.DB.prepare(`UPDATE together_members SET last_seen_at = ? WHERE room_code = ? AND user_id = ?`).bind(now, code, user.id).run();
    const hostHeartbeat = await env.DB.prepare(`SELECT last_seen_at FROM together_members WHERE room_code = ? AND user_id = ?`).bind(code, room.host_user_id).first();
    if (room.host_user_id !== user.id && (!hostHeartbeat || Number(hostHeartbeat.last_seen_at || 0) < now - 6500)) {
        await destroyTogetherRoom(env, code);
        return null;
    }
    if (room.host_user_id !== user.id)
        await env.DB.prepare(`UPDATE together_members SET last_seen_at = ? WHERE room_code = ? AND user_id = ?`).bind(now, code, user.id).run();
    await env.DB.prepare(`DELETE FROM together_members WHERE room_code = ? AND user_id <> ? AND last_seen_at < ?`).bind(code, room.host_user_id, now - 90000).run();
    const rows = await env.DB.prepare(`SELECT m.user_id, u.username, u.display_name, u.avatar_data
        FROM together_members m JOIN users u ON u.id = m.user_id
        WHERE m.room_code = ? ORDER BY CASE WHEN m.user_id = ? THEN 0 ELSE 1 END, m.joined_at ASC`).bind(code, room.host_user_id).all();
    return {
        code,
        isHost: room.host_user_id === user.id,
        hostUserId: String(room.host_user_id || ""),
        state: safeJsonObject(room.state_json, {}),
        members: (rows?.results || []).map((r) => ({
            id:String(r.user_id||""), username:String(r.username||""), displayName:String(r.display_name||r.username||"Evolve 用户"),
            avatar:String(r.avatar_data||""), host:String(r.user_id||"")===String(room.host_user_id||"")
        })),
        updatedAt: Number(room.updated_at || 0),
        expiresAt: Number(room.expires_at || 0)
    };
}

async function createTogetherRoomRoute(env, user) {
    const hosted = await env.DB.prepare(`SELECT code FROM together_rooms WHERE host_user_id = ?`).bind(user.id).all();
    for (const row of (hosted?.results || []))
        await destroyTogetherRoom(env, String(row.code || ""));
    await env.DB.prepare(`DELETE FROM together_members WHERE user_id = ?`).bind(user.id).run();
    let code = roomCode();
    for (let i=0;i<5;i++) {
        if (!(await env.DB.prepare(`SELECT code FROM together_rooms WHERE code = ?`).bind(code).first())) break;
        code = roomCode();
    }
    const now = Date.now(), expires = now + 12*60*60*1000;
    await env.DB.batch([
        env.DB.prepare(`INSERT INTO together_rooms(code,host_user_id,state_json,created_at,updated_at,expires_at) VALUES (?,?,'{}',?,?,?)`).bind(code,user.id,now,now,expires),
        env.DB.prepare(`INSERT OR REPLACE INTO together_members(room_code,user_id,joined_at,last_seen_at) VALUES (?,?,?,?)`).bind(code,user.id,now,now)
    ]);
    return ok({ room: await roomPayload(env, user, code) });
}

async function joinTogetherRoomRoute(request, env, user) {
    const body = await parseJsonBody(request);
    const code = String(body?.code || "").trim().toUpperCase();
    const room = await env.DB.prepare(`SELECT code FROM together_rooms WHERE code = ? AND expires_at > ?`).bind(code, Date.now()).first();
    if (!room) return fail("room_not_found", "房间不存在或已过期", 404);
    const now=Date.now();
    await env.DB.batch([
        env.DB.prepare(`INSERT OR REPLACE INTO together_members(room_code,user_id,joined_at,last_seen_at) VALUES (?,?,?,?)`).bind(code,user.id,now,now),
        env.DB.prepare(`DELETE FROM together_invites WHERE room_code = ? AND to_user_id = ?`).bind(code,user.id)
    ]);
    const payload = await roomPayload(env, user, code);
    return payload ? ok({ room: payload }) : fail("room_not_found", "房间已结束", 404);
}

async function getTogetherRoomRoute(env, user, code) {
    const room = await roomPayload(env, user, code);
    return room ? ok({ room }) : fail("room_not_found", "房间不存在、已过期或你不在房间中", 404);
}

async function updateTogetherStateRoute(request, env, user, code) {
    const room = await env.DB.prepare(`SELECT * FROM together_rooms WHERE code = ?`).bind(code).first();
    if (!room) return fail("room_not_found", "房间不存在", 404);
    if (room.host_user_id !== user.id) return fail("host_required", "只有房主可以同步播放状态", 403);
    const body=await parseJsonBody(request); const state=body?.state && typeof body.state==="object" ? body.state : {};
    const encoded=JSON.stringify(state).slice(0,180000); const now=Date.now();
    await env.DB.prepare(`UPDATE together_rooms SET state_json = ?, updated_at = ?, expires_at = ? WHERE code = ?`).bind(encoded,now,now+12*60*60*1000,code).run();
    return ok({ updated:true });
}

async function leaveTogetherRoomRoute(env, user, code) {
    const room = await env.DB.prepare(`SELECT host_user_id FROM together_rooms WHERE code = ?`).bind(code).first();
    if (!room) return ok({ left:true });
    if (room.host_user_id === user.id) {
        await destroyTogetherRoom(env, code);
    } else await env.DB.prepare(`DELETE FROM together_members WHERE room_code = ? AND user_id = ?`).bind(code,user.id).run();
    return ok({ left:true });
}

async function inviteTogetherUserRoute(request, env, user, code) {
    const room = await env.DB.prepare(`SELECT host_user_id FROM together_rooms WHERE code = ? AND expires_at > ?`).bind(code,Date.now()).first();
    if (!room || room.host_user_id !== user.id) return fail("host_required", "只有房主可以邀请好友", 403);
    const body=await parseJsonBody(request); const key=usernameKey(body?.username || "");
    const target=await env.DB.prepare(`SELECT id FROM users WHERE username_key = ?`).bind(key).first();
    if (!target) return fail("user_not_found", "没有找到这个用户", 404);
    const now=Date.now();
    await env.DB.batch([
        env.DB.prepare(`DELETE FROM together_invites WHERE room_code = ? AND to_user_id = ?`).bind(code,target.id),
        env.DB.prepare(`INSERT INTO together_invites(id,room_code,from_user_id,to_user_id,created_at,expires_at) VALUES (?,?,?,?,?,?)`)
            .bind(`inv_${crypto.randomUUID()}`,code,user.id,target.id,now,now+2*60*60*1000)
    ]);
    return ok({ invited:true });
}

async function dismissTogetherInviteRoute(env, user, inviteId) {
    await env.DB.prepare(`DELETE FROM together_invites WHERE id = ? AND to_user_id = ?`).bind(inviteId, user.id).run();
    return ok({ dismissed: true });
}

async function listTogetherInvitesRoute(env, user) {
    await env.DB.prepare(`DELETE FROM together_invites WHERE expires_at <= ?`).bind(Date.now()).run();
    await env.DB.prepare(`DELETE FROM together_invites WHERE room_code NOT IN (SELECT code FROM together_rooms)`).run();
    const rows=await env.DB.prepare(`SELECT i.id, i.room_code, i.created_at, u.username AS from_username, u.display_name AS from_name
        FROM together_invites i JOIN users u ON u.id=i.from_user_id WHERE i.to_user_id=? ORDER BY i.created_at DESC LIMIT 20`).bind(user.id).all();
    return ok({ invites:(rows?.results||[]).map(r=>({ id:String(r.id||""), code:String(r.room_code||""), fromUsername:String(r.from_username||""), fromName:String(r.from_name||r.from_username||"好友"), createdAt:Number(r.created_at||0) })) });
}

async function presenceRoute(env, user) {
    const now = Date.now();
    await env.DB.prepare(`UPDATE users SET last_seen_at = ? WHERE id = ?`).bind(now, user.id).run();
    await cleanupStaleTogetherRooms(env, now);
    const row = await env.DB.prepare(`SELECT COUNT(*) AS n FROM users WHERE last_seen_at >= ?`).bind(now - 60000).first();
    return ok({ onlineUsers: Number(row?.n || 0), windowSeconds: 60, at: now });
}

function updateManifest(env) {
    const version = String(env.EVOLVE_LATEST_VERSION || "0.17.0").trim();
    const url = String(env.EVOLVE_SETUP_URL || "").trim();
    const sha256 = String(env.EVOLVE_SETUP_SHA256 || "").trim().toLowerCase();
    const notes = String(env.EVOLVE_RELEASE_NOTES || "v0.17.0：一起听房间聊天、图片/文件临时分享与实时语音，房间结束自动清理。").trim();
    return json({
        ok: true,
        version,
        url,
        sha256,
        notes,
        mandatory: String(env.EVOLVE_UPDATE_MANDATORY || "0") === "1",
        available: Boolean(url),
        publishedAt: String(env.EVOLVE_RELEASE_PUBLISHED_AT || "")
    }, 200, { "Cache-Control": "public, max-age=120" });
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

    const missingIds = [...new Set(
        songs
            .filter((song) => !neteaseSongCover(song))
            .map((song) => String(song?.id ?? ""))
            .filter(Boolean)
    )].slice(0, 50);

    if (missingIds.length === 0)
        return songs;

    const detailsById = new Map();
    const addDetails = (rows) => {
        for (const detail of (Array.isArray(rows) ? rows : [])) {
            const id = String(detail?.id ?? "");
            if (!id)
                continue;
            const old = detailsById.get(id);
            if (!old || (!neteaseSongCover(old) && neteaseSongCover(detail)))
                detailsById.set(id, detail);
        }
    };
    const unresolvedIds = () => missingIds.filter((id) => {
        const row = detailsById.get(id);
        return !row || !neteaseSongCover(row);
    });

    // v0.12.3: bulk artwork enrichment is latency-sensitive. The previous
    // version waited for gateway -> public detail #1 -> public detail #2 in
    // sequence. Race all bulk metadata paths so one slow edge no longer holds
    // the entire search list hostage.
    const bulkTasks = [];
    if (env.NETEASE_API_ORIGIN) {
        bulkTasks.push((async () => {
            try {
                const gateway = await gatewayJson(
                    env,
                    "/song/detail",
                    { ids: missingIds.join(",") }
                );
                return gateway?.songs || [];
            } catch {
                return [];
            }
        })());
    }

    for (const detailPath of [
        "https://music.163.com/api/song/detail",
        "https://music.163.com/api/song/detail/"
    ]) {
        bulkTasks.push((async () => {
            try {
                const url = new URL(detailPath);
                url.searchParams.set("ids", JSON.stringify(missingIds));
                const response = await fetch(url, {
                    headers: neteaseSearchHeaders(),
                    redirect: "follow"
                });
                if (!response.ok)
                    return [];
                const body = await response.json().catch(() => null);
                return Array.isArray(body?.songs) ? body.songs : [];
            } catch {
                return [];
            }
        })());
    }

    const bulkRows = await Promise.all(
        bulkTasks.map((task) => Promise.race([
            task,
            sleepMs(850).then(() => [])
        ]))
    );
    for (const rows of bulkRows)
        addDetails(rows);

    // Rescue only the still-missing visible rows, and do those requests in
    // parallel too. This keeps first-screen artwork responsive even when the
    // batched endpoint returns a partial result.
    const rescueIds = unresolvedIds().slice(0, 28);
    if (rescueIds.length > 0) {
        const rescued = await Promise.all(
            rescueIds.map((id) => Promise.race([
                (async () => {
                    try {
                        const url = new URL("https://music.163.com/api/song/detail/");
                        url.searchParams.set("id", id);
                        url.searchParams.set("ids", `[${id}]`);
                        const response = await fetch(url, {
                            headers: neteaseSearchHeaders(),
                            redirect: "follow"
                        });
                        if (!response.ok)
                            return null;
                        const body = await response.json().catch(() => null);
                        return Array.isArray(body?.songs)
                            ? body.songs[0] || null
                            : null;
                    } catch {
                        return null;
                    }
                })(),
                sleepMs(650).then(() => null)
            ]))
        );
        addDetails(rescued);
    }

    if (detailsById.size === 0)
        return songs;

    return songs.map((song) =>
        mergeNeteaseSongMetadata(
            song,
            detailsById.get(String(song?.id ?? ""))
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

function neteaseQualityFallbacks(quality) {
    const value = String(quality || "exhigh").toLowerCase();
    if (value === "standard") return ["standard"];
    if (value === "higher") return ["higher", "standard"];
    if (value === "exhigh") return ["exhigh", "higher", "standard"];
    return [value || "lossless", "exhigh", "higher", "standard"]
        .filter((item, index, rows) => item && rows.indexOf(item) === index);
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

const NETEASE_EAPI_KEY = "e82ckenh8dichen8";
const NETEASE_EAPI_SALT = "-36cd479b6b5-";

function rotateLeft32(value, shift) {
    return ((value << shift) | (value >>> (32 - shift))) >>> 0;
}

function md5Hex(text) {
    const input = encoder.encode(text);
    const bitLength = BigInt(input.length) * 8n;
    let paddedLength = input.length + 1;
    while (paddedLength % 64 !== 56)
        paddedLength++;
    const bytes = new Uint8Array(paddedLength + 8);
    bytes.set(input);
    bytes[input.length] = 0x80;
    for (let i = 0; i < 8; i++)
        bytes[paddedLength + i] = Number((bitLength >> BigInt(i * 8)) & 0xffn);

    const shifts = [
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    ];
    const constants = Array.from({ length: 64 }, (_, i) =>
        Math.floor(Math.abs(Math.sin(i + 1)) * 0x100000000) >>> 0);

    let a0 = 0x67452301;
    let b0 = 0xefcdab89;
    let c0 = 0x98badcfe;
    let d0 = 0x10325476;

    for (let offset = 0; offset < bytes.length; offset += 64) {
        const words = new Uint32Array(16);
        for (let i = 0; i < 16; i++) {
            const j = offset + i * 4;
            words[i] = (
                bytes[j]
                | (bytes[j + 1] << 8)
                | (bytes[j + 2] << 16)
                | (bytes[j + 3] << 24)
            ) >>> 0;
        }

        let a = a0, b = b0, c = c0, d = d0;
        for (let i = 0; i < 64; i++) {
            let f, g;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }
            const sum = (a + (f >>> 0) + constants[i] + words[g]) >>> 0;
            const nextD = d;
            d = c;
            c = b;
            b = (b + rotateLeft32(sum, shifts[i])) >>> 0;
            a = nextD;
        }
        a0 = (a0 + a) >>> 0;
        b0 = (b0 + b) >>> 0;
        c0 = (c0 + c) >>> 0;
        d0 = (d0 + d) >>> 0;
    }

    const words = [a0, b0, c0, d0];
    let out = "";
    for (const word of words) {
        for (let i = 0; i < 4; i++)
            out += ((word >>> (i * 8)) & 0xff).toString(16).padStart(2, "0");
    }
    return out;
}

function pkcs7Pad(bytes, blockSize = 16) {
    const pad = blockSize - (bytes.length % blockSize || blockSize);
    const actualPad = bytes.length % blockSize === 0 ? blockSize : pad;
    const out = new Uint8Array(bytes.length + actualPad);
    out.set(bytes);
    out.fill(actualPad, bytes.length);
    return out;
}

async function aesEcbHex(text, keyText) {
    // WebCrypto intentionally omits AES-ECB. Encrypt each already padded block
    // as the first block of AES-CBC with an all-zero IV; the first CBC block is
    // mathematically identical to ECB. Ignore WebCrypto's extra padding block.
    const key = await crypto.subtle.importKey(
        "raw",
        encoder.encode(keyText),
        { name: "AES-CBC" },
        false,
        ["encrypt"]
    );
    const padded = pkcs7Pad(encoder.encode(text));
    const output = new Uint8Array(padded.length);
    const zeroIv = new Uint8Array(16);
    for (let offset = 0; offset < padded.length; offset += 16) {
        const encrypted = new Uint8Array(await crypto.subtle.encrypt(
            { name: "AES-CBC", iv: zeroIv },
            key,
            padded.slice(offset, offset + 16)
        ));
        output.set(encrypted.slice(0, 16), offset);
    }
    return [...output]
        .map((byte) => byte.toString(16).padStart(2, "0"))
        .join("")
        .toUpperCase();
}

function parseCookieMap(cookie = "") {
    const out = {};
    for (const part of String(cookie || "").split(";")) {
        const index = part.indexOf("=");
        if (index <= 0) continue;
        const key = part.slice(0, index).trim();
        const value = part.slice(index + 1).trim();
        if (key) out[key] = value;
    }
    return out;
}

function randomHex(bytes = 16) {
    const data = new Uint8Array(bytes);
    crypto.getRandomValues(data);
    return [...data].map((v) => v.toString(16).padStart(2, "0")).join("");
}

function createNeteaseEapiHeader(cookie = "") {
    const own = parseCookieMap(cookie);
    const header = {
        osver: own.osver || "Microsoft-Windows-10-Professional-build-19045-64bit",
        deviceId: own.deviceId || randomHex(16),
        os: own.os || "pc",
        appver: own.appver || "3.1.17.204416",
        versioncode: own.versioncode || "140",
        mobilename: own.mobilename || "",
        buildver: own.buildver || String(Date.now()).slice(0, 10),
        resolution: own.resolution || "1920x1080",
        __csrf: own.__csrf || "",
        channel: own.channel || "netease",
        requestId: `${Date.now()}_${String(Math.floor(Math.random() * 10000)).padStart(4, "0")}`
    };
    if (own.MUSIC_U) header.MUSIC_U = own.MUSIC_U;
    if (own.MUSIC_A) header.MUSIC_A = own.MUSIC_A;
    return header;
}

function eapiHeaderCookie(header) {
    return Object.entries(header)
        .map(([key, value]) => `${encodeURIComponent(key)}=${encodeURIComponent(String(value))}`)
        .join("; ");
}

async function buildNeteaseEapiPayload(path, data) {
    const text = JSON.stringify(data);
    const digest = md5Hex(`nobody${path}use${text}md5forencrypt`);
    const payload = `${path}${NETEASE_EAPI_SALT}${text}${NETEASE_EAPI_SALT}${digest}`;
    return aesEcbHex(payload, NETEASE_EAPI_KEY);
}

const NETEASE_WEAPI_IV = "0102030405060708";
const NETEASE_WEAPI_NONCE = "0CoJUm6Qyw8W8jud";
const NETEASE_WEAPI_PUBKEY = 0x10001n;
const NETEASE_WEAPI_MODULUS = BigInt(
    "0x00e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b725152b3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312ecbda92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d813cfe4875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7"
);
const NETEASE_WEAPI_ALPHABET =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

function randomNeteaseSecret(length = 16) {
    const random = new Uint8Array(length);
    crypto.getRandomValues(random);
    let out = "";
    for (const byte of random)
        out += NETEASE_WEAPI_ALPHABET[byte % NETEASE_WEAPI_ALPHABET.length];
    return out;
}

async function neteaseAesCbcBase64(text, keyText) {
    const key = await crypto.subtle.importKey(
        "raw",
        encoder.encode(keyText),
        { name: "AES-CBC" },
        false,
        ["encrypt"]
    );
    const encrypted = await crypto.subtle.encrypt(
        {
            name: "AES-CBC",
            iv: encoder.encode(NETEASE_WEAPI_IV)
        },
        key,
        encoder.encode(text)
    );
    return bytesToBase64(new Uint8Array(encrypted));
}

function modPow(base, exponent, modulus) {
    let result = 1n;
    let value = base % modulus;
    let power = exponent;
    while (power > 0n) {
        if (power & 1n)
            result = (result * value) % modulus;
        power >>= 1n;
        value = (value * value) % modulus;
    }
    return result;
}

function neteaseRsaNoPadding(secret) {
    const reversed = [...secret].reverse().join("");
    const hex = [...encoder.encode(reversed)]
        .map((byte) => byte.toString(16).padStart(2, "0"))
        .join("");
    const message = BigInt(`0x${hex || "0"}`);
    return modPow(
        message,
        NETEASE_WEAPI_PUBKEY,
        NETEASE_WEAPI_MODULUS
    ).toString(16).padStart(256, "0");
}

async function buildNeteaseWeapiPayload(data) {
    const secret = randomNeteaseSecret(16);
    const first = await neteaseAesCbcBase64(
        JSON.stringify(data),
        NETEASE_WEAPI_NONCE
    );
    const params = await neteaseAesCbcBase64(first, secret);
    return {
        params,
        encSecKey: neteaseRsaNoPadding(secret)
    };
}

function neteaseCsrfToken(cookie = "") {
    const match = String(cookie || "")
        .match(/(?:^|;\s*)__csrf=([^;]*)/);
    return match ? decodeURIComponent(match[1]) : "";
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

async function runNeteasePlayerUrlV1Attempt(
    host,
    sourceId,
    level,
    cookie = ""
) {
    const apiPath = "/api/song/enhance/player/url/v1";
    const header = createNeteaseEapiHeader(cookie);
    const data = {
        ids: `[${String(sourceId)}]`,
        level: String(level || "exhigh"),
        encodeType: "flac",
        e_r: false,
        header
    };
    if (data.level === "sky")
        data.immerseType = "c51";

    const params = await buildNeteaseEapiPayload(apiPath, data);
    const form = new URLSearchParams();
    form.set("params", params);

    const response = await fetch(
        `${host}/eapi/song/enhance/player/url/v1`,
        {
            method: "POST",
            headers: {
                "User-Agent":
                    "Mozilla/5.0 (Windows NT 10.0; WOW64) "
                    + "AppleWebKit/537.36 (KHTML, like Gecko) "
                    + "Chrome/91.0.4472.164 NeteaseMusicDesktop/3.1.29.205117",
                "Accept": "application/json,text/plain,*/*",
                "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
                "Cookie": eapiHeaderCookie(header)
            },
            body: form.toString(),
            redirect: "follow"
        }
    );

    const contentType = response.headers.get("content-type") || "";
    const raw = await response.text();
    let body = null;
    try {
        body = JSON.parse(raw);
    } catch {
        body = null;
    }

    const row = Array.isArray(body?.data) ? body.data[0] : null;
    const audioUrl = typeof row?.url === "string"
        ? normalizeNeteaseCdnUrl(row.url)
        : "";

    return {
        kind: `eapi-v1:${host}`,
        ok: response.ok,
        status: response.status,
        contentType,
        body,
        row,
        audioUrl,
        raw
    };
}

async function runNeteasePlayerUrlV1WeapiAttempt(
    sourceId,
    level,
    cookie = ""
) {
    const csrfToken = neteaseCsrfToken(cookie);
    const data = {
        ids: `[${String(sourceId)}]`,
        level: String(level || "exhigh"),
        encodeType: "flac",
        csrf_token: csrfToken
    };
    if (data.level === "sky")
        data.immerseType = "c51";

    const payload = await buildNeteaseWeapiPayload(data);
    const form = new URLSearchParams();
    form.set("params", payload.params);
    form.set("encSecKey", payload.encSecKey);

    const endpoint = new URL(
        "https://music.163.com/weapi/song/enhance/player/url/v1"
    );
    endpoint.searchParams.set("csrf_token", csrfToken);

    const response = await fetch(endpoint, {
        method: "POST",
        headers: {
            ...neteasePlayerHeaders(cookie),
            "Content-Type":
                "application/x-www-form-urlencoded; charset=UTF-8"
        },
        body: form.toString(),
        redirect: "follow"
    });

    const contentType = response.headers.get("content-type") || "";
    const raw = await response.text();
    let body = null;
    try {
        body = JSON.parse(raw);
    } catch {
        body = null;
    }
    const row = Array.isArray(body?.data) ? body.data[0] : null;
    const audioUrl = typeof row?.url === "string"
        ? normalizeNeteaseCdnUrl(row.url)
        : "";
    return {
        kind: "weapi-v1",
        ok: response.ok,
        status: response.status,
        contentType,
        body,
        row,
        audioUrl,
        raw
    };
}

async function runNeteasePlayerUrlAttempt(
    kind,
    sourceId,
    bitrate,
    cookie = ""
) {
    const headers =
        neteasePlayerHeaders(cookie);

    const rawBase =
        "https://music.163.com"
        + "/api/song/enhance/player/url";

    let response;

    if (kind === "eapi") {
        const apiPath = "/api/song/enhance/player/url";
        const header = createNeteaseEapiHeader(cookie);
        const data = {
            ids: JSON.stringify([String(sourceId)]),
            br: Number(bitrate),
            header
        };
        const params = await buildNeteaseEapiPayload(apiPath, data);
        const form = new URLSearchParams();
        form.set("params", params);
        response = await fetch(
            "https://interfacepc.music.163.com/eapi/song/enhance/player/url",
            {
                method: "POST",
                headers: {
                    "User-Agent":
                        "Mozilla/5.0 (Windows NT 10.0; WOW64) "
                        + "AppleWebKit/537.36 (KHTML, like Gecko) "
                        + "Chrome/91.0.4472.164 NeteaseMusicDesktop/3.1.29.205117",
                    "Accept": "application/json,text/plain,*/*",
                    "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
                    "Cookie": eapiHeaderCookie(header)
                },
                body: form.toString(),
                redirect: "follow"
            }
        );
    } else if (kind === "weapi") {
        const csrfToken = neteaseCsrfToken(cookie);
        const payload = await buildNeteaseWeapiPayload({
            ids: [String(sourceId)],
            br: Number(bitrate),
            csrf_token: csrfToken
        });
        const form = new URLSearchParams();
        form.set("params", payload.params);
        form.set("encSecKey", payload.encSecKey);

        const url = new URL(
            "https://music.163.com/weapi/song/enhance/player/url"
        );
        url.searchParams.set("csrf_token", csrfToken);

        response = await fetch(url, {
            method: "POST",
            headers: {
                ...headers,
                "Content-Type":
                    "application/x-www-form-urlencoded; charset=UTF-8"
            },
            body: form.toString(),
            redirect: "follow"
        });
    } else if (kind === "post") {
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
            neteaseCsrfToken(cookie)
        );

        response = await fetch(
            rawBase,
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
            new URL(rawBase);

        url.searchParams.set(
            "csrf_token",
            neteaseCsrfToken(cookie)
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
    const session = await getUserProviderSession(
        env,
        userId,
        "netease"
    );

    // Only this user's own stored provider session is used.
    const cookie = String(session?.cookie || "");
    const qualityLevels = neteaseQualityFallbacks(quality);
    const tasks = [];
    let raceResolved = false;

    const schedule = (delayMs, resolver) => (async () => {
        if (delayMs > 0)
            await sleepMs(delayMs);
        if (raceResolved)
            return null;
        try {
            return await resolver();
        } catch {
            return null;
        }
    })();

    const normalizeResult = (result, resolver, level) => {
        const audioUrl = normalizeNeteaseCdnUrl(result?.audioUrl || result?.url || "");
        if (!audioUrl)
            return null;
        return {
            url: audioUrl,
            access: cookie ? "account" : "free",
            resolver,
            resolvedQuality: level,
            providerRow: result?.row || result?.providerRow || null
        };
    };

    // v0.12.3: the old resolver waited route-by-route and quality-by-quality.
    // One unhealthy NetEase edge could therefore add several seconds before
    // the next route was even attempted. Launch the preferred quality routes
    // together, then stagger lower-quality fallbacks by ~110 ms. Whichever
    // legitimate route returns a playable URL first wins.
    qualityLevels.forEach((level, levelIndex) => {
        const delay = levelIndex === 0 ? 0 : 110 * levelIndex;

        if (env.NETEASE_API_ORIGIN) {
            for (const gatewayPath of ["/song/url/v1", "/song/url"]) {
                tasks.push(schedule(delay, async () => {
                    const query = gatewayPath === "/song/url/v1"
                        ? { id: sourceId, level }
                        : { id: sourceId, br: qualityToBitrate(level) };
                    const body = await gatewayJson(env, gatewayPath, query, cookie);
                    const row = Array.isArray(body?.data) ? body.data[0] : null;
                    return normalizeResult(
                        { url: row?.url || "", row },
                        `${gatewayPath === "/song/url" ? "gateway" : "gateway-v1"}-${level}`,
                        level
                    );
                }));
            }
        }

        for (const host of [
            "https://interface3.music.163.com",
            "https://interfacepc.music.163.com"
        ]) {
            tasks.push(schedule(delay, async () => {
                const result = await runNeteasePlayerUrlV1Attempt(
                    host,
                    sourceId,
                    level,
                    cookie
                );
                return normalizeResult(
                    result,
                    `direct-player-url-v1-${level}`,
                    level
                );
            }));
        }

        tasks.push(schedule(delay, async () => {
            const result = await runNeteasePlayerUrlV1WeapiAttempt(
                sourceId,
                level,
                cookie
            );
            return normalizeResult(
                result,
                `direct-player-url-weapi-v1-${level}`,
                level
            );
        }));

        // Keep the full legacy matrix for the requested quality. Lower tiers
        // only need the two historically most useful compatibility routes,
        // which keeps Worker subrequest count bounded.
        const legacyKinds = levelIndex === 0
            ? ["eapi", "weapi", "post", "get"]
            : ["post", "get"];
        const bitrate = qualityToBitrate(level);
        for (const kind of legacyKinds) {
            tasks.push(schedule(delay + 35, async () => {
                const result = await runNeteasePlayerUrlAttempt(
                    kind,
                    sourceId,
                    bitrate,
                    cookie
                );
                return normalizeResult(
                    result,
                    `direct-player-url-${kind}-${level}`,
                    level
                );
            }));
        }
    });

    // Official outer-url is cheap and useful, but give the player/url family a
    // very short head start so higher-quality official URLs still usually win.
    tasks.push(schedule(260, async () => {
        const outerUrl = await resolveNeteaseOuterUrl(sourceId);
        return outerUrl
            ? {
                url: outerUrl,
                access: cookie ? "account" : "free",
                resolver: "outer-url",
                resolvedQuality: "standard"
            }
            : null;
    }));

    const successful = tasks.map((task) => task.then((value) => {
        if (value?.url) {
            raceResolved = true;
            return value;
        }
        throw new Error("route unavailable");
    }));

    try {
        return await Promise.race([
            Promise.any(successful),
            sleepMs(3600).then(() => {
                raceResolved = true;
                return null;
            })
        ]);
    } catch {
        return null;
    }
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
        "eapi",
        "weapi",
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

function artworkCandidateUrls(inputUrl, size) {
    const out = [];
    const seen = new Set();

    const add = (candidate) => {
        const key = candidate.toString();
        if (!seen.has(key)) {
            seen.add(key);
            out.push(candidate);
        }
    };

    const base = new URL(inputUrl.toString());
    if (base.hostname.endsWith("126.net")) {
        base.searchParams.set(
            "param",
            `${Math.round(size)}y${Math.round(size)}`
        );
    }
    add(base);

    // NetEase artwork is mirrored across p1/p2/p3/p4. An individual edge can
    // fail by region even though the same object is healthy on another mirror.
    const mirrorMatch =
        base.hostname.match(/^p\d+\.music\.126\.net$/i);
    if (base.hostname.toLowerCase() === "music.126.net" || mirrorMatch) {
        for (let i = 1; i <= 4; i++) {
            const mirror = new URL(base.toString());
            mirror.hostname = `p${i}.music.126.net`;
            add(mirror);
        }
    }

    // Some older image objects reject the resize query while the original
    // object itself remains valid. Retry without param before giving up.
    if (base.hostname.endsWith("126.net")
        && base.searchParams.has("param")) {
        const originalSize = new URL(base.toString());
        originalSize.searchParams.delete("param");
        add(originalSize);
    }

    return out;
}

async function handleArtwork(request) {
    const requestUrl = new URL(request.url);
    const raw = String(requestUrl.searchParams.get("url") || "").trim();
    const size = Math.max(
        48,
        Math.min(1200, Number(requestUrl.searchParams.get("size") || 320))
    );

    let upstreamUrl;
    try {
        upstreamUrl = new URL(raw);
    } catch {
        return fail("invalid_artwork", "封面地址无效", 400);
    }

    if (!["http:", "https:"].includes(upstreamUrl.protocol)
        || !allowedArtworkHost(upstreamUrl.hostname)) {
        return fail("invalid_artwork_host", "不允许代理这个封面来源", 403);
    }

    const cache = caches.default;
    const cacheKey = new Request(requestUrl.toString(), { method: "GET" });
    if (request.method === "GET") {
        const cached = await cache.match(cacheKey);
        if (cached)
            return cached;
    }

    const candidates = artworkCandidateUrls(upstreamUrl, size);
    const attempts = candidates.map(async (candidate) => {
        try {
            const response = await fetch(candidate, {
                method: "GET",
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

            const contentType = response.headers.get("content-type") || "";
            if (!response.ok || !contentType.toLowerCase().startsWith("image/")) {
                try { await response.body?.cancel(); } catch {}
                throw new Error(`HTTP ${response.status}`);
            }

            const bytes = await response.arrayBuffer();
            if (!bytes || bytes.byteLength < 128)
                throw new Error("empty image");

            return {
                bytes,
                contentType,
                etag: response.headers.get("etag") || ""
            };
        } catch (error) {
            throw new Error(String(error?.message || error || "封面请求失败"));
        }
    });

    let winner = null;
    try {
        // v0.12.3: race p1/p2/p3/p4 and the original-size URL instead of
        // waiting for each edge serially. A single regional CDN stall can no
        // longer block every album image behind it.
        winner = await Promise.race([
            Promise.any(attempts),
            sleepMs(3400).then(() => {
                throw new Error("封面镜像响应超时");
            })
        ]);
    } catch (error) {
        return fail(
            "artwork_upstream_failed",
            String(error?.message || error || "封面上游请求失败"),
            502
        );
    }

    const headers = new Headers();
    headers.set("Content-Type", winner.contentType || "image/jpeg");
    headers.set("Cache-Control", "public, max-age=86400, stale-while-revalidate=604800");
    headers.set("Access-Control-Allow-Origin", "*");
    if (winner.etag)
        headers.set("ETag", winner.etag);
    if (request.method !== "HEAD")
        headers.set("Content-Length", String(winner.bytes.byteLength));

    const output = new Response(
        request.method === "HEAD" ? null : winner.bytes,
        { status: 200, headers }
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

function roamArtistTokens(value) {
    return String(value || "")
        .split(/[\/&,，、;；]+/)
        .map((item) => item.trim())
        .filter(Boolean)
        .slice(0, 6);
}

function trackIdentityCandidates(track) {
    const values = [
        track?.sourceId,
        track?.id
    ]
        .map((value) => String(value || "").trim())
        .filter(Boolean);
    const expanded = [];
    for (const value of values) {
        expanded.push(value);
        if (value.includes(":"))
            expanded.push(value.split(":").slice(1).join(":"));
    }
    return expanded;
}

function discoveryVersionScore(track, explicitQuery = "") {
    const title = String(track?.title || "").toLowerCase();
    const q = String(explicitQuery || "").toLowerCase();
    const dj = /(^|[\s\-_(【\[])(dj|remix|mix|电音|抖音版|舞曲)/i.test(title);
    const cover = /(翻唱|cover|网友|女声版|男声版|童声|改编)/i.test(title);
    const accompaniment = /(伴奏|纯音乐|instrumental|ktv|钢琴版)/i.test(title);
    const live = /(live|现场|演唱会)/i.test(title);
    const wantsDj = /(dj|remix|电音|舞曲)/i.test(q);
    const wantsCover = /(翻唱|cover|改编)/i.test(q);
    const wantsAccompaniment = /(伴奏|纯音乐|instrumental|ktv)/i.test(q);
    const wantsLive = /(live|现场|演唱会)/i.test(q);
    if (wantsDj) return dj ? 1200 : 0;
    if (wantsCover) return cover ? 1200 : 0;
    if (wantsAccompaniment) return accompaniment ? 1200 : 0;
    if (wantsLive) return live ? 1200 : 0;
    if (accompaniment) return -750;
    if (cover) return 120;
    if (live) return 260;
    if (dj) return 430;
    return 900;
}

async function handleRoam(request, env, user) {
    const url = new URL(request.url);
    const limit = Math.min(
        24,
        Math.max(6, Number(url.searchParams.get("limit") || 18))
    );

    const exclude = new Set(
        String(url.searchParams.get("exclude") || "")
            .split(",")
            .map((item) => item.trim())
            .filter(Boolean)
    );
    const currentArtist = String(url.searchParams.get("artist") || "").trim();
    const currentTitle = String(url.searchParams.get("title") || "").trim();
    const currentId = String(url.searchParams.get("currentId") || "").trim();
    if (currentId) {
        exclude.add(currentId);
        if (currentId.includes(":"))
            exclude.add(currentId.split(":").slice(1).join(":"));
    }

    const stateRow = await env.DB
        .prepare(`SELECT favorites_json, history_json, settings_json
           FROM user_state
          WHERE user_id = ?`)
        .bind(user.id)
        .first()
        .catch(() => null);

    const favorites = safeArray(
        (() => {
            try { return JSON.parse(stateRow?.favorites_json || "[]"); }
            catch { return []; }
        })()
    );
    const history = safeArray(
        (() => {
            try { return JSON.parse(stateRow?.history_json || "[]"); }
            catch { return []; }
        })()
    );
    const userSettings = safeJsonObject(stateRow?.settings_json || "{}", {});
    const diversity = Math.max(0, Math.min(100, Number(userSettings.recommendationDiversity ?? 62)));
    const exploration = diversity / 100;
    const recentPenalty = 5 + (1 - exploration) * 7;

    const weights = new Map();
    const addArtist = (artist, weight) => {
        for (const token of roamArtistTokens(artist)) {
            const key = token.toLowerCase();
            weights.set(key, (weights.get(key) || 0) + weight);
        }
    };

    favorites.slice(0, 80).forEach((track, index) =>
        addArtist(track?.artist, Math.max(5, 16 - index * 0.08)));
    history.slice(0, 60).forEach((track, index) =>
        addArtist(track?.artist, Math.max(1.5, 8 - index * 0.08)));
    addArtist(currentArtist, 10);

    const affinityArtists = [...weights.entries()]
        .sort((a, b) => b[1] - a[1])
        .map(([artist]) => artist)
        .filter(Boolean)
        .slice(0, 5);

    const fallbackArtists = [
        "周杰伦",
        "林俊杰",
        "陈奕迅",
        "孙燕姿",
        "五月天",
        "邓紫棋"
    ];
    const queries = [...affinityArtists];
    for (const artist of fallbackArtists) {
        if (queries.length >= 6)
            break;
        if (!queries.includes(artist.toLowerCase()))
            queries.push(artist);
    }

    const groups = await Promise.all(
        queries.slice(0, 6).map((query) =>
            searchNetease(env, query, 18).catch(() => []))
    );

    const songMap = new Map();
    for (const group of groups) {
        for (const song of group) {
            const id = String(song?.id ?? "");
            if (!id || exclude.has(id) || exclude.has(`netease:${id}`))
                continue;
            if (!songMap.has(id))
                songMap.set(id, song);
        }
    }

    const songs = await enrichNeteaseSongs(
        env,
        [...songMap.values()].slice(0, 72)
    );
    const ownSession = await getUserProviderSession(
        env,
        user.id,
        "netease"
    ).catch(() => null);

    const favoritesIds = new Set();
    for (const track of favorites)
        for (const id of trackIdentityCandidates(track)) favoritesIds.add(id);
    const recentIds = new Set();
    for (const track of history.slice(0, 28))
        for (const id of trackIdentityCandidates(track)) recentIds.add(id);

    const scored = songs.map((song) => {
        const track = normalizeNeteaseSong(
            song,
            null,
            Boolean(ownSession?.cookie)
        );
        let score = Math.random() * (3 + exploration * 10);
        score += discoveryVersionScore(track);
        const artistTokens = roamArtistTokens(track.artist)
            .map((artist) => artist.toLowerCase());
        for (const artist of artistTokens)
            score += weights.get(artist) || 0;
        if (track.cover) score += 1.5;
        if (favoritesIds.has(track.sourceId) || favoritesIds.has(track.id))
            score -= 2.5;
        if (recentIds.has(track.sourceId) || recentIds.has(track.id))
            score -= recentPenalty;
        if (currentTitle && track.title === currentTitle)
            score -= 20;
        return { track, score };
    }).sort((a, b) => b.score - a.score);

    // Keep the feed feeling like a radio station rather than an artist dump.
    // A repeated artist is deferred when another candidate is available.
    const tracks = [];
    const deferred = [];
    let lastArtist = "";
    for (const entry of scored) {
        const primaryArtist = roamArtistTokens(entry.track.artist)[0] || "";
        const artistKey = primaryArtist.toLowerCase();
        if (artistKey && artistKey === lastArtist && tracks.length + deferred.length < scored.length - 1) {
            deferred.push(entry);
            continue;
        }
        tracks.push(entry.track);
        lastArtist = artistKey;
        if (tracks.length >= limit)
            break;
    }
    for (const entry of deferred) {
        if (tracks.length >= limit)
            break;
        if (!tracks.some((track) => track.id === entry.track.id))
            tracks.push(entry.track);
    }

    const displayArtists = affinityArtists
        .slice(0, 3)
        .map((artist) => artist);
    const reason = displayArtists.length
        ? `根据你喜欢的 ${displayArtists.join(" / ")} 推荐 · ${diversity}% 探索度`
        : `根据你的播放与收藏持续探索新歌 · ${diversity}% 探索度`;

    return ok({
        tracks,
        reason,
        basedOn: {
            favorites: favorites.length,
            history: history.length,
            diversity
        }
    });
}

async function handleHome(env, user) {
    const [stateRow, cloudRows] = await Promise.all([
        env.DB.prepare(`SELECT favorites_json, history_json, settings_json FROM user_state WHERE user_id = ?`).bind(user.id).first().catch(()=>null),
        env.DB.prepare(`SELECT id,title,artist,album,cover,duration,access FROM track_catalog ORDER BY created_at DESC LIMIT 24`).all().catch(()=>({results:[]}))
    ]);
    const parseArray = (raw) => { try { const v=JSON.parse(raw||"[]"); return Array.isArray(v)?v:[]; } catch { return []; } };
    const favorites=parseArray(stateRow?.favorites_json), history=parseArray(stateRow?.history_json);
    const settings=safeJsonObject(stateRow?.settings_json||"{}",{});
    const diversity=Math.max(0,Math.min(100,Number(settings.recommendationDiversity ?? 62)));

    const artistWeights=new Map();
    const add=(artist,w)=>{ for(const token of roamArtistTokens(artist)){ const k=token.toLowerCase(); artistWeights.set(k,(artistWeights.get(k)||0)+w); } };
    favorites.slice(0,60).forEach((t,i)=>add(t?.artist,Math.max(5,18-i*.14)));
    history.slice(0,50).forEach((t,i)=>add(t?.artist,Math.max(2,9-i*.11)));
    const topArtists=[...artistWeights.entries()].sort((a,b)=>b[1]-a[1]).map(([a])=>a).filter(Boolean).slice(0,6);
    const fallback=["周杰伦","林俊杰","陈奕迅","孙燕姿","五月天","邓紫棋"];
    const queries=[...topArtists];
    for(const a of fallback){ if(queries.length>=6) break; if(!queries.includes(a.toLowerCase())) queries.push(a); }

    const discovery=await buildNeteaseDiscoveryTracks(env,user,queries,42).catch(()=>[]);
    const recentIds=new Set(); history.slice(0,24).forEach(t=>trackIdentityCandidates(t).forEach(id=>recentIds.add(id)));
    const favoriteIds=new Set(); favorites.forEach(t=>trackIdentityCandidates(t).forEach(id=>favoriteIds.add(id)));
    const cloud=(cloudRows.results||[]).map(normalizeCloudTrack);
    const all=[...discovery,...cloud];
    const seen=new Set();
    const scored=[];
    for(const track of all){
        const key=`${track.providerId}:${track.sourceId}`; if(seen.has(key)) continue; seen.add(key);
        let score=discoveryVersionScore(track);
        for(const a of roamArtistTokens(track.artist).map(x=>x.toLowerCase())) score += (artistWeights.get(a)||0) * (1.25 - diversity/180);
        if(track.cover) score+=10;
        if(favoriteIds.has(track.id)||favoriteIds.has(track.sourceId)) score-=20;
        if(recentIds.has(track.id)||recentIds.has(track.sourceId)) score-=35;
        score += Math.random()*(8+diversity*.18);
        scored.push({track,score});
    }
    scored.sort((a,b)=>b.score-a.score);
    const tracks=[]; const artistUse=new Map();
    for(const e of scored){
        const primary=(roamArtistTokens(e.track.artist)[0]||"").toLowerCase();
        const used=artistUse.get(primary)||0;
        if(primary && used>=2 && tracks.length<18) continue;
        tracks.push(e.track); if(primary) artistUse.set(primary,used+1);
        if(tracks.length>=24) break;
    }

    const labels=["与你最合拍","最近口味延伸","熟悉歌手的新发现","换点新鲜的"];
    const playlists=queries.slice(0,4).map((query,index)=>({
        id:`search:${query}`,sourceId:query,providerId:"search",title:labels[index],name:labels[index],
        subtitle:index===0?`偏好 ${topArtists.slice(0,2).join(" / ")||"持续学习中"}`:(index===3?`${diversity}% 探索度`:query),
        cover:tracks.find(t=>roamArtistTokens(t.artist).some(a=>a.toLowerCase()===String(query).toLowerCase()))?.cover || tracks[index]?.cover || "",
        playCount:0, personalized:true
    }));
    return ok({ playlists, tracks, insight:{ topArtists:topArtists.slice(0,4), diversity, favorites:favorites.length, history:history.length } });
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

function sleepMs(ms) {
    return new Promise((resolve) => setTimeout(resolve, Math.max(0, Number(ms) || 0)));
}

function authorizedAudioSources(env) {
    const raw = String(env.AUTHORIZED_AUDIO_SOURCES_JSON || "").trim();
    if (!raw)
        return [];

    try {
        const rows = JSON.parse(raw);
        if (!Array.isArray(rows))
            return [];

        return rows
            .slice(0, 8)
            .map((row, index) => {
                const item = row && typeof row === "object" ? row : {};
                const id = String(item.id || `source${index + 1}`).trim();
                const name = String(item.name || id).trim();
                const resolveUrl = String(item.resolveUrl || "").trim();
                const token = String(item.token || "").trim();
                const timeoutMs = Math.min(6000, Math.max(800, Number(item.timeoutMs || 2600)));
                try {
                    const parsed = new URL(resolveUrl);
                    if (parsed.protocol !== "https:")
                        return null;
                } catch {
                    return null;
                }
                return { id, name, resolveUrl, token, timeoutMs };
            })
            .filter(Boolean);
    } catch {
        return [];
    }
}

async function resolveAuthorizedAudioSource(entry, track, quality) {
    const started = Date.now();
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), entry.timeoutMs);

    try {
        const headers = {
            "Accept": "application/json",
            "Content-Type": "application/json; charset=utf-8",
            "User-Agent": "EvolveMusic-Cloud/0.11.2"
        };
        if (entry.token)
            headers.Authorization = `Bearer ${entry.token}`;

        const response = await fetch(entry.resolveUrl, {
            method: "POST",
            headers,
            body: JSON.stringify({
                title: String(track.title || ""),
                artist: String(track.artist || ""),
                album: String(track.album || ""),
                sourceProvider: String(track.providerId || "netease"),
                sourceId: String(track.sourceId || ""),
                trackId: String(track.trackId || ""),
                quality: String(quality || "exhigh")
            }),
            redirect: "follow",
            signal: controller.signal
        });

        if (!response.ok)
            return null;

        const body = await response.json().catch(() => null);
        const audioUrl = String(body?.url || body?.streamUrl || "").trim();
        if (!audioUrl)
            return null;

        let parsed;
        try {
            parsed = new URL(audioUrl);
        } catch {
            return null;
        }
        if (parsed.protocol !== "https:" && parsed.protocol !== "http:")
            return null;

        return {
            url: parsed.toString(),
            providerId: `authorized:${entry.id}`,
            providerName: String(body?.providerName || entry.name),
            access: String(body?.access || "authorized"),
            resolver: `authorized:${entry.id}`,
            resolvedQuality: String(body?.quality || quality || "exhigh"),
            latencyMs: Date.now() - started,
            routeType: "authorized"
        };
    } catch {
        return null;
    } finally {
        clearTimeout(timer);
    }
}

async function resolveFastestAuthorizedPlayback(env, userId, track, quality) {
    const preferredStarted = Date.now();
    const preferredPromise = resolveNeteaseAudioUrl(
        env,
        userId,
        String(track.sourceId || ""),
        quality
    ).then((result) => {
        if (!result?.url)
            return null;
        return {
            ...result,
            providerId: "netease",
            providerName: "网易云音乐",
            latencyMs: Date.now() - preferredStarted,
            routeType: "netease"
        };
    }).catch(() => null);

    const externalEntries = authorizedAudioSources(env);
    if (!externalEntries.length)
        return preferredPromise;

    const externalPromises = externalEntries.map((entry) =>
        resolveAuthorizedAudioSource(entry, track, quality));

    const successful = (promise) => promise.then((value) => {
        if (value?.url)
            return value;
        throw new Error("source unavailable");
    });

    const preferredSuccess = successful(preferredPromise);
    let first;
    try {
        first = await Promise.any([
            preferredSuccess,
            ...externalPromises.map(successful)
        ]);
    } catch {
        return null;
    }

    // If NetEase is first, use it immediately. If another authorized source is
    // first, give the official source a short grace window so a near-tie still
    // prefers NetEase without making the user wait on a slow official route.
    if (first.providerId === "netease")
        return first;

    const preferenceGraceMs = Math.min(
        450,
        Math.max(0, Number(env.NETEASE_PREFERENCE_GRACE_MS || 180))
    );
    const preferredWithinGrace = await Promise.race([
        preferredSuccess.catch(() => null),
        sleepMs(preferenceGraceMs).then(() => null)
    ]);
    return preferredWithinGrace?.url ? preferredWithinGrace : first;
}

async function createStreamTicket(request, env, user) {
    const body = safeObject(await request.json()
        .catch(() => ({})));
    const providerId = String(body.providerId || "");
    const sourceId = String(body.sourceId || "");
    const trackId = String(body.trackId || "");
    const quality = String(body.quality || "exhigh");
    const track = {
        providerId,
        sourceId,
        trackId,
        title: String(body.title || ""),
        artist: String(body.artist || ""),
        album: String(body.album || "")
    };

    if (!providerId || !sourceId)
        return fail("missing_track", "缺少播放歌曲标识");

    let resolved = null;

    if (providerId === "netease") {
        resolved = await resolveFastestAuthorizedPlayback(
            env,
            user.id,
            track,
            quality
        );

        if (!resolved?.url) {
            return fail(
                "netease_audio_unavailable",
                "当前可用的授权音源均未返回可播放地址",
                502
            );
        }
    } else if (providerId === "cloud") {
        resolved = {
            providerId: "cloud",
            providerName: "Evolve Cloud",
            access: "cloud",
            routeType: "cloud",
            resolver: "r2",
            latencyMs: 0
        };
    }

    const routeProviderId = String(resolved?.providerId || providerId);
    const upstreamUrl = String(resolved?.url || "");
    const routeType = String(resolved?.routeType || providerId);

    const payload = {
        uid: user.id,
        providerId: routeProviderId,
        originalProviderId: providerId,
        sourceId,
        trackId,
        quality,
        upstreamUrl,
        routeType,
        exp: Date.now() + 6 * 60 * 60 * 1000
    };
    const encoded = base64Url(encoder.encode(JSON.stringify(payload)));
    const signature = await hmacSignature(encoded, env.STREAM_SIGNING_KEY);
    const origin = new URL(request.url).origin;
    const streamUrl = `${origin}/v1/audio?ticket=${encodeURIComponent(encoded)}&sig=${encodeURIComponent(signature)}`;

    return ok({
        url: streamUrl,
        directUrl: upstreamUrl,
        providerId: routeProviderId,
        providerName: String(resolved?.providerName || "Evolve Cloud"),
        access: String(resolved?.access || "cloud"),
        resolver: String(resolved?.resolver || ""),
        latencyMs: Number(resolved?.latencyMs || 0),
        routePolicy: "fastest-authorized-with-netease-preference"
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
    if (String(payload.routeType || "") === "authorized") {
        const upstreamUrl = String(payload.upstreamUrl || "").trim();
        let parsed;
        try {
            parsed = new URL(upstreamUrl);
        } catch {
            return fail("authorized_audio_unavailable", "授权音源地址无效", 502);
        }
        if (parsed.protocol !== "https:" && parsed.protocol !== "http:")
            return fail("authorized_audio_unavailable", "授权音源地址协议无效", 502);
        return proxyStream(request, parsed.toString());
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
                "网易当前账号/地区未返回可播放音频地址（已尝试 v1、旧接口和官方直连）",
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
        if (url.pathname === "/" && request.method === "GET") {
            return ok({
                service: "evolvemusic-cloud",
                version: "0.15.2",
                status: "online",
                accountMode: "username-email-password",
                databaseBound: Boolean(env?.DB && typeof env.DB.prepare === "function"),
                message: "EvolveMusic Cloud 节点在线。客户端会在主节点与备用节点之间自动容灾。"
            });
        }
        if (url.pathname === "/health") {
            return ok({
                service: "evolvemusic-cloud",
                version: "0.15.2",
                cloudFirst: true,
                databaseBound: Boolean(env?.DB && typeof env.DB.prepare === "function")
            });
        }
        if (url.pathname === "/v1/source-sites"
            && request.method === "GET") {
            return ok({
                playbackMode: "web-shell",
                audioProxy: false,
                sources: [
                    { id: "yueting", name: "悦听音乐", url: "https://www.yueting.net", enabledByDefault: true, playback: "site-player" },
                    { id: "gequhai", name: "歌曲海", url: "https://www.gequhai.com", enabledByDefault: true, playback: "site-player" }
                ]
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
        if (url.pathname === "/v1/auth/status"
            && request.method === "GET") {
            const databaseBound = Boolean(env?.DB && typeof env.DB.prepare === "function");
            let schemaReady = false;
            let schemaError = "";
            if (databaseBound) {
                try {
                    await ensureBetaSchema(env);
                    await env.DB.prepare(`SELECT id FROM users LIMIT 1`).first();
                    schemaReady = true;
                } catch (error) {
                    schemaError = String(error?.message || error || "").slice(0, 180);
                }
            }
            return ok({
                service: "evolvemusic-cloud",
                version: "0.15.2",
                accountMode: "username-email-password",
                databaseBound,
                schemaReady,
                passwordHashScheme: PASSWORD_SCHEME,
                passwordIterations: PASSWORD_ITERATIONS,
                authSecretReady: Boolean(accountPasswordSecret(env)),
                emailAuthReady: emailServiceReady(env),
                emailRegistrationRequired: emailRegistrationRequired(env),
                schemaError
            });
        }
        if (url.pathname === "/v1/update/manifest"
            && request.method === "GET") {
            return updateManifest(env);
        }
        if (url.pathname === "/v1/auth/email/send-code"
            && request.method === "POST") {
            try {
                return await sendRegisterEmailCode(request, env);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/auth/email/verify"
            && request.method === "POST") {
            try {
                return await verifyEmailRoute(request, env);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/auth/password/send-code"
            && request.method === "POST") {
            try {
                return await sendPasswordResetCode(request, env);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/auth/password/reset"
            && request.method === "POST") {
            try {
                return await resetPasswordWithEmail(request, env);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/auth/register"
            && request.method === "POST") {
            try {
                return await registerAccount(request, env);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/auth/login"
            && request.method === "POST") {
            try {
                return await loginAccount(request, env);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
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
        if (url.pathname === "/v1/auth/me"
            && request.method === "GET") {
            return ok({ user: {
                id: user.id,
                username: user.username,
                email: user.email || "",
                emailVerified: Boolean(user.emailVerified)
            } });
        }
        if (url.pathname === "/v1/auth/email/change/send-code"
            && request.method === "POST") {
            try {
                return await sendEmailChangeCode(request, env, user);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/auth/email/change"
            && request.method === "POST") {
            try {
                return await applyEmailChange(request, env, user);
            } catch (error) {
                return accountBackendFailure(error, env);
            }
        }
        if (url.pathname === "/v1/presence" && request.method === "POST")
            return presenceRoute(env, user);
        if (url.pathname === "/v1/auth/logout"
            && request.method === "POST") {
            return logoutAccount(request, env, user);
        }
        if (url.pathname === "/v1/me/profile" && request.method === "GET")
            return getMyProfile(env, user);
        if (url.pathname === "/v1/me/profile" && request.method === "PUT")
            return updateMyProfile(request, env, user);
        if (url.pathname === "/v1/users/search" && request.method === "GET")
            return searchUsersRoute(request, env, user);
        if (url.pathname === "/v1/me/following" && request.method === "GET")
            return listFollowingRoute(env, user);
        const followMatch = url.pathname.match(/^\/v1\/users\/([^/]+)\/follow$/);
        if (followMatch && (request.method === "POST" || request.method === "DELETE"))
            return setFollowRoute(request, env, user, decodeURIComponent(followMatch[1]));
        if (url.pathname === "/v1/community/playlists" && request.method === "GET")
            return listCommunityPlaylistsRoute(request, env, user);
        const communityPlaylistMatch = url.pathname.match(/^\/v1\/community\/playlists\/([^/]+)$/);
        if (communityPlaylistMatch && request.method === "GET")
            return getCommunityPlaylistRoute(env, user, decodeURIComponent(communityPlaylistMatch[1]));
        if (url.pathname === "/v1/together/rooms" && request.method === "POST")
            return createTogetherRoomRoute(env, user);
        if (url.pathname === "/v1/together/join" && request.method === "POST")
            return joinTogetherRoomRoute(request, env, user);
        if (url.pathname === "/v1/together/invites" && request.method === "GET")
            return listTogetherInvitesRoute(env, user);
        const togetherInviteDismissMatch = url.pathname.match(/^\/v1\/together\/invites\/([^/]+)$/);
        if (togetherInviteDismissMatch && request.method === "DELETE")
            return dismissTogetherInviteRoute(env, user, decodeURIComponent(togetherInviteDismissMatch[1]));
        const togetherMessagesMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/messages$/);
        if (togetherMessagesMatch && request.method === "GET")
            return listTogetherMessagesRoute(request, env, user, decodeURIComponent(togetherMessagesMatch[1]).toUpperCase());
        if (togetherMessagesMatch && request.method === "POST")
            return sendTogetherMessageRoute(request, env, user, decodeURIComponent(togetherMessagesMatch[1]).toUpperCase());
        const togetherAttachmentsMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/attachments$/);
        if (togetherAttachmentsMatch && request.method === "POST")
            return uploadTogetherAttachmentRoute(request, env, user, decodeURIComponent(togetherAttachmentsMatch[1]).toUpperCase());
        const togetherAttachmentGetMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/attachments\/([^/]+)$/);
        if (togetherAttachmentGetMatch && request.method === "GET")
            return downloadTogetherAttachmentRoute(env, user, decodeURIComponent(togetherAttachmentGetMatch[1]).toUpperCase(), decodeURIComponent(togetherAttachmentGetMatch[2]));
        const togetherVoiceMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/voice$/);
        if (togetherVoiceMatch && (request.method === "GET" || request.method === "POST"))
            return togetherVoiceRoute(request, env, user, decodeURIComponent(togetherVoiceMatch[1]).toUpperCase());
        const togetherRoomMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)$/);
        if (togetherRoomMatch && request.method === "GET")
            return getTogetherRoomRoute(env, user, decodeURIComponent(togetherRoomMatch[1]).toUpperCase());
        const togetherStateMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/state$/);
        if (togetherStateMatch && request.method === "PUT")
            return updateTogetherStateRoute(request, env, user, decodeURIComponent(togetherStateMatch[1]).toUpperCase());
        const togetherLeaveMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/leave$/);
        if (togetherLeaveMatch && request.method === "POST")
            return leaveTogetherRoomRoute(env, user, decodeURIComponent(togetherLeaveMatch[1]).toUpperCase());
        const togetherInviteMatch = url.pathname.match(/^\/v1\/together\/room\/([^/]+)\/invite$/);
        if (togetherInviteMatch && request.method === "POST")
            return inviteTogetherUserRoute(request, env, user, decodeURIComponent(togetherInviteMatch[1]).toUpperCase());
        if (url.pathname === "/v1/me/playlists"
            && request.method === "GET") {
            return listCustomPlaylists(env, user);
        }
        if (url.pathname === "/v1/me/playlists"
            && request.method === "POST") {
            return createCustomPlaylist(request, env, user);
        }
        const playlistMatch = url.pathname.match(/^\/v1\/me\/playlists\/([^/]+)$/);
        if (playlistMatch) {
            const playlistId = decodeURIComponent(playlistMatch[1]);
            if (request.method === "PUT")
                return updateCustomPlaylist(request, env, user, playlistId);
            if (request.method === "DELETE")
                return deleteCustomPlaylist(env, user, playlistId);
        }
        if (url.pathname === "/v1/search"
            && request.method === "GET") {
            return handleSearch(
                request,
                env,
                user
            );
        }
        if (url.pathname === "/v1/roam"
            && request.method === "GET") {
            return handleRoam(request, env, user);
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
