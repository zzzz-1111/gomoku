# 墨弈 Gomoku

一款基于 Qt 的五子棋桌面程序，支持本地对战、人机对战、局域网联机、历史记录、棋局回放、昵称与头像配置。

## 已实现功能

- 本地双人对战
- 人机对战
- 局域网联机对战
- 联机房间发现与加入
- 主机等待客户端加入后再开始对局
- 联机结束后的中文“再来一局”提示
- 头像与昵称本地保存
- 历史记录保存与展示
- 历史对局最简回放
- 棋盘高亮、悬停提示、最后一步标记
- AI 对战入口与难度选择

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

## 启动方式

推荐使用 Qt Creator 打开项目根目录下的 `CMakeLists.txt`，然后直接构建运行。

如果使用命令行构建：

```bash
cmake -S . -B build
cmake --build build --config Release
```

## 默认配置

配置集中在 [src/common/config.h](src/common/config.h)：

- `kBoardSize = 15`
- `kDefaultCellSize = 40`
- `kDefaultPort = 5000`
- `kDefaultDiscoveryPort = 45454`
- `kDefaultReplayDelayMs = 300`

## 目录结构

```text
gomoku/
├── CMakeLists.txt
├── README.md
├── doc/
├── res/
│   └── assets/
│       ├── avatars/
│       ├── backgrounds/
│       ├── board/
│       └── icons/
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

## 资源说明

当前资源索引由 `res.qrc` 管理，主要路径如下：

- `:/assets/icons/app_icon.png`
- `:/assets/backgrounds/home_hero_bg.png`
- `:/assets/backgrounds/game_background.png`
- `:/assets/backgrounds/vs.png`
- `:/assets/avatars/default_avatar.png`
- `:/assets/avatars/ai_avatar.png`
- `:/assets/avatars/black.png`
- `:/assets/avatars/white.png`
- `:/assets/board/board_texture.png`
- `:/assets/board/stone_black.png`
- `:/assets/board/stone_white.png`

## 功能说明

### 1. 主界面

主界面提供入口：

- 本地对战
- 人机对战
- 联机对战
- 历史记录
- 设置
- 退出游戏

### 2. 对局界面

对局页面会显示：

- 当前模式
- 当前玩家
- 当前步数
- 状态提示
- 黑方 / 白方昵称
- 玩家头像

### 3. 联机对战

联机模式采用主机 - 客户端结构：

- 主机创建房间并等待客户端加入
- 客户端扫描并加入房间
- 主机在检测到客户端进入前不可开始落子
- 联机结束后仅主机默认弹出“是否再来一局”

### 4. 历史记录与回放

每局对局都会记录：

- 模式
- 黑方昵称
- 白方昵称
- 胜者
- 总步数
- 起止时间
- 每一步落子

在历史窗口中可以：

- 查看最近对局
- 选择一局回放
- 以 `0.3s` 的间隔自动播放棋盘

### 5. 头像与昵称

程序启动时会读取本地保存的用户信息：

- 如果已有昵称和头像，直接使用
- 如果没有，会提示输入昵称
- 头像可在设置中上传

## 数据存储

历史记录优先写入 SQLite 数据库，数据库不可用时会自动回退到本地 JSON 备用存储，避免对局记录丢失。

## 开发说明

- 棋盘尺寸当前默认使用 `15` 路
- 联机端口和回放间隔都已集中在 `src/common/config.h`
- 新增资源时，记得同步更新 `res.qrc`
- 新增 UI 入口时，优先在 `src/ui/mainwindow.ui` 里调整，而不是完全动态生成

## 主要源码

- [src/app/main.cpp](src/app/main.cpp)
- [src/ui/mainwindow.cpp](src/ui/mainwindow.cpp)
- [src/ui/mainwindow.ui](src/ui/mainwindow.ui)
- [src/ui/history_dialog.cpp](src/ui/history_dialog.cpp)
- [src/ui/replay_dialog.cpp](src/ui/replay_dialog.cpp)
- [src/ui/chessboard_widget.cpp](src/ui/chessboard_widget.cpp)
- [src/core/game_controller.cpp](src/core/game_controller.cpp)
- [src/network/network_manager.cpp](src/network/network_manager.cpp)
- [src/network/game_server.cpp](src/network/game_server.cpp)
- [src/record/record_manager.cpp](src/record/record_manager.cpp)

## 说明

这个项目当前是一个可运行的 Qt 五子棋应用，重点在于：

- 对局体验
- 联机流程
- 记录保存
- 历史查看
- 最简回放

如果你想继续扩展，可以优先做：

1. 回放控制条
2. 联机断线重连
3. 更完整的战绩页
4. 更精致的动画层
