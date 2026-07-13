#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIcon>
#include <QInputDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QLineEdit>
#include <QSettings>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

#include "src/ai/ai_turn_actor.h"
#include "src/common/config.h"
#include "src/core/game_controller.h"
#include "src/core/human_turn_actor.h"
#include "src/network/game_room.h"
#include "src/network/game_server.h"
#include "src/network/network_manager.h"
#include "src/network/protocol.h"
#include "src/ui/chessboard_widget.h"

namespace {

QString colorName(PieceColor color)
{
    switch (color) {
    case PieceColor::Black:
        return QStringLiteral("黑方");
    case PieceColor::White:
        return QStringLiteral("白方");
    case PieceColor::Empty:
        return QStringLiteral("空");
    }
    return QStringLiteral("未知");
}

QString sideName(PlayerSide side)
{
    switch (side) {
    case PlayerSide::Black:
        return QStringLiteral("黑方");
    case PlayerSide::White:
        return QStringLiteral("白方");
    }
    return QStringLiteral("未知");
}

QString difficultyName(AIDifficulty difficulty)
{
    switch (difficulty) {
    case AIDifficulty::Easy:
        return QStringLiteral("简单");
    case AIDifficulty::Normal:
        return QStringLiteral("普通");
    case AIDifficulty::Hard:
        return QStringLiteral("困难");
    }
    return QStringLiteral("未知");
}

QString roomSummaryText(const GameRoom &room)
{
    const QString modeText = room.currentMode() == GameMode::OnlineHost
        ? QStringLiteral("主机")
        : QStringLiteral("客户端");
    return QStringLiteral("%1 | %2 | %3:%4 | %5 | %6")
        .arg(room.roomId().isEmpty() ? QStringLiteral("未命名房间") : room.roomId(),
             room.hostName().isEmpty() ? QStringLiteral("未知主机") : room.hostName(),
             room.hostAddress().isEmpty() ? QStringLiteral("0.0.0.0") : room.hostAddress())
        .arg(room.hostPort())
        .arg(room.boardSize())
        .arg(modeText);
}

QString roomSelectionKey(const GameRoom &room)
{
    if (!room.roomId().isEmpty()) {
        return room.roomId();
    }
    return QStringLiteral("%1:%2").arg(room.hostAddress()).arg(room.hostPort());
}

PieceColor pieceColorForSide(PlayerSide side)
{
    return side == PlayerSide::Black ? PieceColor::Black : PieceColor::White;
}

bool parseHostJoinInput(const QString &text, QString *host, quint16 *port)
{
    if (host == nullptr || port == nullptr) {
        return false;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QString hostPart = trimmed;
    quint16 portValue = gomoku_config::kDefaultPort;
    const int colonIndex = trimmed.lastIndexOf(QLatin1Char(':'));
    if (colonIndex > 0 && colonIndex < trimmed.size() - 1) {
        bool ok = false;
        const int parsedPort = trimmed.mid(colonIndex + 1).toInt(&ok);
        if (!ok || parsedPort <= 0 || parsedPort > 65535) {
            return false;
        }
        hostPart = trimmed.left(colonIndex).trimmed();
        portValue = static_cast<quint16>(parsedPort);
    }

    if (hostPart.isEmpty()) {
        return false;
    }

    *host = hostPart;
    *port = portValue;
    return true;
}

bool chooseHostSideDialog(QWidget *parent, PlayerSide *side)
{
    if (parent == nullptr || side == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("选择主机颜色"));
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *sideCombo = new QComboBox(&dialog);
    sideCombo->addItem(QStringLiteral("黑方"), static_cast<int>(PlayerSide::Black));
    sideCombo->addItem(QStringLiteral("白方"), static_cast<int>(PlayerSide::White));
    sideCombo->setCurrentIndex(*side == PlayerSide::Black ? 0 : 1);
    form->addRow(QStringLiteral("主机执子"), sideCombo);
    layout->addLayout(form);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttonBox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *side = static_cast<PlayerSide>(sideCombo->currentData().toInt());
    return true;
}

QString loadOrPromptLocalAccountName(QWidget *parent)
{
    QSettings settings;
    const QString storedName = settings.value(QStringLiteral("user/accountName")).toString().trimmed();
    if (!storedName.isEmpty()) {
        return storedName;
    }

    bool accepted = false;
    const QString input = QInputDialog::getText(parent,
                                                QStringLiteral("登录"),
                                                QStringLiteral("请输入账号名称"),
                                                QLineEdit::Normal,
                                                QString(),
                                                &accepted);
    const QString accountName = accepted && !input.trimmed().isEmpty()
        ? input.trimmed()
        : QStringLiteral("Player");
    settings.setValue(QStringLiteral("user/accountName"), accountName);
    return accountName;
}

QString playerIconPath(PieceColor color)
{
    switch (color) {
    case PieceColor::Black:
        return QStringLiteral(":/assets/profile/black.png");
    case PieceColor::White:
        return QStringLiteral(":/assets/profile/white.png");
    case PieceColor::Empty:
        return {};
    }
    return {};
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(new GameController(this))
    , networkManager_(new NetworkManager(this))
    , gameServer_(new GameServer(this))
{
    ui->setupUi(this);
    localAccountName_ = loadOrPromptLocalAccountName(this);
    networkManager_->setPlayerName(localAccountName_);

    ui->homePage->setStyleSheet(
        "QWidget#homePage {"
        "border-image: url(:/assets/home/home_hero_bg.png) 0 0 0 0 stretch stretch;"
        "}"
        "QLabel#titleLabel, QLabel#subtitleLabel {"
        "color: #f5f7ef;"
        "}"
        "QPushButton {"
        "background-color: rgba(18, 42, 30, 0.34);"
        "color: #f7fbf4;"
        "border: 1px solid rgba(255, 255, 255, 0.28);"
        "border-radius: 14px;"
        "padding: 10px 18px;"
        "}"
        "QPushButton:hover {"
        "background-color: rgba(18, 42, 30, 0.46);"
        "}"
        "QPushButton:pressed {"
        "background-color: rgba(18, 42, 30, 0.58);"
        "}");

    resize(gomoku_config::kMainWindowWidth, gomoku_config::kMainWindowHeight);
    setMinimumSize(gomoku_config::kMainWindowMinWidth, gomoku_config::kMainWindowMinHeight);
    setWindowTitle(QStringLiteral("墨弈"));
    setWindowIcon(QIcon(":/assets/app/app_icon.png"));

    if (ui->boardPlaceholder->layout() == nullptr) {
        auto *boardLayout = new QVBoxLayout(ui->boardPlaceholder);
        boardLayout->setContentsMargins(0, 0, 0, 0);
        boardLayout->setSpacing(0);
    }

    boardWidget_ = new ChessBoardWidget(ui->boardPlaceholder);
    boardWidget_->setBoardSize(controller_->boardSize());
    ui->boardPlaceholder->layout()->addWidget(boardWidget_);

    connect(boardWidget_, &ChessBoardWidget::cellClicked, this, [this](int x, int y) {
        if (currentMode_ == GameMode::OnlineHost) {
            // 主机落子的硬门槛：客户端必须已经发送 LOGIN、服务器已经将其标记为 ready，
            // 并且主机端已经正式进入对局。任何一个条件不满足都不能调用 placeStone。
            if (!hostMayPlaceStone()) {
                updateBoardMoveInput();
                statusBar()->showMessage(QStringLiteral("请等待客户端加入后再落子"), 2200);
                return;
            }
        } else if (currentMode_ == GameMode::OnlineClient) {
            if (!onlineGameStarted_ || !networkManager_->isConnected()) {
                setOnlineGameActive(false);
                statusBar()->showMessage(QStringLiteral("对局尚未开始或连接已断开"), 2200);
                return;
            }
        }

        if (currentMode_ == GameMode::OnlineHost || currentMode_ == GameMode::OnlineClient) {
            const PieceColor localColor = pieceColorForSide(humanSide_);
            if (controller_->currentPlayer() != localColor) {
                statusBar()->showMessage(QStringLiteral("现在还不是你的回合"), 1500);
                return;
            }
        }

        if (!controller_->placeStone(x, y)) {
            statusBar()->showMessage(QStringLiteral("非法落子"), 2000);
        }
    });

    connect(boardWidget_, &ChessBoardWidget::moveInputRejected, this, [this]() {
        if (currentMode_ == GameMode::OnlineHost && !hostMayPlaceStone()) {
            statusBar()->showMessage(QStringLiteral("客户端尚未加入，主机不能落子"), 2200);
            return;
        }
        if (currentMode_ == GameMode::OnlineClient && !onlineGameStarted_) {
            statusBar()->showMessage(QStringLiteral("等待主机开始对局"), 2200);
        }
    });

    connect(boardWidget_, &ChessBoardWidget::cellHovered, this, [this](int x, int y) {
        statusBar()->showMessage(QStringLiteral("悬停位置：%1, %2").arg(x + 1).arg(y + 1));
    });

    connect(boardWidget_, &ChessBoardWidget::hoverLeftBoard, this, [this]() {
        statusBar()->clearMessage();
    });

    connect(controller_, &GameController::boardChanged, this, [this]() {
        syncBoardFromController();
    });

    connect(controller_, &GameController::currentPlayerChanged, this, [this](PieceColor) {
        refreshGameInfo();
    });

    connect(controller_, &GameController::moveApplied, this, [this](const MoveInfo &move) {
        boardWidget_->setLastMove(move.position);
        refreshGameInfo();
        statusBar()->showMessage(QStringLiteral("%1已落子").arg(colorName(move.color)), 2000);
    });

    connect(controller_, &GameController::moveApplied, this, [this](const MoveInfo &move) {
        if (applyingNetworkMove_) {
            return;
        }

        if (currentMode_ != GameMode::OnlineHost && currentMode_ != GameMode::OnlineClient) {
            return;
        }

        // 双方尚未完成开局握手时，绝不发送落子消息。
        // 这也是防止“主机提前落子、后来加入的客户端永远收不到”的第二层保护。
        if (currentMode_ == GameMode::OnlineHost && !hostMayPlaceStone()) {
            // 理论上输入锁已经阻止了本地落子；这里再做一次发送层防护。
            return;
        }
        if (currentMode_ == GameMode::OnlineClient
            && (!onlineGameStarted_ || !networkManager_->isConnected())) {
            return;
        }

        NetworkMessage message;
        message.type = MessageType::Move;
        message.payload.insert(QStringLiteral("x"), move.position.x);
        message.payload.insert(QStringLiteral("y"), move.position.y);
        message.payload.insert(QStringLiteral("color"), static_cast<int>(move.color));
        message.payload.insert(QStringLiteral("stepNumber"), move.stepNumber);

        if (currentMode_ == GameMode::OnlineHost) {
            gameServer_->broadcastMessage(message);
        } else if (networkManager_ != nullptr) {
            networkManager_->sendMessage(message);
        }
    });

    connect(controller_, &GameController::gameFinished, this, [this](PieceColor winner) {
        showGameOverPrompt(QStringLiteral("对局结束"),
                           QStringLiteral("%1获胜").arg(colorName(winner)));
    });

    connect(controller_, &GameController::drawDetected, this, [this]() {
        showGameOverPrompt(QStringLiteral("对局结束"), QStringLiteral("平局"));
    });

    setupHomePage();
    setupGamePage();
    setupNetworkPage();

    connect(networkManager_, &NetworkManager::roomsChanged, this, &MainWindow::refreshDiscoveredRooms);
    connect(networkManager_, &NetworkManager::discoveryStarted, this, [this](quint16 port) {
        ui->networkStatusLabel->setText(QStringLiteral("状态：正在扫描端口 %1").arg(port));
        statusBar()->showMessage(QStringLiteral("已开始联机扫描"), 2000);
    });
    connect(networkManager_, &NetworkManager::discoveryStopped, this, [this]() {
        ui->networkStatusLabel->setText(QStringLiteral("状态：扫描已停止"));
    });
    connect(networkManager_, &NetworkManager::connectedToServer, this, [this]() {
        // 此时只是 TCP 建链成功，必须继续等待服务器的 GAME_START。
        setOnlineGameActive(false);
        ui->networkStatusLabel->setText(QStringLiteral("状态：已连接，等待主机开始对局"));
        statusBar()->showMessage(QStringLiteral("已连接到主机，正在等待开局"), 2000);
    });
    connect(networkManager_, &NetworkManager::disconnectedFromServer, this, [this]() {
        setOnlineGameActive(false);
        ui->networkStatusLabel->setText(QStringLiteral("状态：已断开连接"));
        statusBar()->showMessage(QStringLiteral("已与主机断开，对局已暂停"), 3000);
    });
    connect(networkManager_, &NetworkManager::connectionError, this, [this](const QString &errorText) {
        setOnlineGameActive(false);
        ui->networkStatusLabel->setText(QStringLiteral("状态：%1").arg(errorText));
        statusBar()->showMessage(errorText, 4000);
    });

    connect(networkManager_, &NetworkManager::messageReceived, this, [this](const NetworkMessage &message) {
        if (message.type == MessageType::GameStart) {
            const PlayerSide localSide = static_cast<PlayerSide>(
                message.payload.value(QStringLiteral("yourSide")).toInt(static_cast<int>(PlayerSide::White)));
            const PieceColor firstPlayer = static_cast<PieceColor>(
                message.payload.value(QStringLiteral("firstPlayer")).toInt(static_cast<int>(PieceColor::Black)));
            const int boardSize = message.payload.value(QStringLiteral("boardSize")).toInt(controller_->boardSize());

            if (boardSize >= 5 && boardSize != controller_->boardSize()) {
                controller_->setBoardSize(boardSize);
                boardWidget_->setBoardSize(boardSize);
            }

            // 只有收到 GAME_START 才把客户端切换到可落子状态。
            setOnlineGameActive(true);
            startGame(GameMode::OnlineClient, localSide);
            controller_->setCurrentPlayer(firstPlayer);
            ui->networkStatusLabel->setText(QStringLiteral("状态：联机对局已开始"));
            statusBar()->showMessage(QStringLiteral("联机对局已开始"), 2000);
            return;
        }

        if (message.type == MessageType::RestartRequest) {
            const bool accepted = message.payload.value(QStringLiteral("accepted")).toBool(false);
            if (accepted) {
                setOnlineGameActive(true);
                controller_->resetGame();
                ui->stackedWidget->setCurrentWidget(ui->gamePage);
                syncBoardFromController();
                refreshGameInfo();
                statusBar()->showMessage(QStringLiteral("Rematch started"), 2000);
            } else {
                setOnlineGameActive(false);
                ui->stackedWidget->setCurrentWidget(ui->networkPage);
                statusBar()->showMessage(QStringLiteral("Host ended the match"), 2500);
            }
            return;
        }

        if (message.type != MessageType::Move
            || currentMode_ != GameMode::OnlineClient
            || !onlineGameStarted_
            || !networkManager_->isConnected()) {
            return;
        }

        const int x = message.payload.value(QStringLiteral("x")).toInt(-1);
        const int y = message.payload.value(QStringLiteral("y")).toInt(-1);
        const int colorValue = message.payload.value(QStringLiteral("color")).toInt(static_cast<int>(PieceColor::Empty));
        const PieceColor color = static_cast<PieceColor>(colorValue);
        if (x < 0 || y < 0 || color != PieceColor::Black
            || controller_->currentPlayer() != PieceColor::Black) {
            return;
        }

        applyingNetworkMove_ = true;
        const bool applied = controller_->placeStone(x, y);
        applyingNetworkMove_ = false;
        if (!applied) {
            statusBar()->showMessage(QStringLiteral("收到无效的主机落子，双方状态可能不同步"), 3000);
        }
    });

    connect(gameServer_, &GameServer::serverStarted, this, [this](quint16 port) {
        // 创建主机后只进入“等待玩家”状态，棋盘保持禁用。
        // 客户端完成 LOGIN 前，主机无论如何点击都不会产生落子。
        prepareOnlineHostWaiting();
        ui->hostInfoLabel->setText(QStringLiteral("主机：运行中，端口 %1 | 账号：%2 | 你是%3").arg(port).arg(localAccountName_, sideName(humanSide_)));
        ui->hostButton->setText(QStringLiteral("停止主机"));
        ui->networkStatusLabel->setText(QStringLiteral("状态：等待客户端加入，主机不能落子"));
        statusBar()->showMessage(QStringLiteral("主机已启动，请等待客户端加入后再落子"), 2500);
        refreshDiscoveredRooms();
    });
    connect(gameServer_, &GameServer::serverStopped, this, [this]() {
        hostPlayerJoined_ = false;
        setOnlineGameActive(false);
        ui->hostInfoLabel->setText(QStringLiteral("账号：%1").arg(localAccountName_));
        ui->hostButton->setText(QStringLiteral("创建主机"));
        ui->networkStatusLabel->setText(QStringLiteral("状态：主机已停止"));
    });
    connect(gameServer_, &GameServer::serverError, this, [this](const QString &errorText) {
        ui->networkStatusLabel->setText(QStringLiteral("状态：%1").arg(errorText));
        statusBar()->showMessage(errorText, 4000);
    });
    connect(gameServer_, &GameServer::roomBroadcasted, this, [this](const GameRoom &) {
        refreshDiscoveredRooms();
    });

    connect(gameServer_, &GameServer::clientConnected, this, [this](const QString &playerName) {
        // 只有收到客户端 LOGIN 后才设置为 true；单纯建立 TCP 连接不算加入。
        hostPlayerJoined_ = true;
        onlineGameStarted_ = true;
        startGame(GameMode::OnlineHost, humanSide_);
        updateBoardMoveInput();
        ui->networkStatusLabel->setText(QStringLiteral("状态：%1 已加入，对局开始").arg(playerName));
        statusBar()->showMessage(QStringLiteral("%1 已加入，双方可以开始落子").arg(playerName), 2500);
    });
    connect(gameServer_, &GameServer::clientDisconnected, this, [this](const QString &playerName) {
        // 对手离开后立即锁住主机输入。
        hostPlayerJoined_ = false;
        prepareOnlineHostWaiting();
        ui->networkStatusLabel->setText(QStringLiteral("状态：%1 已断开，等待重新加入").arg(playerName));
        statusBar()->showMessage(QStringLiteral("对方已断开，已停止落子"), 3000);
    });

    // TCP connected 只表示传输层建立成功。客户端必须等服务器发来 GAME_START，
    // 否则这里初始化一次、收到 GAME_START 后又初始化一次，会清空刚同步的状态。
    connect(gameServer_, &GameServer::serverStarted, this, [this](quint16 port) {
        const GameRoom room = gameServer_->room();
        ui->hostInfoLabel->setText(QStringLiteral("主机：%1:%2，房间码 %3 | 账号：%4 | 你是%5")
                                       .arg(room.hostAddress().isEmpty() ? QStringLiteral("127.0.0.1") : room.hostAddress())
                                       .arg(port)
                                       .arg(room.roomId().isEmpty() ? QStringLiteral("未生成") : room.roomId())
                                       .arg(localAccountName_, sideName(humanSide_)));
    });

    connect(gameServer_, &GameServer::clientMessageReceived, this, [this](QTcpSocket *clientSocket, const NetworkMessage &message) {
        if (message.type != MessageType::Move
            || currentMode_ != GameMode::OnlineHost
            || !hostMayPlaceStone()) {
            return;
        }

        const int x = message.payload.value(QStringLiteral("x")).toInt(-1);
        const int y = message.payload.value(QStringLiteral("y")).toInt(-1);
        const int colorValue = message.payload.value(QStringLiteral("color")).toInt(static_cast<int>(PieceColor::Empty));
        const PieceColor color = static_cast<PieceColor>(colorValue);
        if (x < 0 || y < 0 || color != PieceColor::White
            || controller_->currentPlayer() != PieceColor::White) {
            return;
        }

        applyingNetworkMove_ = true;
        const bool applied = controller_->placeStone(x, y);
        applyingNetworkMove_ = false;

        // 主机是权威状态。只有主机确认落子有效后，才把它转发给其他连接。
        if (applied) {
            gameServer_->broadcastMessage(message, clientSocket);
        }
        showPendingOnlineHostGameOverPrompt();
    });

    configureTurnActors(GameMode::LocalTwoPlayer, PlayerSide::Black, AIDifficulty::Normal);
    controller_->setGameMode(GameMode::LocalTwoPlayer);
    controller_->setHumanSide(PlayerSide::Black);
    controller_->resetGame();

    ui->stackedWidget->setCurrentWidget(ui->homePage);
    refreshGameInfo();
    refreshDiscoveredRooms();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::modeText(GameMode mode) const
{
    switch (mode) {
    case GameMode::LocalTwoPlayer:
        return QStringLiteral("本地对战");
    case GameMode::HumanVsAI:
        return QStringLiteral("人机对战");
    case GameMode::OnlineHost:
        return QStringLiteral("联机主机");
    case GameMode::OnlineClient:
        return QStringLiteral("联机客户端");
    case GameMode::Replay:
        return QStringLiteral("复盘模式");
    }
    return QStringLiteral("未知模式");
}

QString MainWindow::playerText(PieceColor color) const
{
    return colorName(color);
}

void MainWindow::configureTurnActors(GameMode mode, PlayerSide humanSide, AIDifficulty aiDifficulty)
{
    controller_->clearTurnActors();

    if (mode == GameMode::HumanVsAI) {
        auto createAiActor = [aiDifficulty]() {
            auto ai = std::make_unique<AiTurnActor>();
            ai->setDifficulty(aiDifficulty);
            return ai;
        };

        if (humanSide == PlayerSide::Black) {
            controller_->setTurnActor(PieceColor::Black, std::make_unique<HumanTurnActor>());
            controller_->setTurnActor(PieceColor::White, createAiActor());
        } else {
            controller_->setTurnActor(PieceColor::Black, createAiActor());
            controller_->setTurnActor(PieceColor::White, std::make_unique<HumanTurnActor>());
        }
        return;
    }

    controller_->setTurnActor(PieceColor::Black, std::make_unique<HumanTurnActor>());
    controller_->setTurnActor(PieceColor::White, std::make_unique<HumanTurnActor>());
}

void MainWindow::startGame(GameMode mode, PlayerSide humanSide, AIDifficulty aiDifficulty)
{
    currentMode_ = mode;
    humanSide_ = humanSide;
    controller_->setGameMode(mode);
    controller_->setHumanSide(humanSide);
    configureTurnActors(mode, humanSide, aiDifficulty);
    controller_->resetGame();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);

    const bool onlineMode = mode == GameMode::OnlineHost || mode == GameMode::OnlineClient;
    if (!onlineMode) {
        onlineGameStarted_ = false;
        hostPlayerJoined_ = false;
    }
    updateBoardMoveInput();
    if (onlineMode) {
        refreshOnlineIdentityLabels();
    }

    syncBoardFromController();
    refreshGameInfo();
    statusBar()->showMessage(QStringLiteral("已进入%1").arg(modeText(mode)), 2000);
}

void MainWindow::prepareOnlineHostWaiting()
{
    hostPlayerJoined_ = false;
    onlineGameStarted_ = false;
    startGame(GameMode::OnlineHost, humanSide_);
    updateBoardMoveInput();
    refreshOnlineIdentityLabels();
    ui->statusLabel->setText(QStringLiteral("状态：等待客户端加入，主机暂时不能落子"));
    statusBar()->showMessage(QStringLiteral("主机已创建，请等待客户端加入"), 2500);
}

void MainWindow::setOnlineGameActive(bool active)
{
    onlineGameStarted_ = active;
    updateBoardMoveInput();
}

bool MainWindow::hostMayPlaceStone() const
{
    return currentMode_ == GameMode::OnlineHost
        && hostPlayerJoined_
        && onlineGameStarted_
        && gameServer_ != nullptr
        && gameServer_->hasReadyClient();
}

void MainWindow::updateBoardMoveInput()
{
    if (boardWidget_ == nullptr) {
        return;
    }

    bool allowInput = true;
    if (currentMode_ == GameMode::OnlineHost) {
        allowInput = hostMayPlaceStone();
    } else if (currentMode_ == GameMode::OnlineClient) {
        allowInput = onlineGameStarted_
            && networkManager_ != nullptr
            && networkManager_->isConnected();
    }

    boardWidget_->setMoveInputEnabled(allowInput);
}

void MainWindow::setupHomePage()
{
    ui->startGameButton->setText(QStringLiteral("本地对战"));
    ui->historyButton->setText(QStringLiteral("人机对战"));
    ui->settingsButton->setText(QStringLiteral("墨弈联机"));
    ui->exitButton->setText(QStringLiteral("退出系统"));

    connect(ui->startGameButton, &QPushButton::clicked, this, [this]() {
        startGame(GameMode::LocalTwoPlayer, PlayerSide::Black);
    });

    connect(ui->historyButton, &QPushButton::clicked, this, [this]() {
        PlayerSide humanSide = PlayerSide::Black;
        AIDifficulty aiDifficulty = AIDifficulty::Normal;
        if (!showAiSettingsDialog(&humanSide, &aiDifficulty)) {
            return;
        }
        startGame(GameMode::HumanVsAI, humanSide, aiDifficulty);
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        if (!networkManager_->isDiscovering()) {
            networkManager_->clearDiscoveredRooms();
            networkManager_->startDiscovery();
        }
        refreshDiscoveredRooms();
        ui->networkStatusLabel->setText(QStringLiteral("状态：准备扫描"));
        ui->stackedWidget->setCurrentWidget(ui->networkPage);
    });

    connect(ui->exitButton, &QPushButton::clicked, this, [this]() {
        close();
    });
}

void MainWindow::setupGamePage()
{
    connect(ui->backButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->homePage);
        statusBar()->showMessage(QStringLiteral("返回首页"), 2000);
    });
}

void MainWindow::setupNetworkPage()
{
    ui->roomListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(ui->networkBackButton, &QPushButton::clicked, this, [this]() {
        networkManager_->stopDiscovery();
        ui->stackedWidget->setCurrentWidget(ui->homePage);
        statusBar()->showMessage(QStringLiteral("返回首页"), 2000);
    });

    connect(ui->discoverButton, &QPushButton::clicked, this, [this]() {
        if (networkManager_->isDiscovering()) {
            networkManager_->stopDiscovery();
        }
        networkManager_->clearDiscoveredRooms();
        if (networkManager_->startDiscovery()) {
            ui->networkStatusLabel->setText(QStringLiteral("状态：扫描房间中"));
            refreshDiscoveredRooms();
        }
    });

    connect(ui->refreshRoomsButton, &QPushButton::clicked, this, [this]() {
        refreshDiscoveredRooms();
    });

    connect(ui->hostButton, &QPushButton::clicked, this, [this]() {
        if (gameServer_->isListening()) {
            gameServer_->stopListening();
            return;
        }

        PlayerSide hostSide = humanSide_;
        if (!chooseHostSideDialog(this, &hostSide)) {
            return;
        }
        humanSide_ = hostSide;
        gameServer_->setHostSide(hostSide);

        gameServer_->setHostName(QStringLiteral("本地主机"));
        gameServer_->setBoardSize(controller_->boardSize());
        gameServer_->setCurrentMode(GameMode::OnlineHost);
        gameServer_->setDiscoveryPort(gomoku_config::kDefaultDiscoveryPort);
        if (!gameServer_->startListening(gomoku_config::kDefaultPort)) {
            ui->hostInfoLabel->setText(QStringLiteral("主机：启动失败"));
        }
    });

    connect(ui->joinRoomButton, &QPushButton::clicked, this, [this]() {
        const QList<QListWidgetItem *> items = ui->roomListWidget->selectedItems();
        if (items.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("请先选择一个房间"), 2000);
            return;
        }

        const QString roomId = items.first()->data(Qt::UserRole).toString();
        auto rooms = networkManager_->discoveredRooms();
        if (gameServer_ != nullptr && gameServer_->isListening()) {
            const GameRoom localRoom = gameServer_->room();
            const QString localRoomKey = roomSelectionKey(localRoom);
            bool alreadyListed = false;
            for (const GameRoom &room : rooms) {
                if (roomSelectionKey(room) == localRoomKey) {
                    alreadyListed = true;
                    break;
                }
            }
            if (!alreadyListed && localRoom.isReady()) {
                rooms.prepend(localRoom);
            }
        }

        for (const GameRoom &room : rooms) {
            if (roomSelectionKey(room) != roomId) {
                continue;
            }

            if (gameServer_ != nullptr
                && gameServer_->isListening()
                && roomSelectionKey(room) == roomSelectionKey(gameServer_->room())) {
                ui->stackedWidget->setCurrentWidget(ui->gamePage);
                updateBoardMoveInput();
                syncBoardFromController();
                refreshGameInfo();
                statusBar()->showMessage(QStringLiteral("Returned to host game"), 2000);
                return;
            }

            setOnlineGameActive(false);
            if (networkManager_->connectToRoom(room)) {
                ui->networkStatusLabel->setText(QStringLiteral("状态：正在连接 %1").arg(roomSummaryText(room)));
            }
            return;
        }

        statusBar()->showMessage(QStringLiteral("房间已失效，请刷新后重试"), 2000);
    });

    connect(ui->directJoinButton, &QPushButton::clicked, this, [this]() {
        bool accepted = false;
        const QString input = QInputDialog::getText(this,
                                                    QStringLiteral("通过IP加入"),
                                                    QStringLiteral("请输入主机地址，格式可为 192.168.1.8 或 192.168.1.8:7777"),
                                                    QLineEdit::Normal,
                                                    QString(),
                                                    &accepted);
        if (!accepted) {
            return;
        }

        QString host;
        quint16 port = gomoku_config::kDefaultPort;
        if (!parseHostJoinInput(input, &host, &port)) {
            QMessageBox::warning(this,
                                 QStringLiteral("输入无效"),
                                 QStringLiteral("请输入有效的主机IP，或使用“IP:端口”的格式"));
            return;
        }

        setOnlineGameActive(false);
        if (networkManager_->connectToHost(host, port)) {
            ui->networkStatusLabel->setText(QStringLiteral("状态：正在连接 %1:%2").arg(host).arg(port));
        }
    });

    connect(ui->roomListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        ui->joinRoomButton->click();
    });

    ui->networkStatusLabel->setText(QStringLiteral("状态：等待"));
    ui->hostInfoLabel->setText(QStringLiteral("账号：%1").arg(localAccountName_));
}

bool MainWindow::showAiSettingsDialog(PlayerSide *humanSide, AIDifficulty *aiDifficulty)
{
    if (humanSide == nullptr || aiDifficulty == nullptr) {
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("人机设置"));
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *sideCombo = new QComboBox(&dialog);
    sideCombo->addItem(QStringLiteral("黑方"), static_cast<int>(PlayerSide::Black));
    sideCombo->addItem(QStringLiteral("白方"), static_cast<int>(PlayerSide::White));
    sideCombo->setCurrentIndex(0);

    auto *difficultyCombo = new QComboBox(&dialog);
    difficultyCombo->addItem(QStringLiteral("简单"), static_cast<int>(AIDifficulty::Easy));
    difficultyCombo->addItem(QStringLiteral("普通"), static_cast<int>(AIDifficulty::Normal));
    difficultyCombo->addItem(QStringLiteral("困难"), static_cast<int>(AIDifficulty::Hard));
    difficultyCombo->setCurrentIndex(1);

    form->addRow(QStringLiteral("玩家方"), sideCombo);
    form->addRow(QStringLiteral("对手难度"), difficultyCombo);
    layout->addLayout(form);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *humanSide = static_cast<PlayerSide>(sideCombo->currentData().toInt());
    *aiDifficulty = static_cast<AIDifficulty>(difficultyCombo->currentData().toInt());
    statusBar()->showMessage(QStringLiteral("已选择%1，难度%2").arg(sideName(*humanSide), difficultyName(*aiDifficulty)), 2000);
    return true;
}

void MainWindow::refreshGameInfo()
{
    if (controller_ == nullptr) {
        return;
    }

    const PieceColor currentColor = controller_->currentPlayer();
    const bool onlineMode = controller_->gameMode() == GameMode::OnlineHost
        || controller_->gameMode() == GameMode::OnlineClient;
    if (onlineMode) {
        ui->modeLabel->setText(QStringLiteral("当前模式：%1 | 账号：%2 | 你是%3")
                                   .arg(modeText(controller_->gameMode()), localAccountName_, sideName(humanSide_)));
    } else {
        ui->modeLabel->setText(QStringLiteral("当前模式：%1").arg(modeText(controller_->gameMode())));
    }
    ui->playerLabel->setText(QStringLiteral("当前玩家：%1").arg(playerText(currentColor)));
    ui->stepLabel->setText(QStringLiteral("当前步数：%1").arg(controller_->moveHistory().size()));
    ui->statusLabel->setText(QStringLiteral("状态：等待落子"));

    const QString iconPath = playerIconPath(currentColor);
    if (!iconPath.isEmpty()) {
        QPixmap icon(iconPath);
        ui->playerIconLabel->setPixmap(icon.scaled(ui->playerIconLabel->size(),
                                                   Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
    } else {
        ui->playerIconLabel->clear();
    }
}

void MainWindow::refreshOnlineIdentityLabels()
{
    const QString sideText = sideName(humanSide_);
    if (currentMode_ == GameMode::OnlineHost) {
        ui->hostInfoLabel->setText(QStringLiteral("主机：运行中 | 账号：%1 | 你是%2").arg(localAccountName_, sideText));
        ui->networkStatusLabel->setText(QStringLiteral("状态：等待客户端加入 | 账号：%1 | 你是%2").arg(localAccountName_, sideText));
    } else if (currentMode_ == GameMode::OnlineClient) {
        ui->hostInfoLabel->setText(QStringLiteral("账号：%1 | 你是%2").arg(localAccountName_, sideText));
        ui->networkStatusLabel->setText(QStringLiteral("状态：联机对局已开始 | 账号：%1 | 你是%2").arg(localAccountName_, sideText));
    } else {
        ui->hostInfoLabel->setText(QStringLiteral("账号：%1").arg(localAccountName_));
    }
}

void MainWindow::refreshDiscoveredRooms()
{
    if (ui == nullptr || networkManager_ == nullptr) {
        return;
    }

    const QString selectedRoomId = ui->roomListWidget->currentItem() != nullptr
        ? ui->roomListWidget->currentItem()->data(Qt::UserRole).toString()
        : QString();

    ui->roomListWidget->clear();
    auto rooms = networkManager_->discoveredRooms();
    if (gameServer_ != nullptr && gameServer_->isListening()) {
        const GameRoom localRoom = gameServer_->room();
        const QString localRoomKey = roomSelectionKey(localRoom);
        bool alreadyListed = false;
        for (const GameRoom &room : rooms) {
            if (roomSelectionKey(room) == localRoomKey) {
                alreadyListed = true;
                break;
            }
        }
        if (!alreadyListed && localRoom.isReady()) {
            rooms.prepend(localRoom);
        }
    }

    for (const GameRoom &room : rooms) {
        auto *item = new QListWidgetItem(roomSummaryText(room), ui->roomListWidget);
        item->setData(Qt::UserRole, roomSelectionKey(room));
        if (!selectedRoomId.isEmpty() && selectedRoomId == roomSelectionKey(room)) {
            item->setSelected(true);
        }
    }

    if (rooms.isEmpty()) {
        ui->networkStatusLabel->setText(QStringLiteral("状态：未发现房间"));
    } else {
        ui->networkStatusLabel->setText(QStringLiteral("状态：发现 %1 个房间").arg(rooms.size()));
    }
}

void MainWindow::syncBoardFromController()
{
    if (boardWidget_ == nullptr || controller_ == nullptr) {
        return;
    }

    boardWidget_->setBoard(controller_->board());
    if (controller_->moveHistory().isEmpty()) {
        boardWidget_->clearMarkers();
    }
    refreshGameInfo();
}

void MainWindow::showPendingOnlineHostGameOverPrompt()
{
    if (!pendingOnlineHostGameOverPrompt_
        || currentMode_ != GameMode::OnlineHost
        || gameServer_ == nullptr) {
        return;
    }

    pendingOnlineHostGameOverPrompt_ = false;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(pendingGameOverTitle_);
    box.setText(pendingGameOverMessage_);
    box.setInformativeText(QStringLiteral("Start another round?"));
    QPushButton *yesButton = box.addButton(QStringLiteral("Yes"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("No"), QMessageBox::RejectRole);
    box.exec();

    const bool restart = box.clickedButton() == yesButton;

    NetworkMessage decision;
    decision.type = MessageType::RestartRequest;
    decision.payload.insert(QStringLiteral("accepted"), restart);
    gameServer_->broadcastMessage(decision);

    if (restart) {
        setOnlineGameActive(true);
        controller_->resetGame();
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
        syncBoardFromController();
        refreshGameInfo();
        statusBar()->showMessage(QStringLiteral("Rematch started"), 2000);
        return;
    }

    setOnlineGameActive(false);
    ui->stackedWidget->setCurrentWidget(ui->networkPage);
    refreshDiscoveredRooms();
    statusBar()->showMessage(QStringLiteral("Returned to network page"), 2500);
}

void MainWindow::showGameOverPrompt(const QString &title, const QString &message)
{
    if (controller_ == nullptr) {
        return;
    }

    ui->statusLabel->setText(QStringLiteral("状态：%1").arg(message));
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, message), 4000);

    if (currentMode_ == GameMode::OnlineClient) {
        return;
    }

    if (currentMode_ == GameMode::OnlineHost) {
        pendingGameOverTitle_ = title;
        pendingGameOverMessage_ = message;
        pendingOnlineHostGameOverPrompt_ = true;
        if (!applyingNetworkMove_) {
            showPendingOnlineHostGameOverPrompt();
        }
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setText(message);
    box.setInformativeText(QStringLiteral("你可以重新开始，或者返回首页。"));
    QPushButton *restartButton = box.addButton(QStringLiteral("重新开始"), QMessageBox::AcceptRole);
    QPushButton *homeButton = box.addButton(QStringLiteral("返回首页"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == restartButton) {
        controller_->resetGame();
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
        syncBoardFromController();
        refreshGameInfo();
        statusBar()->showMessage(QStringLiteral("已重新开始"), 2000);
        return;
    }

    if (box.clickedButton() == homeButton) {
        ui->stackedWidget->setCurrentWidget(ui->homePage);
        statusBar()->showMessage(QStringLiteral("返回首页"), 2000);
    }
}
