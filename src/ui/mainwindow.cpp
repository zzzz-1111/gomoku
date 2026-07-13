#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <memory>

#include "src/ai/ai_turn_actor.h"
#include "src/common/config.h"
#include "src/core/game_controller.h"
#include "src/core/human_turn_actor.h"
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
        return QStringLiteral("空位");
    }
    return QStringLiteral("未知");
}

QString sideName(PlayerSide side)
{
    switch (side) {
    case PlayerSide::Black:
        return QStringLiteral("黑棋");
    case PlayerSide::White:
        return QStringLiteral("白棋");
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

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(new GameController(this))
{
    ui->setupUi(this);
    resize(gomoku_config::kMainWindowWidth, gomoku_config::kMainWindowHeight);
    setMinimumSize(gomoku_config::kMainWindowMinWidth, gomoku_config::kMainWindowMinHeight);
    setWindowTitle(QStringLiteral("五子棋"));

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
            statusBar()->showMessage(QStringLiteral("这里不能落子"), 2000);
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
        showGameOverPrompt(QStringLiteral("对局结束"), QStringLiteral("双方平局"));
    });

    setupHomePage();
    setupGamePage();
    configureTurnActors(GameMode::LocalTwoPlayer, PlayerSide::Black, AIDifficulty::Normal);
    controller_->setGameMode(GameMode::LocalTwoPlayer);
    controller_->setHumanSide(PlayerSide::Black);
    controller_->resetGame();
    ui->stackedWidget->setCurrentIndex(gomoku_config::kHomePageIndex);
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
        return QStringLiteral("AI对战");
    case GameMode::OnlineHost:
        return QStringLiteral("局域网主机");
    case GameMode::OnlineClient:
        return QStringLiteral("局域网客户端");
    case GameMode::Replay:
        return QStringLiteral("回放模式");
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
    ui->stackedWidget->setCurrentIndex(gomoku_config::kGamePageIndex);
    syncBoardFromController();
    refreshGameInfo();
    statusBar()->showMessage(QStringLiteral("已进入%1").arg(modeText(mode)), 2000);
}

void MainWindow::setupHomePage()
{
    ui->startGameButton->setText(QStringLiteral("本地对战"));
    ui->historyButton->setText(QStringLiteral("AI对战"));
    ui->settingsButton->setText(QStringLiteral("局域网对战"));
    ui->exitButton->setText(QStringLiteral("退出程序"));

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
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("局域网模式后续接入网络模块"));
    });

    connect(ui->exitButton, &QPushButton::clicked, this, [this]() {
        close();
    });
}

void MainWindow::setupGamePage()
{
    connect(ui->backButton, &QPushButton::clicked, this, [this]() {
        ui->stackedWidget->setCurrentIndex(gomoku_config::kHomePageIndex);
        statusBar()->showMessage(QStringLiteral("已返回首页"), 2000);
    });
}

bool MainWindow::showAiSettingsDialog(PlayerSide *humanSide, AIDifficulty *aiDifficulty)
{
    if (humanSide == nullptr || aiDifficulty == nullptr) {
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("AI对局设置"));
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *sideCombo = new QComboBox(&dialog);
    sideCombo->addItem(QStringLiteral("黑棋"), static_cast<int>(PlayerSide::Black));
    sideCombo->addItem(QStringLiteral("白棋"), static_cast<int>(PlayerSide::White));
    sideCombo->setCurrentIndex(0);

    auto *difficultyCombo = new QComboBox(&dialog);
    difficultyCombo->addItem(QStringLiteral("简单"), static_cast<int>(AIDifficulty::Easy));
    difficultyCombo->addItem(QStringLiteral("普通"), static_cast<int>(AIDifficulty::Normal));
    difficultyCombo->addItem(QStringLiteral("困难"), static_cast<int>(AIDifficulty::Hard));
    difficultyCombo->setCurrentIndex(1);

    form->addRow(QStringLiteral("玩家执棋"), sideCombo);
    form->addRow(QStringLiteral("AI难度"), difficultyCombo);
    layout->addLayout(form);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始对局"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *humanSide = static_cast<PlayerSide>(sideCombo->currentData().toInt());
    *aiDifficulty = static_cast<AIDifficulty>(difficultyCombo->currentData().toInt());
    statusBar()->showMessage(
        QStringLiteral("已选择%1，难度%2").arg(sideName(*humanSide), difficultyName(*aiDifficulty)),
        2000);
    return true;
}

void MainWindow::refreshGameInfo()
{
    if (controller_ == nullptr) {
        return;
    }

    ui->modeLabel->setText(QStringLiteral("当前模式：%1").arg(modeText(controller_->gameMode())));
    ui->playerLabel->setText(QStringLiteral("当前玩家：%1").arg(playerText(controller_->currentPlayer())));
    ui->stepLabel->setText(QStringLiteral("当前步数：%1").arg(controller_->moveHistory().size()));
    ui->statusLabel->setText(QStringLiteral("状态：等待落子"));
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
    statusBar()->showMessage(QStringLiteral("%1：%2").arg(title, message), 4000);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setText(message);
    box.setInformativeText(QStringLiteral("你可以重新开始，或者返回首页。"));
    QPushButton *restartButton = box.addButton(QStringLiteral("再来一局"), QMessageBox::AcceptRole);
    QPushButton *homeButton = box.addButton(QStringLiteral("返回首页"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == restartButton) {
        controller_->resetGame();
        ui->stackedWidget->setCurrentIndex(gomoku_config::kGamePageIndex);
        syncBoardFromController();
        refreshGameInfo();
        statusBar()->showMessage(QStringLiteral("已重新开始"), 2000);
        return;
    }

    if (box.clickedButton() == homeButton) {
        ui->stackedWidget->setCurrentIndex(gomoku_config::kHomePageIndex);
        statusBar()->showMessage(QStringLiteral("已返回首页"), 2000);
    }
}
