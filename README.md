# Gomoku

一款基于 Qt 的五子棋桌面应用，当前版本重点放在对局体验、联机直连、历史记录、回放和本地账号信息管理。

## 功能

- 本地双人对战
- 人机对战
- 直连联机对战
  - 点击联机后直接输入对方 `IP` 即可连接
  - 不再保留房间创建、房间列表和局域网扫描页面
- 开局对局入场动画
- 对局结束后的结果提示与再来一局
- 历史记录保存与查看
- 最简回放功能
  - 仅回放棋盘落子
  - 默认每步间隔 `0.3s`
- 本地账号信息
  - 昵称
  - 头像
  - 联机时展示双方昵称与头像
- 设置功能
  - 修改昵称
  - 上传头像

## 技术栈

- C++17
- Qt Widgets
- Qt Network
- Qt Sql
- SQLite

## 运行环境

- Qt 6.11.1
- MinGW 64-bit
- Windows

## 构建

推荐使用 Qt Creator 直接打开项目根目录下的 `CMakeLists.txt` 构建运行。

命令行构建示例：

```bash
cmake -S . -B build
cmake --build build --config Release
```

## 默认配置

主要配置集中在 `src/common/config.h`：

- `kBoardSize = 15`
- `kDefaultCellSize = 40`
- `kDefaultPort = 5000`
- `kDefaultDiscoveryPort = 45454`
- `kRoomBroadcastIntervalMs = 1000`
- `kDefaultReplayDelayMs = 300`

## 资源目录

主要资源位于 `res/assets`：

- `icons/`：程序图标
- `backgrounds/`：主界面和对局背景
- `avatars/`：默认头像、AI 头像、黑白方头像
- `board/`：棋盘与棋子贴图

## 目录结构

```text
gomoku/
├── CMakeLists.txt
├── README.md
├── res/
│   └── assets/
└── src/
    ├── ai/
    ├── app/
    ├── common/
    ├── core/
    ├── data/
    ├── network/
    ├── record/
    └── ui/
```

## 模块说明

### `src/app`

程序入口。

### `src/core`

对局核心规则、状态和回合控制。

### `src/ai`

人机落子决策与局面评估。

### `src/network`

联机通信。当前采用直连方式，不再包含房间管理和局域网广播发现。

### `src/data`

本地数据存储，包含账号信息和历史记录相关数据。

### `src/record`

对局记录管理与持久化。

### `src/ui`

主窗口、棋盘组件、历史记录窗口、回放窗口、开局动画等界面实现。

## 说明

- 棋盘默认是 `15` 路。
- 联机模式下会显示双方昵称和头像。
- 如果本地没有账号信息，程序会提示先完成登录/设置。
- 历史记录同时覆盖人机对局和联机对局。
- 回放功能目前是最简版，只回放棋盘落子过程。

