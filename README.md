# EvolveMusic

EvolveMusic 是一款面向 Windows 桌面的 Qt 6 音乐播放器。项目使用 C++20、QML 和 Qt Multimedia，提供多音源检索与播放、歌单与收藏、歌词、沉浸式播放页、迷你播放器，以及可选的账号与社区云功能。

当前源码版本：`0.18.1`

## 主要功能

- 网易云、酷狗、酷我等多音源检索与播放线路回退
- 播放队列、歌词、收藏、历史记录和自定义歌单
- 多种播放器布局与桌面集成
- 可选 Qt WebView / Edge WebView2 登录页
- 本地资料与 Windows DPAPI 会话保护
- 可选 Evolve Cloud 账号、社区歌单和一起听功能

## 仓库内容

```text
components/             QML 通用组件
core/                   应用控制、播放、账号、本地资料与云客户端
pages/                  主要页面
providers/              音源适配与线路解析
assets/                 应用图标等必要资源
installer/              Windows 安装程序配置
scripts/                环境检查、构建、运行与打包脚本
cloudflare/              可选 Cloudflare Worker 后端源码
docs/                   多音源、插件和 Provider 协议文档
CMakeLists.txt           CMake 构建入口
Main.qml / main.cpp      桌面客户端入口
```

以下内容不会提交到 Git：编译产物、`node_modules`、运行缓存、账号数据、网站工程、服务器一键部署包，以及可由脚本重新获取的第三方源码副本。

## 构建要求

- Windows 10/11 x64
- Qt `6.10.x` MinGW 64-bit，包含 Quick、QuickControls2、Multimedia、Network、Widgets
- CMake `3.21+`
- Git for Windows
- 可选：Qt WebView（启用内嵌网页登录）
- 可选：Node.js `22+`（运行本地网易 API 增强服务）

项目当前针对 Qt 6.10 MinGW 验证。其他 Qt 版本或 MSVC 工具链可能需要调整脚本。

## 获取源码与依赖

```bat
git clone https://github.com/YIRT66/QAQ.git EvolveMusic
cd EvolveMusic
scripts\setup_evolveui.bat
```

`EvolveUI` 会被下载到 `third_party/EvolveUI`，该目录是可重建依赖，因此不会提交到本仓库。

如需本地网易 API 增强服务，可额外执行：

```bat
scripts\setup_netease_api.bat
```

它会把上游项目下载到 `runtime/netease-api`。播放器的主要音乐浏览与播放后端为 Ourcraft Music API，本地服务属于可选能力。

## 编译与运行

```bat
scripts\build_windows.bat clean
scripts\run_windows.bat
```

构建结果位于 `build-mingw/EvolveMusic.exe`。如需生成可分发目录：

```bat
scripts\package_windows.bat
```

打包结果写入 `dist/EvolveMusic`，不会被 Git 跟踪。

Qt WebView 是可选组件。缺少它时客户端仍可编译和播放音乐，只会关闭内嵌网页登录；可用 `scripts\check_webview.bat` 检查安装状态。

## 可选配置

运行脚本默认配置 Evolve Cloud 主、备节点，也可以在启动前覆盖：

```bat
set EVOLVE_MUSIC_CLOUD_API=https://your-worker.example.com
set EVOLVE_MUSIC_CLOUD_API_BACKUP=https://your-backup.example.com
scripts\run_windows.bat
```

设置 `EVOLVE_MUSIC_CLOUD_DISABLE=1` 可关闭云端账号与社区功能。后端源码和部署说明见 [`cloudflare/evolvemusic-cloud/README.md`](cloudflare/evolvemusic-cloud/README.md)。

## 文档

- [`ARCHITECTURE.md`](ARCHITECTURE.md)：客户端架构与边界
- [`docs/MULTISOURCE_LOGIC.md`](docs/MULTISOURCE_LOGIC.md)：多音源合并与回退逻辑
- [`docs/PLUGIN_API.md`](docs/PLUGIN_API.md)：插件 API
- [`docs/PROVIDER_GATEWAY.md`](docs/PROVIDER_GATEWAY.md)：Provider 网关约定
- [`CHANGELOG.md`](CHANGELOG.md)：版本记录

## 数据与安全

- 不要提交 `.env`、Cookie、Token、密码、客户端运行数据或日志。
- Windows 下保存的 Provider 会话由当前用户账户的 DPAPI 加密。
- 音源与播放能力应遵守对应平台条款、版权和所在地区法律；本项目不提供版权内容。

## 第三方项目

- [EvolveUI](https://github.com/sudoevolve/EvolveUI) 在构建前由脚本获取，并按其自身许可证使用。
- [NeteaseCloudMusicApiEnhanced/api-enhanced](https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced) 是可选本地服务，并按其自身许可证使用。

本仓库目前未声明统一开源许可证；除上述第三方组件外，未获得明确授权前请勿将项目代码视为已授予开源许可。
