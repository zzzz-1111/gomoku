# 模块接口文档

本文档用于约定 `gomoku` 项目的模块边界、核心类职责、公开接口和模块依赖。

说明：

- 这是接口约定文档，不是实现说明
- 当前项目仍处于骨架阶段，接口以现有头文件为准
- 后续实现应尽量保持这些接口稳定，避免 UI、核心逻辑、AI、网络和数据层互相缠绕

## 1. 模块总览

```text
src/common
  ├── 共享常量
  └── 共享类型

src/ui
  ├── 主窗口
  └── 棋盘控件

src/core
  ├── 规则判断
  └── 对局控制

src/ai
  ├── 局面评分
  └── AI 落子

src/network
  ├── 通信协议
  ├── 客户端通信
  ├── 服务端
  └── 房间状态

src/record
  └── 对局记录与回放

src/data
  ├── SQLite 管理
  └── 数据结构
```

## 2. `src/common`

这一层是所有模块共享的基础定义。

### 2.1 `config.h`

职责：

- 提供全局固定配置值
- 避免棋盘大小、端口号、回放间隔等常量散落到各处

公开内容：

- `gomoku_config::kBoardSize`
- `gomoku_config::kDefaultCellSize`
- `gomoku_config::kDefaultMargin`
- `gomoku_config::kDefaultPort`
- `gomoku_config::kMaxChatLength`
- `gomoku_config::kDefaultReplayDelayMs`

约定：

- 这些值作为默认值使用
- 真正运行时可由设置页覆盖

### 2.2 `gomoku_types.h`

职责：

- 定义跨模块共享枚举和结构体
- 统一棋子颜色、模式、难度、落子信息等类型

公开类型：

- `PieceColor`
- `GameMode`
- `PlayerSide`
- `AIDifficulty`
- `BoardPosition`
- `MoveInfo`
- `GameSettings`

字段说明：

- `BoardPosition::x / y`：棋盘坐标
- `MoveInfo::position`：本步落点
- `MoveInfo::color`：落子颜色
- `MoveInfo::stepNumber`：步数
- `MoveInfo::score`：局面评分
- `GameSettings`：界面设置、AI 难度和棋盘设置的统一载体

约定：

- `PieceColor::Empty` 表示空位
- `GameMode` 用于区分本地双人、人机、联机和回放

## 3. `src/ui`

这一层只负责显示和交互，不直接写复杂规则。

### 3.1 `MainWindow`

文件：

- `src/ui/mainwindow.h`
- `src/ui/mainwindow.cpp`

职责：

- 承载主界面
- 以后放模式入口、信息面板和页面切换逻辑
- 当前阶段用于启动后可见的主窗口壳

当前接口：

- `MainWindow(QWidget *parent = nullptr)`
- `~MainWindow() override`

约定：

- 主窗口不直接负责落子规则
- 主窗口通过信号/槽或控制器转发用户操作

### 3.2 `ChessBoardWidget`

文件：

- `src/ui/chessboard_widget.h`

职责：

- 自定义绘制 15x15 棋盘
- 显示黑白棋子
- 显示最后一步、推荐点、悬停点
- 接收鼠标点击并转换为棋盘坐标

当前接口：

- `ChessBoardWidget(QWidget *parent = nullptr)`
- `void setBoardSize(int size)`
- `void setBoard(const QVector<QVector<PieceColor>> &board)`
- `void setLastMove(const BoardPosition &position)`
- `void setSuggestedMove(const BoardPosition &position)`
- `void clearMarkers()`
- `QSize minimumSizeHint() const override`
- `QSize sizeHint() const override`

信号：

- `cellClicked(int x, int y)`
- `cellHovered(int x, int y)`
- `hoverLeftBoard()`

重写事件：

- `paintEvent(QPaintEvent *event)`
- `mousePressEvent(QMouseEvent *event)`
- `mouseMoveEvent(QMouseEvent *event)`
- `leaveEvent(QEvent *event)`
- `resizeEvent(QResizeEvent *event)`

约定：

- 这个控件只负责坐标显示和输入，不做最终胜负判定
- 落子合法性应交给 `GameController` 或 `GameRule`

## 4. `src/core`

这一层是五子棋逻辑中枢。

### 4.1 `GameRule`

文件：

- `src/core/game_rule.h`

职责：

- 提供静态规则判断函数
- 判断边界、空位、合法落子、五连、满盘和胜者

当前接口：

- `static bool isInsideBoard(int x, int y, int boardSize = 15)`
- `static bool isCellEmpty(const QVector<QVector<PieceColor>> &board, int x, int y)`
- `static bool isLegalMove(const QVector<QVector<PieceColor>> &board, int x, int y)`
- `static bool hasFiveInRow(const QVector<QVector<PieceColor>> &board, int x, int y)`
- `static bool isBoardFull(const QVector<QVector<PieceColor>> &board)`
- `static PieceColor winnerAt(const QVector<QVector<PieceColor>> &board, int x, int y)`

约定：

- 规则函数不依赖 UI
- 规则函数只接收棋盘状态，不持有状态
- `winnerAt` 仅根据最后一步或指定点判断胜者

### 4.2 `GameController`

文件：

- `src/core/game_controller.h`

职责：

- 管理当前棋盘状态
- 管理当前玩家、模式、回合、历史记录和结束状态
- 统一处理落子、悔棋、重开和通知 UI 更新

当前接口：

- `GameController(QObject *parent = nullptr)`
- `void resetGame()`
- `void setGameMode(GameMode mode)`
- `GameMode gameMode() const`
- `void setBoardSize(int size)`
- `int boardSize() const`
- `void setHumanSide(PlayerSide side)`
- `PlayerSide humanSide() const`
- `void setCurrentPlayer(PieceColor color)`
- `PieceColor currentPlayer() const`
- `const QVector<QVector<PieceColor>> &board() const`
- `QVector<MoveInfo> moveHistory() const`
- `bool canPlaceAt(int x, int y) const`
- `bool placeStone(int x, int y)`
- `bool undoLastMove()`
- `bool canUndo() const`

信号：

- `boardChanged()`
- `currentPlayerChanged(PieceColor color)`
- `moveApplied(const MoveInfo &move)`
- `gameFinished(PieceColor winner)`
- `drawDetected()`
- `tipRequested(const QString &text)`

内部职责：

- `switchTurn()`
- `appendMove(int x, int y, PieceColor color)`
- `checkGameOver(int x, int y)`

约定：

- UI 不直接修改棋盘数组
- 所有落子都应先经过 `GameController`
- 联机模式下，网络结果也应最终回到 `GameController` 统一落盘

## 5. `src/ai`

这一层负责局面评估和 AI 决策。

### 5.1 `BoardEvaluator`

文件：

- `src/ai/board_evaluator.h`

职责：

- 对棋盘局面进行打分
- 对候选点进行局部评分
- 为不同 AI 难度提供基础分值

当前接口：

- `int evaluateBoard(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const`
- `int evaluatePoint(const QVector<QVector<PieceColor>> &board, int x, int y, PieceColor aiColor) const`

私有辅助：

- `int scoreLine(...) const`

约定：

- 评分逻辑只关注棋盘，不关心 UI
- 评分结果主要用于 AI 难度区分和候选点筛选

### 5.2 `AIEngine`

文件：

- `src/ai/ai_engine.h`

职责：

- 根据当前棋盘选择落点
- 管理 AI 难度、风格和搜索深度
- 在简单场景下给出即时落点，在复杂场景下进行搜索

当前接口：

- `AIEngine()`
- `void setDifficulty(AIDifficulty difficulty)`
- `void setSearchDepth(int depth)`
- `AIDifficulty difficulty() const`
- `int searchDepth() const`
- `MoveInfo chooseMove(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const`

私有辅助：

- `QVector<BoardPosition> generateCandidates(const QVector<QVector<PieceColor>> &board) const`
- `int searchBestScore(const QVector<QVector<PieceColor>> &board, PieceColor aiColor, int depth) const`

约定：

- AI 不依赖云端接口
- AI 的输出是一个明确的落点和评分
- UI 层可以把推荐点显示出来，但不能代替控制器直接写盘

## 6. `src/network`

这一层负责局域网对战通信。

### 6.1 `protocol.h`

职责：

- 定义消息类型
- 定义网络消息载体
- 提供消息类型与字符串的转换接口

当前接口：

- `enum class MessageType`
- `struct NetworkMessage`
- `QString messageTypeToString(MessageType type)`
- `MessageType messageTypeFromString(const QString &type)`

约定：

- 所有网络消息都应有类型字段
- 消息载荷使用 `QJsonObject` 承载
- 后续可以在此基础上扩展版本号和校验字段

### 6.2 `NetworkManager`

职责：

- 作为客户端通信入口
- 负责连接服务器、断开连接、收发消息
- 向 UI 或控制层发出网络事件

当前接口：

- `NetworkManager(QObject *parent = nullptr)`
- `void setPlayerName(const QString &name)`
- `QString playerName() const`
- `bool connectToHost(const QString &host, quint16 port)`
- `void disconnectFromHost()`
- `bool isConnected() const`
- `void sendMessage(const NetworkMessage &message)`

信号：

- `connectedToServer()`
- `disconnectedFromServer()`
- `messageReceived(const NetworkMessage &message)`
- `connectionError(const QString &errorText)`

私有槽：

- `handleConnected()`
- `handleDisconnected()`
- `handleReadyRead()`
- `handleSocketError()`

约定：

- 只负责连接和消息，不负责对局规则
- 网络解析失败时应向上层发错误，不直接崩溃

### 6.3 `GameRoom`

职责：

- 表示一个联机房间的状态
- 保存房间号、房主、客人、棋盘大小和模式

当前接口：

- `GameRoom()`
- `void setRoomId(const QString &roomId)`
- `QString roomId() const`
- `void setHostName(const QString &name)`
- `QString hostName() const`
- `void setGuestName(const QString &name)`
- `QString guestName() const`
- `void setBoardSize(int size)`
- `int boardSize() const`
- `void setCurrentMode(GameMode mode)`
- `GameMode currentMode() const`
- `void setCreatedAt(const QDateTime &time)`
- `QDateTime createdAt() const`
- `bool isReady() const`
- `void reset()`

约定：

- 房间只是状态容器，不承担网络收发
- 房间状态应由服务器和控制层共同维护

### 6.4 `GameServer`

职责：

- 在本地启动监听
- 接收客户端连接
- 维护服务端侧的连接集合

当前接口：

- `GameServer(QObject *parent = nullptr)`
- `bool startListening(quint16 port)`
- `void stopListening()`
- `bool isListening() const`
- `quint16 listeningPort() const`
- `QString roomCode() const`
- `int connectedClientCount() const`

信号：

- `serverStarted(quint16 port)`
- `serverStopped()`
- `clientConnected(const QString &playerName)`
- `clientDisconnected(const QString &playerName)`
- `serverError(const QString &errorText)`

私有槽：

- `handleNewConnection()`
- `handleClientDisconnected()`

约定：

- 服务器侧只负责对局同步和连接管理
- 具体消息语义仍由 `protocol.h` 约束

## 7. `src/record`

这一层负责棋局持久化和回放控制。

### `RecordManager`

文件：

- `src/record/record_manager.h`

职责：

- 保存完整对局
- 读取历史对局
- 加载指定对局的落子列表

当前接口：

- `RecordManager(QObject *parent = nullptr)`
- `bool saveGame(const GameRecord &game, const QVector<MoveRecord> &moves)`
- `QVector<GameRecord> recentGames(int limit = 20) const`
- `QVector<MoveRecord> movesForGame(int gameId) const`
- `bool loadGame(int gameId)`

信号：

- `recordSaved(int gameId)`
- `recordLoaded(int gameId)`

约定：

- 记录层只处理业务数据，不直接画回放界面
- 回放播放速度、暂停等 UI 行为应交给界面层

## 8. `src/data`

这一层负责 SQLite 和数据模型。

### 8.1 `models.h`

职责：

- 定义数据库记录对应的数据结构

当前结构：

- `UserRecord`
- `GameRecord`
- `MoveRecord`

字段约定：

- `UserRecord`：玩家统计信息
- `GameRecord`：单局对局信息
- `MoveRecord`：单步落子信息

### 8.2 `DatabaseManager`

职责：

- 统一打开和关闭数据库
- 确保数据库 schema 存在
- 对外提供 `QSqlDatabase`

当前接口：

- `static DatabaseManager &instance()`
- `bool open(const QString &filePath)`
- `void close()`
- `bool isOpen() const`
- `bool ensureSchema()`
- `QSqlDatabase database() const`

约定：

- 数据库管理建议全局单例
- schema 创建逻辑应幂等
- 所有业务 SQL 最好通过这一层拿到数据库句柄

## 9. 模块依赖关系

### 9.1 允许的依赖方向

- `ui` 可以依赖 `core`、`ai`、`network`、`record`
- `core` 可以依赖 `common`
- `ai` 可以依赖 `core` 的棋盘数据类型和 `common`
- `network` 可以依赖 `common` 和部分 `core` 数据结构
- `record` 可以依赖 `data` 和 `common`
- `data` 可以依赖 `common`
- `common` 不依赖其他业务模块

### 9.2 不建议的依赖方向

- `core` 直接依赖 `ui`
- `ai` 直接操作 UI 控件
- `network` 直接修改界面状态
- `data` 直接处理页面逻辑

## 10. 推荐实现顺序

接口实现建议按以下顺序推进：

1. `src/ui` 先跑起来
2. `src/core` 的落子和胜负判断先闭环
3. `src/ai` 再接入
4. `src/record` 再接历史保存与回放
5. `src/network` 再做联机同步
6. `src/data` 最后统一接数据库

原因很简单：

- 界面先跑起来，调试反馈最快
- 核心规则先稳定，AI 和联机才不会重复返工
- 数据层最后接入，容易一次性把模型和表结构定清楚

## 11. 后续维护约定

- 新增模块时，先补接口文档，再补实现
- 新增公共类型时，优先放到 `src/common`
- 如果接口要改，先判断是否会影响 UI、AI、联机和记录模块
- 只要模块对外接口变化，本文档也要同步更新
