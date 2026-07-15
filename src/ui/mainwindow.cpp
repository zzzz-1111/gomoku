#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QAbstractAnimation>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QFormLayout>
#include <QIcon>
#include <QFileInfo>
#include <QInputDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <memory>

#include "src/ai/ai_turn_actor.h"
#include "src/common/config.h"
#include "src/core/game_controller.h"
#include "src/core/human_turn_actor.h"
#include "src/network/network_manager.h"
#include "src/network/protocol.h"
#include "src/profile/user_profile.h"
#include "src/record/record_manager.h"
#include "src/ui/chessboard_widget.h"
#include "src/ui/history_dialog.h"
#include "src/ui/match_intro_overlay.h"

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
        const int parsedPort = trimmed.sliced(colonIndex + 1).toInt(&ok);
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

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(new GameController(this))
    , recordManager_(new RecordManager(this))
    , networkManager_(new NetworkManager(this))
{
    ui->setupUi(this);
    localAccountName_ = UserProfile::loadOrPromptAccountName(this);
    localAvatarPath_ = UserProfile::resolveLocalAvatarPath();
    networkManager_->setPlayerName(localAccountName_);
    networkManager_->setAvatarData(UserProfile::encodeAvatarForNetwork(localAvatarPath_));

    ui->homePage->setStyleSheet(
        "QWidget#homePage {"
        "border-image: url(:/assets/backgrounds/home_hero_bg.png) 0 0 0 0 stretch stretch;"
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
    setWindowIcon(QIcon(":/assets/icons/app_icon.png"));

    if (ui->boardPlaceholder->layout() == nullptr) {
        auto *boardLayout = new QVBoxLayout(ui->boardPlaceholder);
        boardLayout->setContentsMargins(0, 0, 0, 0);
        boardLayout->setSpacing(0);
    }

    boardWidget_ = new ChessBoardWidget(ui->boardPlaceholder);
    boardWidget_->setBoardSize(controller_->boardSize());
    ui->boardPlaceholder->layout()->addWidget(boardWidget_);

    debugHostButton_ = ui->debugHostButton;
    debugClientButton_ = ui->debugClientButton;

    connect(boardWidget_, &ChessBoardWidget::cellClicked, this, [this](int x, int y) {
        if (currentMode_ == GameMode::OnlineClient) {
            const bool connected = networkManager_ != nullptr && networkManager_->isConnected();
            if (!onlineGameStarted_ || (!onlineDebugMode_ && !connected)) {
                setOnlineGameActive(false);
                statusBar()->showMessage(QStringLiteral("对局尚未开始或连接已断开"), 2200);
                return;
            }
        }

        if (currentMode_ == GameMode::OnlineClient) {
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
        if (currentMode_ == GameMode::OnlineClient && !onlineGameStarted_ && !onlineDebugMode_) {
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

        if (currentMode_ == GameMode::OnlineClient
            && (!onlineGameStarted_
                || (!onlineDebugMode_ && (networkManager_ == nullptr || !networkManager_->isConnected())))) {
            return;
        }

        if (!onlineDebugMode_ && networkManager_ != nullptr) {
            NetworkMessage message;
            message.type = MessageType::Move;
            message.payload.insert(QStringLiteral("x"), move.position.x);
            message.payload.insert(QStringLiteral("y"), move.position.y);
            message.payload.insert(QStringLiteral("color"), static_cast<int>(move.color));
            message.payload.insert(QStringLiteral("stepNumber"), move.stepNumber);
            networkManager_->sendMessage(message);
        }

        if (currentMode_ == GameMode::OnlineClient
            && onlineDebugMode_
            && move.color == pieceColorForSide(humanSide_)
            && controller_ != nullptr
            && !controller_->snapshot().gameOver) {
            scheduleOnlineDebugRemoteMove();
        }
    });

    connect(controller_, &GameController::gameFinished, this, [this](PieceColor winner) {
        saveCurrentGameResult(winner);
        const QString message = QStringLiteral("%1获胜").arg(colorName(winner));
        playWinAnimation(winner, [this, message]() {
            showGameOverPrompt(QStringLiteral("对局结束"), message);
        });
    });

    connect(controller_, &GameController::drawDetected, this, [this]() {
        saveCurrentGameResult(PieceColor::Empty);
        showGameOverPrompt(QStringLiteral("对局结束"), QStringLiteral("平局"));
    });

    if (debugHostButton_ != nullptr && debugClientButton_ != nullptr) {
        connect(debugHostButton_, &QPushButton::clicked, this, [this]() {
            switchOnlineDebugRole(true);
        });
        connect(debugClientButton_, &QPushButton::clicked, this, [this]() {
            switchOnlineDebugRole(false);
        });
    }

    setupHomePage();
    setupGamePage();
    connect(networkManager_, &NetworkManager::connectedToServer, this, [this]() {
        setOnlineGameActive(false);
        statusBar()->showMessage(QStringLiteral("已连接到主机，正在等待开局"), 2000);
    });
    connect(networkManager_, &NetworkManager::disconnectedFromServer, this, [this]() {
        setOnlineGameActive(false);
        onlineDebugMode_ = false;
        onlineDebugIsHost_ = true;
        onlineDebugWaiting_ = false;
        if (matchIntroOverlay_ != nullptr) {
            matchIntroOverlay_->close();
        }
        remoteAccountName_.clear();
        remoteAvatarBytes_.clear();
        statusBar()->showMessage(QStringLiteral("已与主机断开，对局已暂停"), 3000);
        ui->stackedWidget->setCurrentWidget(ui->homePage);
    });
    connect(networkManager_, &NetworkManager::connectionError, this, [this](const QString &errorText) {
        setOnlineGameActive(false);
        onlineDebugMode_ = false;
        onlineDebugIsHost_ = true;
        onlineDebugWaiting_ = false;
        statusBar()->showMessage(errorText, 4000);
        ui->stackedWidget->setCurrentWidget(ui->homePage);
    });

    connect(networkManager_, &NetworkManager::messageReceived, this, [this](const NetworkMessage &message) {
        if (message.type == MessageType::GameStart) {
            const PlayerSide localSide = static_cast<PlayerSide>(
                message.payload.value(QStringLiteral("yourSide")).toInt(static_cast<int>(PlayerSide::White)));
            const PieceColor firstPlayer = static_cast<PieceColor>(
                message.payload.value(QStringLiteral("firstPlayer")).toInt(static_cast<int>(PieceColor::Black)));
            const int boardSize = message.payload.value(QStringLiteral("boardSize")).toInt(controller_->boardSize());
            remoteAccountName_ = message.payload.value(QStringLiteral("hostName")).toString(QStringLiteral("Host")).trimmed();
            if (remoteAccountName_.isEmpty()) {
                remoteAccountName_ = QStringLiteral("Host");
            }
            remoteAvatarBytes_ = QByteArray::fromBase64(
                message.payload.value(QStringLiteral("hostAvatar")).toString().toLatin1());

            if (boardSize >= 5 && boardSize != controller_->boardSize()) {
                controller_->setBoardSize(boardSize);
                boardWidget_->setBoardSize(boardSize);
            }

            setOnlineGameActive(true);
            startGame(GameMode::OnlineClient, localSide);
            controller_->setCurrentPlayer(firstPlayer);
            statusBar()->showMessage(QStringLiteral("联机对局已开始"), 2000);
            playOnlineMatchIntro();
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

    configureTurnActors(GameMode::LocalTwoPlayer, PlayerSide::Black, AIDifficulty::Normal);
    controller_->setGameMode(GameMode::LocalTwoPlayer);
    controller_->setHumanSide(PlayerSide::Black);
    controller_->resetGame();

    ui->stackedWidget->setCurrentWidget(ui->homePage);
    refreshGameInfo();
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
    case GameMode::OnlineClient:
        if (onlineDebugMode_) {
            return onlineDebugIsHost_ ? QStringLiteral("联机调试（模拟主机）")
                                      : QStringLiteral("联机调试（模拟客户端）");
        }
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
    const bool onlineMode = mode == GameMode::OnlineClient;
    if (!onlineMode) {
        beginActiveMatchRecord();
    }
    controller_->resetGame();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
    if (!onlineMode) {
        onlineGameStarted_ = false;
    }
    updateBoardMoveInput();

    syncBoardFromController();
    refreshGameInfo();
    statusBar()->showMessage(QStringLiteral("已进入%1").arg(modeText(mode)), 2000);
}

void MainWindow::setOnlineGameActive(bool active)
{
    onlineGameStarted_ = active;
    updateBoardMoveInput();
}

void MainWindow::updateBoardMoveInput()
{
    if (boardWidget_ == nullptr) {
        return;
    }

    bool allowInput = true;
    if (currentMode_ == GameMode::OnlineClient) {
        allowInput = onlineGameStarted_ && !onlineDebugWaiting_;
        if (!onlineDebugMode_) {
            allowInput = allowInput
                && networkManager_ != nullptr
                && networkManager_->isConnected();
        } else if (controller_ != nullptr) {
            allowInput = allowInput
                && controller_->currentPlayer() == pieceColorForSide(humanSide_);
        }
    }

    boardWidget_->setMoveInputEnabled(allowInput);
}

void MainWindow::beginActiveMatchRecord()
{
    currentGameStartTime_ = QDateTime::currentDateTime();
    currentGameResultRecorded_ = false;
}

void MainWindow::setupHomePage()
{
    ui->startGameButton->setText(QStringLiteral("本地对战"));
    ui->historyButton->setText(QStringLiteral("人机对战"));
    ui->historyRecordButton->setText(QStringLiteral("历史记录"));
    ui->networkButton->setText(QStringLiteral("联机对战"));
    ui->onlineDebugButton->setText(QStringLiteral("联机调试"));
    ui->settingsButton->setText(QStringLiteral("设置"));
    ui->exitButton->setText(QStringLiteral("退出游戏"));

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
        playAiMatchIntro();
    });

    connect(ui->historyRecordButton, &QPushButton::clicked, this, [this]() {
        showHistoryDialog();
    });

    connect(ui->networkButton, &QPushButton::clicked, this, [this]() {
        onlineDebugMode_ = false;
        onlineDebugIsHost_ = true;
        onlineDebugWaiting_ = false;
        bool accepted = false;
        const QString input = QInputDialog::getText(this,
                                                    QStringLiteral("联机对战"),
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
                                 QStringLiteral("请输入有效的主机 IP，或使用“IP:端口”的格式"));
            return;
        }

        setOnlineGameActive(false);
        if (networkManager_->connectToHost(host, port)) {
            statusBar()->showMessage(QStringLiteral("正在连接 %1:%2").arg(host).arg(port), 2000);
        }
    });

    connect(ui->onlineDebugButton, &QPushButton::clicked, this, [this]() {
        if (networkManager_ != nullptr && networkManager_->isConnected()) {
            QMessageBox::warning(this,
                                 QStringLiteral("无法调试"),
                                 QStringLiteral("请先退出当前真实联机，再进入调试模式"));
            return;
        }
        showOnlineDebugDialog();
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        const bool profileChanged = UserProfile::showProfileSettingsDialog(this, &localAccountName_, &localAvatarPath_);
        if (profileChanged) {
            networkManager_->setPlayerName(localAccountName_);
            networkManager_->setAvatarData(UserProfile::encodeAvatarForNetwork(localAvatarPath_));
            refreshGameInfo();
        }
    });

    connect(ui->exitButton, &QPushButton::clicked, this, [this]() {
        close();
    });
}

void MainWindow::setupGamePage()
{
    connect(ui->backButton, &QPushButton::clicked, this, [this]() {
        if (matchIntroOverlay_ != nullptr) {
            matchIntroOverlay_->close();
        }
        onlineDebugMode_ = false;
        onlineDebugIsHost_ = true;
        onlineDebugWaiting_ = false;
        ui->stackedWidget->setCurrentWidget(ui->homePage);
        statusBar()->showMessage(QStringLiteral("返回首页"), 2000);
    });
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
    const bool onlineMode = controller_->gameMode() == GameMode::OnlineClient;
    if (onlineMode) {
        ui->modeLabel->setText(QStringLiteral("当前模式：%1 | 账号：%2 | 你是%3")
                                   .arg(modeText(controller_->gameMode()), localAccountName_, sideName(humanSide_)));
    } else {
        ui->modeLabel->setText(QStringLiteral("当前模式：%1").arg(modeText(controller_->gameMode())));
    }
    const bool currentIsLocal = onlineMode && currentColor == pieceColorForSide(humanSide_);
    const bool currentIsRemote = onlineMode && !currentIsLocal;
    const QString currentName = currentIsLocal
        ? localAccountName_
        : (remoteAccountName_.isEmpty() ? QStringLiteral("对方玩家") : remoteAccountName_);
    if (onlineMode) {
        ui->playerLabel->setText(QStringLiteral("当前玩家：%1 | %2")
                                     .arg(playerText(currentColor), currentName));
    } else {
        ui->playerLabel->setText(QStringLiteral("当前玩家：%1").arg(playerText(currentColor)));
    }
    ui->stepLabel->setText(QStringLiteral("当前步数：%1").arg(controller_->moveHistory().size()));
    ui->statusLabel->setText(QStringLiteral("状态：等待落子"));

    if (currentIsRemote) {
        ui->playerIconLabel->setPixmap(UserProfile::loadAvatarPixmapFromBytes(remoteAvatarBytes_, ui->playerIconLabel->size()));
    } else {
        const QString avatarPath = QFileInfo::exists(localAvatarPath_)
            ? localAvatarPath_
            : UserProfile::defaultAvatarResourcePath();
        ui->playerIconLabel->setPixmap(UserProfile::loadAvatarPixmap(avatarPath, ui->playerIconLabel->size()));
    }

    if (debugHostButton_ != nullptr && debugClientButton_ != nullptr) {
        const bool showDebugControls = onlineDebugMode_ && controller_->gameMode() == GameMode::OnlineClient;
        debugHostButton_->setVisible(showDebugControls);
        debugClientButton_->setVisible(showDebugControls);
        if (showDebugControls) {
            debugHostButton_->setText(onlineDebugIsHost_ ? QStringLiteral("当前：主机") : QStringLiteral("切换主机"));
            debugClientButton_->setText(onlineDebugIsHost_ ? QStringLiteral("切换客户端") : QStringLiteral("当前：客户端"));
            const QString activeStyle = QStringLiteral(
                "background: #1f3a2b; color: white; border-radius: 12px; padding: 8px 14px;");
            const QString inactiveStyle = QStringLiteral(
                "background: rgba(31,58,43,0.08); color: #1f3a2b; border-radius: 12px; padding: 8px 14px;");
            debugHostButton_->setStyleSheet(onlineDebugIsHost_ ? activeStyle : inactiveStyle);
            debugClientButton_->setStyleSheet(onlineDebugIsHost_ ? inactiveStyle : activeStyle);
        }
    }
}

QString MainWindow::displayNameForSide(PlayerSide side) const
{
    switch (currentMode_) {
    case GameMode::HumanVsAI:
        if (side == humanSide_) {
            return localAccountName_.isEmpty() ? QStringLiteral("玩家") : localAccountName_;
        }
        return QStringLiteral("AI玩家");
    case GameMode::OnlineClient:
        if (side == humanSide_) {
            return localAccountName_.isEmpty() ? QStringLiteral("玩家") : localAccountName_;
        }
        return remoteAccountName_.isEmpty() ? QStringLiteral("主机") : remoteAccountName_;
    case GameMode::LocalTwoPlayer:
    case GameMode::Replay:
    default:
        return side == PlayerSide::Black ? QStringLiteral("黑方玩家") : QStringLiteral("白方玩家");
    }
}

void MainWindow::saveCurrentGameResult(PieceColor winner)
{
    if (currentGameResultRecorded_ || recordManager_ == nullptr || controller_ == nullptr) {
        return;
    }

    GameRecord game;
    game.mode = modeText(currentMode_);
    game.blackPlayer = displayNameForSide(PlayerSide::Black);
    game.whitePlayer = displayNameForSide(PlayerSide::White);
    if (winner == PieceColor::Black) {
        game.winner = game.blackPlayer;
    } else if (winner == PieceColor::White) {
        game.winner = game.whitePlayer;
    } else {
        game.winner = QStringLiteral("平局");
    }
    game.totalMoves = controller_->moveHistory().size();
    game.startTime = currentGameStartTime_.isValid() ? currentGameStartTime_ : QDateTime::currentDateTime();
    game.endTime = QDateTime::currentDateTime();

    QVector<MoveRecord> moves;
    moves.reserve(controller_->moveHistory().size());
    for (const MoveInfo &move : controller_->moveHistory()) {
        MoveRecord record;
        record.gameId = -1;
        record.stepNumber = move.stepNumber;
        record.playerColor = static_cast<int>(move.color);
        record.x = move.position.x;
        record.y = move.position.y;
        moves.push_back(record);
    }

    if (recordManager_->saveGame(game, moves)) {
        currentGameResultRecorded_ = true;
    } else if (statusBar() != nullptr) {
        statusBar()->showMessage(QStringLiteral("历史记录保存失败"), 3000);
    }
}

void MainWindow::showHistoryDialog()
{
    HistoryDialog dialog(recordManager_, this);
    dialog.exec();
}

void MainWindow::playMatchIntro(const IntroPlayerInfo &leftPlayer,
                                const IntroPlayerInfo &rightPlayer,
                                std::function<void()> finished)
{
    if (ui == nullptr || boardWidget_ == nullptr) {
        return;
    }

    if (matchIntroOverlay_ != nullptr) {
        matchIntroOverlay_->close();
        matchIntroOverlay_ = nullptr;
    }

    boardWidget_->setMoveInputEnabled(false);
    matchIntroOverlay_ = new MatchIntroOverlay(ui->centralwidget);
    matchIntroOverlay_->setGeometry(ui->centralwidget->rect());
    connect(matchIntroOverlay_, &QObject::destroyed, this, [this]() {
        matchIntroOverlay_ = nullptr;
    });
    matchIntroOverlay_->play(leftPlayer, rightPlayer, [this, finished = std::move(finished)]() mutable {
        updateBoardMoveInput();
        if (finished) {
            finished();
        }
    });
}

void MainWindow::playOnlineMatchIntro(std::function<void()> finished)
{
    const QString avatarPath = QFileInfo::exists(localAvatarPath_)
        ? localAvatarPath_
        : UserProfile::defaultAvatarResourcePath();

    IntroPlayerInfo localPlayer;
    localPlayer.name = localAccountName_.isEmpty() ? QStringLiteral("Player") : localAccountName_;
    localPlayer.sideText = sideName(humanSide_);
    localPlayer.avatar = UserProfile::loadAvatarPixmap(avatarPath, QSize(128, 128));

    const PlayerSide remoteSide = humanSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black;
    IntroPlayerInfo remotePlayer;
    remotePlayer.name = remoteAccountName_.isEmpty() ? QStringLiteral("对方玩家") : remoteAccountName_;
    remotePlayer.sideText = sideName(remoteSide);
    remotePlayer.avatar = UserProfile::loadAvatarPixmapFromBytes(remoteAvatarBytes_, QSize(128, 128));

    const bool localIsBlack = humanSide_ == PlayerSide::Black;
    const IntroPlayerInfo &leftPlayer = localIsBlack ? localPlayer : remotePlayer;
    const IntroPlayerInfo &rightPlayer = localIsBlack ? remotePlayer : localPlayer;

    playMatchIntro(leftPlayer, rightPlayer, std::move(finished));
}

void MainWindow::playAiMatchIntro()
{
    const QString avatarPath = QFileInfo::exists(localAvatarPath_)
        ? localAvatarPath_
        : UserProfile::defaultAvatarResourcePath();

    IntroPlayerInfo humanPlayer;
    humanPlayer.name = localAccountName_.isEmpty() ? QStringLiteral("玩家") : localAccountName_;
    humanPlayer.sideText = sideName(humanSide_);
    humanPlayer.avatar = UserProfile::loadAvatarPixmap(avatarPath, QSize(128, 128));

    const PlayerSide aiSide = humanSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black;
    IntroPlayerInfo aiPlayer;
    aiPlayer.name = QStringLiteral("AI");
    aiPlayer.sideText = sideName(aiSide);
    aiPlayer.avatar = UserProfile::loadAvatarPixmap(UserProfile::aiAvatarResourcePath(), QSize(128, 128));

    const bool humanIsBlack = humanSide_ == PlayerSide::Black;
    const IntroPlayerInfo &leftPlayer = humanIsBlack ? humanPlayer : aiPlayer;
    const IntroPlayerInfo &rightPlayer = humanIsBlack ? aiPlayer : humanPlayer;

    playMatchIntro(leftPlayer, rightPlayer);
}

void MainWindow::playWinAnimation(PieceColor winner, std::function<void()> finished)
{
    if (winner != PieceColor::Black && winner != PieceColor::White) {
        if (finished) {
            finished();
        }
        return;
    }

    QWidget *parent = ui != nullptr ? ui->centralwidget : nullptr;
    if (parent == nullptr) {
        if (finished) {
            finished();
        }
        return;
    }

    const QString resourcePath = winner == PieceColor::Black
        ? QStringLiteral(":/assets/board/black_win.png")
        : QStringLiteral(":/assets/board/white_win.png");
    QPixmap source(resourcePath);
    if (source.isNull()) {
        if (finished) {
            finished();
        }
        return;
    }

    auto *overlay = new QWidget(parent);
    overlay->setAttribute(Qt::WA_StyledBackground);
    overlay->setStyleSheet(QStringLiteral("background: rgba(12, 22, 16, 150);"));
    overlay->setGeometry(parent->rect());
    overlay->raise();
    overlay->show();

    auto *imageLabel = new QLabel(overlay);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setScaledContents(true);
    imageLabel->setStyleSheet(QStringLiteral("background: transparent;"));

    const int maxWidth = qMin(overlay->width() * 7 / 10, 600);
    const int maxHeight = qMin(overlay->height() * 7 / 10, 560);
    QSize targetSize = source.size();
    targetSize.scale(QSize(maxWidth, maxHeight), Qt::KeepAspectRatio);
    imageLabel->setPixmap(source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    const QRect endRect(overlay->width() / 2 - targetSize.width() / 2,
                        overlay->height() / 2 - targetSize.height() / 2,
                        targetSize.width(),
                        targetSize.height());
    const int insetX = qMax(36, targetSize.width() / 8);
    const int insetY = qMax(48, targetSize.height() / 8);
    const QRect startRect = endRect.adjusted(insetX, insetY, -insetX, -insetY);
    imageLabel->setGeometry(startRect);
    imageLabel->raise();
    imageLabel->show();

    auto *opacityEffect = new QGraphicsOpacityEffect(overlay);
    opacityEffect->setOpacity(0.0);
    overlay->setGraphicsEffect(opacityEffect);

    auto *fadeIn = new QPropertyAnimation(opacityEffect, "opacity", overlay);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    auto *scaleIn = new QPropertyAnimation(imageLabel, "geometry", overlay);
    scaleIn->setDuration(520);
    scaleIn->setStartValue(startRect);
    scaleIn->setEndValue(endRect);
    scaleIn->setEasingCurve(QEasingCurve::OutBack);

    auto *intro = new QParallelAnimationGroup(overlay);
    intro->addAnimation(fadeIn);
    intro->addAnimation(scaleIn);

    auto *fadeOut = new QPropertyAnimation(opacityEffect, "opacity", overlay);
    fadeOut->setDuration(260);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    auto *sequence = new QSequentialAnimationGroup(overlay);
    sequence->addAnimation(intro);
    sequence->addPause(1200);
    sequence->addAnimation(fadeOut);

    connect(sequence, &QSequentialAnimationGroup::finished, overlay, [overlay, finished = std::move(finished)]() mutable {
        overlay->hide();
        overlay->deleteLater();
        if (finished) {
            finished();
        }
    });

    sequence->start(QAbstractAnimation::DeleteWhenStopped);
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

    if (ui != nullptr && ui->statusLabel != nullptr) {
        ui->statusLabel->setText(QStringLiteral("状态：%1").arg(message));
    }
    statusBar()->showMessage(QStringLiteral("%1: %2").arg(title, message), 4000);

    if (currentMode_ == GameMode::OnlineClient) {
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
        startGame(currentMode_, humanSide_);
        statusBar()->showMessage(QStringLiteral("已重新开始"), 2000);
        return;
    }

    if (box.clickedButton() == homeButton) {
        ui->stackedWidget->setCurrentWidget(ui->homePage);
        statusBar()->showMessage(QStringLiteral("返回首页"), 2000);
    }
}


