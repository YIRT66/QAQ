interface Env {}

const API_VERSION = 1;

const SOURCE_POLICY = {
  mode: "netease-first",
  priorities: {
    netease: 0,
    qq: 20,
    kugou: 30,
    yueting: 40,
    "2t58": 50
  },
  authenticatedNeteaseFirst: true,
  sharedProviderCredentials: false,
  credentialLocation: "local-profile-only"
};

const PRIVACY_POLICY = {
  profileData: "local-only",
  providerSessions: "local-only",
  cloudStoresProviderCredentials: false,
  cloudStoresFavorites: false,
  cloudStoresHistory: false,
  requestContainsProfileId: false
};

function corsHeaders(): HeadersInit {
  return {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET,OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type,Accept",
    "Access-Control-Max-Age": "86400"
  };
}

function json(
  data: unknown,
  status = 200,
  cacheSeconds = 60
): Response {
  return new Response(JSON.stringify(data, null, 2), {
    status,
    headers: {
      "Content-Type": "application/json; charset=utf-8",
      "Cache-Control": `public, max-age=${cacheSeconds}`,
      ...corsHeaders()
    }
  });
}

function html(body: string): Response {
  return new Response(body, {
    headers: {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "public, max-age=60",
      ...corsHeaders()
    }
  });
}

function statusPage(origin: string): string {
  return `<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EvolveMusic API</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{
  margin:0;min-height:100vh;display:grid;place-items:center;
  font-family:Inter,"Segoe UI","Microsoft YaHei",system-ui,sans-serif;
  background:#0d1114;color:#eef4f1
}
main{
  width:min(760px,calc(100% - 32px));padding:34px;border-radius:22px;
  border:1px solid #26312e;background:#141b19;box-shadow:0 24px 80px #0008
}
.badge{
  display:inline-flex;padding:7px 12px;border-radius:999px;
  background:#163b34;color:#59e6be;font-size:13px;font-weight:700
}
h1{font-size:34px;margin:18px 0 8px}
p{line-height:1.7;color:#aab8b3}
code{
  display:block;margin-top:10px;padding:13px 15px;border-radius:12px;
  background:#0b100f;color:#8cf1d2;overflow:auto
}
.grid{
  display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));
  gap:12px;margin-top:22px
}
.card{
  padding:16px;border-radius:14px;background:#101715;border:1px solid #202b28
}
.card strong{display:block;margin-bottom:6px}
small{color:#7e918a}
</style>
</head>
<body>
<main>
  <span class="badge">ONLINE · API v${API_VERSION}</span>
  <h1>EvolveMusic API</h1>
  <p>
    这个 Worker 只下发公开应用策略和健康状态。
    不接收、不保存、不代理网易云/QQ/酷狗账号 Cookie，
    也不保存本地用户收藏、历史或播放数据。
  </p>
  <div class="grid">
    <div class="card"><strong>网易云优先</strong><small>priority = 0</small></div>
    <div class="card"><strong>本地用户隔离</strong><small>sessions stay on device</small></div>
    <div class="card"><strong>Cloudflare</strong><small>stateless Worker</small></div>
  </div>
  <p>API:</p>
  <code>${origin}/health</code>
  <code>${origin}/v1/config</code>
  <code>${origin}/v1/privacy</code>
</main>
</body>
</html>`;
}

export default {
  async fetch(request: Request, _env: Env): Promise<Response> {
    if (request.method === "OPTIONS") {
      return new Response(null, {
        status: 204,
        headers: corsHeaders()
      });
    }

    if (request.method !== "GET") {
      return json(
        { ok: false, error: "method_not_allowed" },
        405,
        0
      );
    }

    const url = new URL(request.url);

    if (url.pathname === "/")
      return html(statusPage(url.origin));

    if (url.pathname === "/health") {
      return json(
        {
          ok: true,
          service: "evolvemusic-api",
          apiVersion: API_VERSION,
          time: new Date().toISOString()
        },
        200,
        10
      );
    }

    if (url.pathname === "/v1/config") {
      return json({
        ok: true,
        apiVersion: API_VERSION,
        app: {
          currentPolicyVersion: "0.9.1",
          minimumClientVersion: "0.9.0"
        },
        sourcePolicy: SOURCE_POLICY,
        privacy: PRIVACY_POLICY
      });
    }

    if (url.pathname === "/v1/privacy") {
      return json({
        ok: true,
        apiVersion: API_VERSION,
        privacy: PRIVACY_POLICY
      });
    }

    return json(
      { ok: false, error: "not_found" },
      404,
      0
    );
  }
};
