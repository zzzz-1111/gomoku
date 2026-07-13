#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIcon>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
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
        if (!controller_->placeStone(x, y)) {
            statusBar()->showMessage(QStringLiteral("非法落子"), 2000);
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
    connect(networkManager_, &NetworkManager::roomDiscovered, this, [this](const GameRoom &) {
        refreshDiscoveredRooms();
    });
    connect(networkManager_, &NetworkManager::discoveryStarted, this, [this](quint16 port) {
        ui->networkStatusLabel->setText(QStringLiteral("状态：正在扫描端口 %1").arg(port));
        statusBar()->showMessage(QStringLiteral("已开始联机扫描"), 2000);
    });
    connect(networkManager_, &NetworkManager::discoveryStopped, this, [this]() {
        ui->networkStatusLabel->setText(QStringLiteral("状态：扫描已停止"));
    });
    connect(networkManager_, &NetworkManager::connectedToServer, this, [this]() {
        ui->networkStatusLabel->setText(QStringLiteral("状态：已连接"));
        statusBar()->showMessage(QStringLiteral("已连接到主机"), 2000);
    });
    connect(networkManager_, &NetworkManager::disconnectedFromServer, this, [this]() {
        ui->networkStatusLabel->setText(QStringLiteral("状态：已断开连接"));
    });
    connect(networkManager_, &NetworkManager::connectionError, this, [this](const QString &errorText) {
        ui->networkStatusLabel->setText(QStringLiteral("状态：%1").arg(errorText));
        statusBar()->showMessage(errorText, 4000);
    });

    connect(gameServer_, &GameServer::serverStarted, this, [this](quint16 port) {
        ui->hostInfoLabel->setText(QStringLiteral("主机：运行中，端口 %1").arg(port));
        ui->hostButton->setText(QStringLiteral("停止主机"));
        ui->networkStatusLabel->setText(QStringLiteral("状态：主机运行中"));
        statusBar()->showMessage(QStringLiteral("联机主机已启动"), 2000);
        refreshDiscoveredRooms();
    });
    connect(gameServer_, &GameServer::serverStopped, this, [this]() {
        ui->hostInfoLabel->setText(QStringLiteral("主机：未运行"));
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
    syncBoardFromController();
    refreshGameInfo();
    statusBar()->showMessage(QStringLiteral("已进入%1").arg(modeText(mode)), 2000);
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
        const auto rooms = networkManager_->discoveredRooms();
        for (const GameRoom &room : rooms) {
            if (room.roomId() != roomId) {
                continue;
            }

            if (networkManager_->connectToRoom(room)) {
                ui->networkStatusLabel->setText(QStringLiteral("状态：正在连接 %1").arg(roomSummaryText(room)));
            }
            return;
        }

        statusBar()->showMessage(QStringLiteral("房间已失效，请刷新后重试"), 2000);
    });

    connect(ui->roomListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        ui->joinRoomButton->click();
    });

    ui->networkStatusLabel->setText(QStringLiteral("状态：等待"));
    ui->hostInfoLabel->setText(QStringLiteral("主机：未运行"));
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
    ui->modeLabel->setText(QStringLiteral("当前模式：%1").arg(modeText(controller_->gameMode())));
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

void MainWindow::refreshDiscoveredRooms()
{
    if (ui == nullptr || networkManager_ == nullptr) {
        return;
    }

    const QString selectedRoomId = ui->roomListWidget->currentItem() != nullptr
        ? ui->roomListWidget->currentItem()->data(Qt::UserRole).toString()
        : QString();

    ui->roomListWidget->clear();
    const auto rooms = networkManager_->discoveredRooms();
    for (const GameRoom &room : rooms) {
        auto *item = new QListWidgetItem(roomSummaryText(room), ui->roomListWidget);
        item->setData(Qt::UserRole, room.roomId());
        if (!selectedRoomId.isEmpty() && selectedRoomId == room.roomId()) {
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

void MainWindow::showGameOverPrompt(const QString &title, const QString &message)
{
    if (controller_ == nullptr) {
        return;
    }

    ui->statusLabel->setText(QStringLiteral("状态：%1").arg(message));
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, message), 4000);

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
