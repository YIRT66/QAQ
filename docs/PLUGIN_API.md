# EvolveMusic 本地扩展 API

EvolveMusic 插件是用户明确安装到插件目录的本地扩展，不是一个独立页面。插件默认关闭，只有声明的权限全部由用户授权后才能启用。

## Manifest

```json
{
  "id": "example.plugin",
  "name": "Example Plugin",
  "version": "1.0.0",
  "entry": "Main.qml",
  "permissions": ["player.read", "player.control"],
  "capabilities": ["background"],
  "contributes": {
    "commands": ["example.toggle"]
  }
}
```

## 权限命名

- `player.read`: 读取当前歌曲、队列和播放状态。
- `player.control`: 播放、暂停、切歌、定位、音量、倍速。
- `library.read`: 读取收藏、历史和歌单。
- `library.write`: 修改收藏和自建歌单。
- `network`: 访问网络。插件应同时声明允许访问的域名。
- `files.read`: 读取用户选择或授权目录中的文件。
- `files.write`: 写入用户选择或授权目录中的文件。
- `account.profile`: 读取 Evolve 账户公开资料。
- `account.private`: 读取账户私有数据，属于敏感权限。

撤销任意权限会立即停用插件。当前版本只自动加载已启用的本地 QML 后台扩展；菜单、主题、页面插槽、音源、快捷键和命令贡献点将沿用 `contributes` 字段逐步接入统一注册表。

## 安全边界

插件目录中的 QML 在本机运行，只应安装可信来源。EvolveMusic 不会把网易云 Cookie、Ourcraft 服务端凭据或 Evolve Cloud Token 注入插件。网络、文件和账户权限必须分开授权。
