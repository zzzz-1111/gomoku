# 简单联机头像入场动画方案

## 目标

在联机对局正式开始前，播放一个 1.5 到 2 秒的简单过场：

- 左侧显示一名玩家头像
- 右侧显示另一名玩家头像
- 两个头像向中间移动
- 中间短暂闪一下
- 动画结束后进入棋盘并允许落子

这个版本只追求“有对战开始的仪式感”，不追求复杂同步、粒子、音效或完整动画系统。

## 最简单方案

新增一个临时浮层控件，覆盖在主窗口上方。

动画播放时：

1. 先让棋盘不可点击
2. 显示半透明背景
3. 左右两个头像从屏幕两边滑向中间
4. 中间显示“对局开始”
5. 头像轻微放大一次
6. 浮层淡出
7. 恢复棋盘输入

整个动画只在本机播放，不要求两台电脑毫秒级同步。

## 为什么这是最小改动

当前项目已经具备这些数据：

- 本机昵称：`localAccountName_`
- 本机头像：`localAvatarPath_`
- 对方昵称：`remoteAccountName_`
- 对方头像：`remoteAvatarBytes_`
- 本机黑白方：`humanSide_`
- 联机开始事件：`GAME_START` / `clientConnected`

所以动画不需要再改网络协议，只需要复用已经拿到的双方资料。

## 建议新增文件

新增两个文件：

- `src/ui/match_intro_overlay.h`
- `src/ui/match_intro_overlay.cpp`

新增一个轻量 QWidget：

```cpp
class MatchIntroOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit MatchIntroOverlay(QWidget *parent = nullptr);
    void play(const IntroPlayerInfo &leftPlayer,
              const IntroPlayerInfo &rightPlayer,
              std::function<void()> finished);
};
```

其中 `IntroPlayerInfo` 只需要：

```cpp
struct IntroPlayerInfo
{
    QString name;
    QString sideText;
    QPixmap avatar;
};
```

## 动画结构

推荐只用 Qt 自带动画：

- `QPropertyAnimation`
- `QParallelAnimationGroup`
- `QSequentialAnimationGroup`
- `QGraphicsOpacityEffect`

控件结构：

- 一个全屏半透明背景
- 左头像 QLabel
- 右头像 QLabel
- 左昵称 QLabel
- 右昵称 QLabel
- 中间标题 QLabel

动画时间建议：

- 淡入：200ms
- 头像移动：600ms
- 中间缩放/闪烁：300ms
- 淡出：300ms

总时长控制在 1.4 秒左右。

## 触发时机

主机端：

- 在 `GameServer::clientConnected` 对应的 MainWindow 回调里触发
- 也就是客户端完成 `LOGIN` 后
- 此时双方昵称和头像已经齐全

客户端：

- 在收到 `GAME_START` 后触发
- 此时主机昵称和头像已经在消息里传过来

## 输入控制

动画期间必须禁用棋盘输入。

最简单处理：

1. 触发动画前调用 `boardWidget_->setMoveInputEnabled(false)`
2. 动画结束后调用 `updateBoardMoveInput()`

这样不会因为动画没播完就提前落子。

## 黑白方显示

动画中不要用“左边一定是主机、右边一定是客户端”来表达胜负关系。

更简单稳定的显示方式：

- 左侧显示黑方
- 右侧显示白方

这样玩家一眼能看懂谁先手，也不会因为主机选白方导致显示混乱。

组装数据时：

- 如果 `humanSide_ == PlayerSide::Black`，本机放左边，对方放右边
- 如果 `humanSide_ == PlayerSide::White`，对方放左边，本机放右边

## 页面切换顺序

建议顺序如下：

1. 收到联机开始事件
2. 调用 `startGame(...)` 切到对局页并初始化棋盘
3. 立即禁用棋盘输入
4. 播放头像入场动画
5. 动画结束后调用 `updateBoardMoveInput()`

这样实现最简单，也不会影响现有棋盘初始化逻辑。

## 不建议第一版做的事

第一版不要做这些：

- 不做音效
- 不做粒子
- 不做复杂遮罩
- 不做双方精确同步倒计时
- 不新增网络消息类型
- 不把动画嵌进棋盘绘制逻辑

这些都可以后续加，但第一版会明显增加调试成本。

## 预计改动量

最小版本大概需要：

- 新增 2 个 UI 文件
- `CMakeLists.txt` 增加这两个文件
- `MainWindow` 增加一个播放入口函数
- 在主机和客户端联机开始处各调用一次

核心逻辑不会超过几百行。

## 推荐实现顺序

1. 新增 `MatchIntroOverlay`
2. 先用默认头像跑通移动动画
3. 接入 `localAvatarPath_` 和 `remoteAvatarBytes_`
4. 接入昵称和黑白方标签
5. 接到 `GAME_START` 和 `clientConnected`
6. 编译并手动验证主机/客户端各一次

## 风险点

### 头像为空

如果对方没有头像数据，直接使用默认头像。

### 动画重复播放

同一局只在开始时播放一次。再来一局时可以重新播放，也可以先不播放。

第一版建议：

- 初次联机开始播放
- 再来一局暂时不播放

### 用户快速返回页面

如果动画播放中用户点返回或断线，浮层需要自动关闭。

最简单做法：

- 浮层 parent 设为 `MainWindow`
- 动画结束 `deleteLater()`
- 断线或返回首页时，如果浮层存在就 `close()`

## 结论

最简单可行方案是：做一个临时覆盖层，只在 `GAME_START` / `clientConnected` 后播放本地动画，不改网络协议，不改棋盘绘制，不追求两端严格同步。

这个方案对现有代码影响最小，也最容易调试。
