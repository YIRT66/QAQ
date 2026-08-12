# EvolveMusic Cloud

这是 EvolveMusic 桌面客户端的可选 Cloudflare Worker 后端源码。它负责账号、用户资料、社区歌单、一起听、更新清单，以及可选的云端媒体代理能力。

`src/index.ts` 是唯一的 Worker 源码入口；仓库不再保存 Dashboard 粘贴副本或按版本复制的导出文件。

## 依赖与绑定

- Node.js 22+
- Cloudflare Wrangler
- D1 数据库绑定：`DB`
- 可选 R2 存储桶绑定：`AUDIO`
- 可选 R2 存储桶绑定：`ROOM_FILES`

必须通过 Wrangler Secret 配置：

```text
SESSION_MASTER_KEY
STREAM_SIGNING_KEY
ADMIN_KEY
```

可选变量包括 `NETEASE_API_ORIGIN`、`AUTHORIZED_AUDIO_SOURCES_JSON` 以及 `EVOLVE_*` 更新清单变量。不要把真实密钥、数据库 ID 或生产环境配置提交到 Git。

## 本地检查

```bat
npm install
npm run check
```

`npm run check` 使用 Wrangler 进行 dry-run，只验证 Worker 能否打包，不会部署。

## 部署

1. 复制 `wrangler.example.jsonc` 为 `wrangler.jsonc`。
2. 填写自己的 D1 database ID，并确认 R2 存储桶名称。
3. 应用数据库结构并设置 Secret。
4. 执行部署。

```bat
npx wrangler d1 execute evolvemusic-db --remote --file=./schema.sql
npx wrangler secret put SESSION_MASTER_KEY
npx wrangler secret put STREAM_SIGNING_KEY
npx wrangler secret put ADMIN_KEY
npm run deploy
```

部署完成后访问 `/health` 验证服务状态，再通过 `EVOLVE_MUSIC_CLOUD_API` 指向自己的 Worker 地址。
