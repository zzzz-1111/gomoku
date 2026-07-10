#include "mainwindow.h"

#include "ui_mainwindow.h"
#include "src/common/config.h"
#include "src/common/gomoku_types.h"
#include "src/core/game_controller.h"
#include "src/ui/chessboard_widget.h"

#include <QMessageBox>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(new GameController(this))
{
    ui->setupUi(this);
    resize(gomoku_config::kMainWindowWidth, gomoku_config::kMainWindowHeight);
    setMinimumSize(gomoku_config::kMainWindowMinWidth, gomoku_config::kMainWindowMinHeight);
    setWindowTitle(QStringLiteral("基于Qt的局域网智能五子棋竞技系统"));

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
            statusBar()->showMessage(QStringLiteral("该位置不能落子"), 2000);
        }
    });

    connect(boardWidget_, &ChessBoardWidget::cellHovered, this, [this](int x, int y) {
        statusBar()->showMessage(QStringLiteral("悬停位置：(%1, %2)").arg(x + 1).arg(y + 1));
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
        const QString playerText = (move.color == PieceColor::Black) ? QStringLiteral("黑方") : QStringLiteral("白方");
        statusBar()->showMessage(QStringLiteral("%1已落子").arg(playerText), 2000);
    });

    connect(controller_, &GameController::gameFinished, this, [this](PieceColor winner) {
        const QString winnerText = (winner == PieceColor::Black) ? QStringLiteral("黑方") : QStringLiteral("白方");
        showGameOverPrompt(QStringLiteral("对局结束"), QStringLiteral("%1获胜").arg(winnerText));
    });

    connect(controller_, &GameController::drawDetected, this, [this]() {
        showGameOverPrompt(QStringLiteral("对局结束"), QStringLiteral("双方平局"));
    });

    setupHomePage();
    setupGamePage();
    controller_->resetGame();
    ui->stackedWidget->setCurrentIndex(gomoku_config::kHomePageIndex);
    refreshGameInfo();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupHomePage()
{
    connect(ui->startGameButton, &QPushButton::clicked, this, [this]() {
        controller_->resetGame();
        ui->stackedWidget->setCurrentIndex(gomoku_config::kGamePageIndex);
        statusBar()->showMessage(QStringLiteral("已进入对局界面"), 2000);
    });

    connect(ui->historyButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("历史页面后续再接入。"));
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("设置页面后续再接入。"));
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

void MainWindow::refreshGameInfo()
{
    if (controller_ == nullptr) {
        return;
    }

    const QString currentPlayerText = (controller_->currentPlayer() == PieceColor::Black)
                                          ? QStringLiteral("黑方")
                                          : QStringLiteral("白方");

    ui->modeLabel->setText(QStringLiteral("当前模式：本地双人"));
    ui->playerLabel->setText(QStringLiteral("当前玩家：%1").arg(currentPlayerText));
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
    statusBar()->showMessage(QStringLiteral("%1，%2").arg(title, message), 4000);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(title);
    box.setText(message);
    box.setInformativeText(QStringLiteral("你可以选择继续下一局，或者返回首页。"));
    QPushButton *restartButton = box.addButton(QStringLiteral("再来一局"), QMessageBox::AcceptRole);
    QPushButton *homeButton = box.addButton(QStringLiteral("返回首页"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("关闭"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == restartButton) {
        controller_->resetGame();
        boardWidget_->clearMarkers();
        ui->stackedWidget->setCurrentIndex(gomoku_config::kGamePageIndex);
        refreshGameInfo();
        statusBar()->showMessage(QStringLiteral("已重新开始"), 2000);
        return;
    }

    if (box.clickedButton() == homeButton) {
        ui->stackedWidget->setCurrentIndex(gomoku_config::kHomePageIndex);
        statusBar()->showMessage(QStringLiteral("已返回首页"), 2000);
    }
}
