# EvolveMusic Cloud v0.15.2 部署说明

注册修复必须部署到 Cloudflare Worker 后才会生效，重新编译桌面客户端不能替代服务端部署。

1. 打开 Cloudflare Dashboard，进入当前 EvolveMusic Worker 的“编辑代码”。
2. 用 `worker-dashboard-v0.15.2.js` 的完整内容替换 Worker 代码并点击“部署”。
3. 如果主域名和备用域名对应两个不同 Worker，请两边都部署同一份文件；如果只是同一 Worker 的两个路由，只部署一次。
4. 保留现有绑定和密钥：D1 绑定名 `DB`，以及 `PASSWORD_PEPPER`（或原有会话主密钥）、`RESEND_API_KEY`、`EMAIL_HASH_SECRET`、`EMAIL_FROM`。
5. 部署后打开 `/v1/auth/status`，确认 `version` 为 `0.15.2`，且 `schemaReady`、`authSecretReady`、`emailAuthReady` 均为 `true`。

本版把新账户密码摘要改为适配 Workers 免费 CPU 配额的带服务端密钥 HMAC，并保留旧 PBKDF2 账户的读取与升级分支，不会删除现有账户。
