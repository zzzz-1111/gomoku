# 五子棋项目美工素材清单

这份清单只列“真正需要找的美工素材”。  
原则是：**棋盘核心尽量用代码绘制，素材只负责氛围、图标和装饰。**

## 1. 总原则

- 棋盘本体、棋子、网格线、提示点：优先用 `QPainter` 绘制
- 图片素材主要用于：首页、大厅、按钮图标、背景纹理、状态装饰
- 素材风格统一，尽量保持“深绿 + 暖木色 + 简洁现代”

## 2. 必需素材

| 类别 | 素材名称 | 用途 | 建议格式 | 建议尺寸 | 优先级 | 搜索关键词 |
|---|---|---|---|---|---|---|
| 应用标识 | 项目 Logo / App Icon | 窗口标题栏、任务栏、启动入口 | SVG、PNG | 256x256、512x512 | 高 | `gomoku logo`, `board game icon`, `minimal game logo` |
| 首页背景 | 首页主视觉背景 | 首页或大厅页的背景氛围 | PNG、JPG | 1920x1080 | 高 | `dark green texture`, `wood texture`, `minimal gradient background` |
| 导航图标 | 首页菜单图标 | 本地双人、AI、联机、历史、设置等按钮 | SVG | 24x24、32x32 | 高 | `ui icons`, `line icons`, `dashboard icons` |
| 返回图标 | 返回 / 后退 | 页面切换按钮 | SVG | 24x24 | 高 | `back icon`, `arrow left icon` |
| 设置图标 | 系统设置 | 设置页入口 | SVG | 24x24 | 高 | `settings icon`, `gear icon` |
| 开始图标 | 开始对局 | 主按钮、进入游戏按钮 | SVG | 24x24 | 高 | `play icon`, `start icon` |
| 联机图标 | 网络 / 房间状态 | 联机大厅、连接状态、房间状态提示 | SVG | 24x24 | 高 | `network icon`, `wifi icon`, `server icon`, `room icon` |
| 历史图标 | 历史记录 | 历史对局入口 | SVG | 24x24 | 高 | `history icon`, `clock icon`, `archive icon` |

## 3. 推荐素材

| 类别 | 素材名称 | 用途 | 建议格式 | 建议尺寸 | 优先级 | 搜索关键词 |
|---|---|---|---|---|---|---|
| 棋盘纹理 | 木纹 / 纸纹背景 | 棋盘区或卡片背景 | PNG | 1024x1024 或更高 | 中 | `wood texture`, `paper texture`, `parchment texture` |
| 棋子质感 | 黑棋 / 白棋阴影纹理 | 给棋子增加真实感 | PNG、透明背景 PNG | 256x256 | 中 | `black stone texture`, `white stone texture`, `go stone` |
| 高亮效果 | 最后一步 / 推荐落点光晕 | 提示当前重点位置 | PNG、透明背景 PNG | 128x128 | 中 | `glow circle`, `highlight marker`, `selection ring` |
| 状态卡片背景 | 卡片底纹 | 首页统计卡片、信息面板 | PNG | 800x600 | 中 | `subtle gradient background`, `card texture` |
| 分割线装饰 | 细分割线 / 金属线 | 侧边栏、信息区分割 | SVG、PNG | 1x 若干 | 中 | `divider line`, `minimal separator` |
| 默认头像 | 通用玩家头像 | 玩家昵称、战绩页 | PNG、SVG | 128x128 | 中 | `avatar placeholder`, `user avatar`, `anonymous avatar` |

## 4. 可选素材

| 类别 | 素材名称 | 用途 | 建议格式 | 建议尺寸 | 优先级 | 搜索关键词 |
|---|---|---|---|---|---|---|
| 空状态插图 | 无历史记录 / 未连接 / 等待对手 | 提升页面完整度 | PNG、SVG | 512x512 | 低 | `empty state illustration`, `waiting illustration` |
| 徽章图标 | 胜利、连胜、积分、段位 | 战绩页展示 | SVG、PNG | 32x32、64x64 | 低 | `badge icon`, `trophy icon`, `medal icon` |
| 聊天图标 | 消息、表情、发送 | 联机聊天区 | SVG | 24x24 | 低 | `chat icon`, `send icon`, `message icon` |
| 播放控制图标 | 播放、暂停、快进、上一手、下一手 | 回放页 | SVG | 24x24 | 低 | `play icon`, `pause icon`, `forward icon`, `rewind icon` |
| 胜负特效 | 五连胜利特效、轻微粒子 | 对局结束动画 | PNG、Lottie 也可 | 不固定 | 低 | `victory effect`, `confetti`, `shine effect` |

## 5. 首页最少需要的一组素材

如果只做首页预览，最低配置建议是：

- 1 个项目 Logo
- 1 张首页背景图
- 8 到 12 个导航/功能图标
- 1 套卡片底纹或背景纹理
- 1 套棋盘预览相关图层：
  - 棋盘底板纹理
  - 黑棋质感
  - 白棋质感
  - 推荐落点高亮

## 6. 棋盘页建议素材

棋盘页尽量少依赖图片，建议只准备：

- 棋盘底板纹理
- 黑白棋子质感
- 选中高亮圈
- 最后一步高亮圈
- 五连胜利线或闪光效果

其余部分用代码绘制，这样缩放和适配会更稳。

## 7. 大厅页建议素材

大厅页是最适合放素材的地方，建议准备：

- 大背景图
- 页面卡片纹理
- 模式入口图标
- 数据统计图标
- 默认头像
- 状态提示图标

## 8. 搜索素材时的关键词组合

可以直接这样搜：

- `dark green UI background`
- `wood texture board game`
- `minimal line icon set`
- `game dashboard background`
- `paper texture dark theme`
- `go stone texture`
- `selection ring glow`
- `empty state illustration minimal`

## 9. 推荐来源

- 图标：Lucide、Font Awesome、Material Symbols
- 背景和纹理：Unsplash、Pexels、Pixabay
- 透明装饰图：Pixabay、Pexels

## 10. 不建议找的素材

- 复杂卡通插画
- 花哨科幻背景
- 过于写实的人物图
- 与五子棋无关的大场景图
- 颜色过多、风格不统一的图标包

这些东西很容易让界面变乱，也不利于答辩展示。

## 11. 最终建议

优先级按这个顺序找就够了：

1. Logo 和图标
2. 首页背景
3. 棋盘页棋子质感
4. 卡片和面板纹理
5. 空状态图和胜负特效

如果时间不够，先把前 3 类做好，页面就已经能看起来完整了。

## 12. 文件级素材清单

下面这份是最实用的版本。你可以直接按这个文件名去找图，然后放到对应目录里。

### 12.1 推荐目录结构

```text
res/
└── assets/
    ├── app/
    ├── home/
    ├── icons/
    ├── board/
    ├── cards/
    ├── profile/
    ├── empty/
    ├── badges/
    ├── chat/
    └── replay/
```

### 12.2 必需文件

| 文件名 | 建议路径 | 用途 | 格式 | 备注 |
|---|---|---|---|---|
| `app_icon.svg` | `res/assets/app/app_icon.svg` | 程序图标 | SVG | 同时可导出 `app_icon_256.png` |
| `app_icon_256.png` | `res/assets/app/app_icon_256.png` | Windows / Qt 窗口图标 | PNG | 256x256 |
| `app_icon_512.png` | `res/assets/app/app_icon_512.png` | 高分辨率图标 | PNG | 512x512 |
| `home_hero_bg.png` | `res/assets/home/home_hero_bg.png` | 首页主背景 | PNG / JPG | 1920x1080 |
| `home_hero_overlay.png` | `res/assets/home/home_hero_overlay.png` | 首页叠层氛围图 | PNG | 透明背景更好 |
| `icon_local_two_player.svg` | `res/assets/icons/icon_local_two_player.svg` | 本地双人按钮 | SVG | 线性风格 |
| `icon_ai_battle.svg` | `res/assets/icons/icon_ai_battle.svg` | 人机对战按钮 | SVG | 线性风格 |
| `icon_network_battle.svg` | `res/assets/icons/icon_network_battle.svg` | 局域网对战按钮 | SVG | 线性风格 |
| `icon_history.svg` | `res/assets/icons/icon_history.svg` | 历史棋局按钮 | SVG | 线性风格 |
| `icon_stats.svg` | `res/assets/icons/icon_stats.svg` | 玩家战绩按钮 | SVG | 线性风格 |
| `icon_settings.svg` | `res/assets/icons/icon_settings.svg` | 系统设置按钮 | SVG | 线性风格 |
| `icon_exit.svg` | `res/assets/icons/icon_exit.svg` | 退出按钮 | SVG | 线性风格 |
| `icon_back.svg` | `res/assets/icons/icon_back.svg` | 返回按钮 | SVG | 线性风格 |
| `icon_play.svg` | `res/assets/icons/icon_play.svg` | 开始按钮 | SVG | 线性风格 |
| `icon_refresh.svg` | `res/assets/icons/icon_refresh.svg` | 重新开始 | SVG | 线性风格 |

### 12.3 棋盘页文件

| 文件名 | 建议路径 | 用途 | 格式 | 备注 |
|---|---|---|---|---|
| `board_texture.png` | `res/assets/board/board_texture.png` | 棋盘底板纹理 | PNG | 木纹或纸纹 |
| `board_shadow.png` | `res/assets/board/board_shadow.png` | 棋盘阴影 | PNG | 透明背景 |
| `stone_black.png` | `res/assets/board/stone_black.png` | 黑棋质感参考 | PNG | 透明背景，圆形 |
| `stone_white.png` | `res/assets/board/stone_white.png` | 白棋质感参考 | PNG | 透明背景，圆形 |
| `stone_last_move.png` | `res/assets/board/stone_last_move.png` | 最后一步高亮 | PNG | 透明背景 |
| `stone_suggested.png` | `res/assets/board/stone_suggested.png` | 推荐落点高亮 | PNG | 透明背景 |
| `board_star_point.png` | `res/assets/board/board_star_point.png` | 星位标记 | PNG | 可选 |
| `board_win_line.svg` | `res/assets/board/board_win_line.svg` | 胜利连线效果 | SVG | 可选 |
| `board_hover_ring.png` | `res/assets/board/board_hover_ring.png` | 鼠标悬停提示 | PNG | 透明背景 |

### 12.4 首页和大厅页文件

| 文件名 | 建议路径 | 用途 | 格式 | 备注 |
|---|---|---|---|---|
| `card_bg.png` | `res/assets/cards/card_bg.png` | 首页统计卡片背景 | PNG | 半透明纹理 |
| `card_border.png` | `res/assets/cards/card_border.png` | 卡片边框装饰 | PNG / SVG | 也可代码画 |
| `divider_line.svg` | `res/assets/cards/divider_line.svg` | 区域分割线 | SVG | 细线风格 |
| `avatar_placeholder.png` | `res/assets/profile/avatar_placeholder.png` | 默认头像 | PNG | 圆形或方形均可 |
| `empty_history.svg` | `res/assets/empty/empty_history.svg` | 无历史记录 | SVG | 空状态插图 |
| `empty_network.svg` | `res/assets/empty/empty_network.svg` | 未连接 / 等待连接 | SVG | 空状态插图 |
| `empty_records.svg` | `res/assets/empty/empty_records.svg` | 无战绩记录 | SVG | 空状态插图 |
| `badge_win.svg` | `res/assets/badges/badge_win.svg` | 胜利徽章 | SVG | 战绩页使用 |
| `badge_streak.svg` | `res/assets/badges/badge_streak.svg` | 连胜徽章 | SVG | 战绩页使用 |
| `badge_rank.svg` | `res/assets/badges/badge_rank.svg` | 段位徽章 | SVG | 战绩页使用 |

### 12.5 联机和聊天文件

| 文件名 | 建议路径 | 用途 | 格式 | 备注 |
|---|---|---|---|---|
| `icon_server.svg` | `res/assets/icons/icon_server.svg` | 创建服务器 / 主机状态 | SVG | 联机页 |
| `icon_room.svg` | `res/assets/icons/icon_room.svg` | 房间状态 | SVG | 联机页 |
| `icon_user.svg` | `res/assets/icons/icon_user.svg` | 玩家头像 / 用户信息 | SVG | 联机页 |
| `icon_chat.svg` | `res/assets/chat/icon_chat.svg` | 聊天消息 | SVG | 联机页 |
| `icon_send.svg` | `res/assets/chat/icon_send.svg` | 发送消息 | SVG | 联机页 |
| `icon_emoji.svg` | `res/assets/chat/icon_emoji.svg` | 表情按钮 | SVG | 联机页，可选 |

### 12.6 回放文件

| 文件名 | 建议路径 | 用途 | 格式 | 备注 |
|---|---|---|---|---|
| `icon_replay_play.svg` | `res/assets/replay/icon_replay_play.svg` | 播放回放 | SVG | 回放页 |
| `icon_replay_pause.svg` | `res/assets/replay/icon_replay_pause.svg` | 暂停回放 | SVG | 回放页 |
| `icon_replay_prev.svg` | `res/assets/replay/icon_replay_prev.svg` | 上一步 | SVG | 回放页 |
| `icon_replay_next.svg` | `res/assets/replay/icon_replay_next.svg` | 下一步 | SVG | 回放页 |
| `icon_replay_fast.svg` | `res/assets/replay/icon_replay_fast.svg` | 快进 / 自动播放 | SVG | 回放页，可选 |

## 13. 你最先要找的文件

如果你想先把首页做出来，只需要先找这 10 个文件：

1. `res/assets/app/app_icon.svg`
2. `res/assets/app/app_icon_256.png`
3. `res/assets/home/home_hero_bg.png`
4. `res/assets/icons/icon_local_two_player.svg`
5. `res/assets/icons/icon_ai_battle.svg`
6. `res/assets/icons/icon_network_battle.svg`
7. `res/assets/icons/icon_history.svg`
8. `res/assets/icons/icon_settings.svg`
9. `res/assets/cards/card_bg.png`
10. `res/assets/board/board_texture.png`

如果你只想把“首页预览图”先做出来，这 10 个基本够用。

## 14. 文件对应网站索引

下面这张表把“文件名”和“应该去哪里找”直接对应起来。  
你可以先按这个表去找图，再按前面的建议命名保存。

| 文件名 | 推荐网站 | 备注 |
|---|---|---|
| `app_icon.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 先找线性风格图标，再导出 SVG/PNG |
| `app_icon_256.png` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 由图标导出 PNG 即可 |
| `app_icon_512.png` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 由图标导出 PNG 即可 |
| `home_hero_bg.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 搜背景、纹理、木纹、纸纹 |
| `home_hero_overlay.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 适合找透明氛围图或渐变叠层素材 |
| `icon_local_two_player.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 本地对战、用户、棋盘、游戏相关图标 |
| `icon_ai_battle.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | AI、机器人、大脑、星标类图标 |
| `icon_network_battle.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 网络、服务器、wifi、链路类图标 |
| `icon_history.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 时钟、历史、归档类图标 |
| `icon_stats.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 数据、图表、统计类图标 |
| `icon_settings.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 设置、齿轮、调节类图标 |
| `icon_exit.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 退出、关门、箭头类图标 |
| `icon_back.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 左箭头、返回图标 |
| `icon_play.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 播放、开始图标 |
| `icon_refresh.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 刷新、重开、循环图标 |
| `board_texture.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 木纹、纸纹、棋盘背景材质 |
| `board_shadow.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 适合找柔和阴影或暗部纹理 |
| `stone_black.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 搜棋子、石头、圆形高光材质 |
| `stone_white.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 搜白石、圆石、浅色石头材质 |
| `stone_last_move.png` | [Pixabay](https://pixabay.com/service/license-summary/)、[Pexels](https://www.pexels.com/license/) | 推荐找高亮圆环或发光叠层 |
| `stone_suggested.png` | [Pixabay](https://pixabay.com/service/license-summary/)、[Pexels](https://www.pexels.com/license/) | 推荐找光圈、提示圈、目标圈 |
| `board_star_point.png` | [Pixabay](https://pixabay.com/service/license-summary/) | 星位点也可以直接自己画 |
| `board_win_line.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 实际上也可以直接用代码画线 |
| `board_hover_ring.png` | [Pixabay](https://pixabay.com/service/license-summary/)、[Pexels](https://www.pexels.com/license/) | 鼠标悬停高亮圈 |
| `card_bg.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 卡片底纹、磨砂背景、渐变背景 |
| `card_border.png` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 也可自己画边框，不一定要找图 |
| `divider_line.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 细分割线、短横线元素 |
| `avatar_placeholder.png` | [Unsplash](https://unsplash.com/license)、[Pexels](https://www.pexels.com/license/)、[Pixabay](https://pixabay.com/service/license-summary/) | 可找默认人物头像或纯色占位图 |
| `empty_history.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 空状态插图也可以用图标组合代替 |
| `empty_network.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 网络断开、等待连接类图标 |
| `empty_records.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 战绩为空状态图标 |
| `badge_win.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 勋章、奖杯、皇冠类图标 |
| `badge_streak.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 连胜、火焰、闪电类图标 |
| `badge_rank.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[iconfont](https://www.iconfont.cn/) | 段位、徽章、等级类图标 |
| `icon_server.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 服务器、主机、网络节点图标 |
| `icon_room.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 房间、门牌、卡片类图标 |
| `icon_user.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 用户、头像、个人中心图标 |
| `icon_chat.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 聊天气泡、消息图标 |
| `icon_send.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 发送箭头、纸飞机图标 |
| `icon_emoji.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 表情、笑脸图标 |
| `icon_replay_play.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 播放按钮图标 |
| `icon_replay_pause.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 暂停按钮图标 |
| `icon_replay_prev.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 上一步、后退图标 |
| `icon_replay_next.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 下一步、前进图标 |
| `icon_replay_fast.svg` | [Lucide](https://lucide.dev/icons)、[Font Awesome](https://fontawesome.com/icons)、[Material Symbols](https://fonts.google.com/icons)、[iconfont](https://www.iconfont.cn/) | 快进、倍速图标 |

## 15. 最省事的找图顺序

如果你想先快速完成首页，建议按这个顺序找：

1. `app_icon.svg`
2. `home_hero_bg.png`
3. `icon_local_two_player.svg`
4. `icon_ai_battle.svg`
5. `icon_network_battle.svg`
6. `icon_history.svg`
7. `icon_settings.svg`
8. `card_bg.png`
9. `board_texture.png`
10. `stone_black.png`
11. `stone_white.png`
12. `stone_suggested.png`

这样首页、大厅页、对局页的基础视觉就能先立住。
