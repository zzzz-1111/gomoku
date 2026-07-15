#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QBuffer>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIODevice>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include "src/core/game_controller.h"

namespace {

PieceColor pieceColorForSide(PlayerSide side)
{
    return side == PlayerSide::Black ? PieceColor::Black : PieceColor::White;
}

QString sideAvatarResourcePath(PlayerSide side)
{
    return side == PlayerSide::Black
        ? QStringLiteral(":/assets/avatars/black.png")
        : QStringLiteral(":/assets/avatars/white.png");
}

QByteArray encodeAvatarForNetwork(const QString &path)
{
    QImage image(path);
    if (image.isNull()) {
        return {};
    }

    const QImage scaled = image.scaled(QSize(128, 128),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "PNG");
    return bytes;
}

bool showOnlineDebugDialogPrompt(QWidget *parent, bool *isHost, QString *remoteName)
{
    if (parent == nullptr || isHost == nullptr || remoteName == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("联机调试"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *hintLabel = new QLabel(QStringLiteral("调试模式会在本地模拟联机流程，方便演示头像、昵称、入场动画、主机/客户端切换和轮流落子。"), &dialog);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #4b5c52;"));
    layout->addWidget(hintLabel);

    auto *roleLabel = new QLabel(QStringLiteral("当前角色：模拟主机"), &dialog);
    roleLabel->setStyleSheet(QStringLiteral("color: #1f3a2b; font-weight: 600;"));
    layout->addWidget(roleLabel);

    auto *roleButtonRow = new QHBoxLayout;
    auto *hostButton = new QPushButton(QStringLiteral("切换到主机"), &dialog);
    auto *clientButton = new QPushButton(QStringLiteral("切换到客户端"), &dialog);
    hostButton->setCheckable(true);
    clientButton->setCheckable(true);

    auto applyRoleStyle = [&](bool hostSelected) {
        const QString activeStyle = QStringLiteral(
            "background: #1f3a2b; color: white; border-radius: 12px; padding: 8px 14px;");
        const QString inactiveStyle = QStringLiteral(
            "background: rgba(31,58,43,0.08); color: #1f3a2b; border-radius: 12px; padding: 8px 14px;");
        hostButton->setStyleSheet(hostSelected ? activeStyle : inactiveStyle);
        clientButton->setStyleSheet(hostSelected ? inactiveStyle : activeStyle);
        roleLabel->setText(hostSelected ? QStringLiteral("当前角色：模拟主机")
                                        : QStringLiteral("当前角色：模拟客户端"));
    };

    auto syncRoleButtons = [&](bool hostSelected) {
        hostButton->setChecked(hostSelected);
        clientButton->setChecked(!hostSelected);
        applyRoleStyle(hostSelected);
    };

    QObject::connect(hostButton, &QPushButton::clicked, &dialog, [&]() {
        syncRoleButtons(true);
    });
    QObject::connect(clientButton, &QPushButton::clicked, &dialog, [&]() {
        syncRoleButtons(false);
    });

    syncRoleButtons(true);
    roleButtonRow->addWidget(hostButton);
    roleButtonRow->addWidget(clientButton);
    layout->addLayout(roleButtonRow);

    auto *form = new QFormLayout;
    auto *remoteEdit = new QLineEdit(&dialog);
    remoteEdit->setPlaceholderText(QStringLiteral("默认：调试对手"));
    remoteEdit->setText(QStringLiteral("调试对手"));
    form->addRow(QStringLiteral("对手名称"), remoteEdit);
    layout->addLayout(form);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("开始调试"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttonBox);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *isHost = hostButton->isChecked();
    const QString enteredName = remoteEdit->text().trimmed();
    *remoteName = enteredName.isEmpty() ? QStringLiteral("调试对手") : enteredName;
    return true;
}

}

void MainWindow::showOnlineDebugDialog()
{
    QString remoteName = QStringLiteral("调试对手");
    bool isHost = true;
    if (!showOnlineDebugDialogPrompt(this, &isHost, &remoteName)) {
        return;
    }

    startOnlineDebugSession(isHost, remoteName);
}

void MainWindow::switchOnlineDebugRole(bool isHost)
{
    if (!onlineDebugMode_ || currentMode_ != GameMode::OnlineClient || controller_ == nullptr) {
        return;
    }

    if (onlineDebugIsHost_ == isHost) {
        return;
    }

    onlineDebugIsHost_ = isHost;
    humanSide_ = isHost ? PlayerSide::Black : PlayerSide::White;
    controller_->setHumanSide(humanSide_);
    ++onlineDebugSessionToken_;
    onlineDebugWaiting_ = false;
    setOnlineGameActive(true);
    refreshGameInfo();

    if (ui != nullptr) {
        ui->statusLabel->setText(isHost
                                     ? QStringLiteral("状态：已切换为主机控制")
                                     : QStringLiteral("状态：已切换为客户端控制"));
    }

    statusBar()->showMessage(isHost ? QStringLiteral("已切换为主机控制")
                                     : QStringLiteral("已切换为客户端控制"),
                             2200);

    if (controller_->snapshot().gameOver) {
        return;
    }

    if (controller_->currentPlayer() != pieceColorForSide(humanSide_)) {
        onlineDebugWaiting_ = true;
        scheduleOnlineDebugRemoteMove();
    }

    updateBoardMoveInput();
}

void MainWindow::startOnlineDebugSession(bool isHost, const QString &remoteName)
{
    onlineDebugMode_ = true;
    onlineDebugIsHost_ = isHost;
    onlineDebugWaiting_ = !isHost;
    ++onlineDebugSessionToken_;
    const quint64 sessionToken = onlineDebugSessionToken_;
    const PlayerSide localSide = isHost ? PlayerSide::Black : PlayerSide::White;
    remoteAccountName_ = remoteName.trimmed().isEmpty() ? QStringLiteral("调试对手") : remoteName.trimmed();
    const PlayerSide remoteSide = isHost ? PlayerSide::White : PlayerSide::Black;
    remoteAvatarBytes_ = encodeAvatarForNetwork(sideAvatarResourcePath(remoteSide));

    startGame(GameMode::OnlineClient, localSide);
    beginActiveMatchRecord();
    setOnlineGameActive(false);
    refreshGameInfo();
    if (ui != nullptr) {
        ui->statusLabel->setText(isHost
                                     ? QStringLiteral("状态：联机调试中，模拟主机可发起对局")
                                     : QStringLiteral("状态：联机接入中，联机模式下主机在收到客户端前不能落子"));
    }

    auto beginDebugMatch = [this, isHost, sessionToken]() {
        if (!onlineDebugMode_ || currentMode_ != GameMode::OnlineClient) {
            return;
        }
        onlineDebugWaiting_ = false;
        setOnlineGameActive(true);
        refreshGameInfo();
        statusBar()->showMessage(isHost ? QStringLiteral("已进入联机调试模式：模拟主机")
                                         : QStringLiteral("客户端已接入，联机模式禁止主机在收到客户端前落子"),
                                 2500);

        playOnlineMatchIntro([this, sessionToken]() {
            if (sessionToken != onlineDebugSessionToken_) {
                return;
            }
            if (controller_ != nullptr
                && !controller_->snapshot().gameOver
                && controller_->currentPlayer() != pieceColorForSide(humanSide_)) {
                scheduleOnlineDebugRemoteMove();
            }
        });
    };

    if (isHost) {
        beginDebugMatch();
        return;
    }

    statusBar()->showMessage(QStringLiteral("客户端接入中，请稍候，联机模式下主机未收到客户端前不能动"), 2500);
    QTimer::singleShot(2400, this, [this, beginDebugMatch, sessionToken]() {
        if (sessionToken != onlineDebugSessionToken_) {
            return;
        }
        beginDebugMatch();
    });
}

bool MainWindow::chooseDebugRemoteMove(BoardPosition *position) const
{
    if (position == nullptr || controller_ == nullptr) {
        return false;
    }

    const auto &board = controller_->board();
    const int size = controller_->boardSize();
    if (size <= 0) {
        return false;
    }

    const int center = size / 2;
    if (board[center][center] == PieceColor::Empty) {
        position->x = center;
        position->y = center;
        return true;
    }

    for (int radius = 1; radius < size; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const int x = center + dx;
                const int y = center + dy;
                if (x < 0 || y < 0 || x >= size || y >= size) {
                    continue;
                }
                if (board[y][x] != PieceColor::Empty) {
                    continue;
                }
                position->x = x;
                position->y = y;
                return true;
            }
        }
    }

    return false;
}

void MainWindow::scheduleOnlineDebugRemoteMove()
{
    if (!onlineDebugMode_ || controller_ == nullptr || currentMode_ != GameMode::OnlineClient) {
        return;
    }

    if (controller_->snapshot().gameOver
        || controller_->currentPlayer() == pieceColorForSide(humanSide_)) {
        return;
    }

    BoardPosition movePosition;
    if (!chooseDebugRemoteMove(&movePosition)) {
        return;
    }

    const quint64 sessionToken = onlineDebugSessionToken_;
    QTimer::singleShot(360, this, [this, movePosition, sessionToken]() {
        if (sessionToken != onlineDebugSessionToken_
            || !onlineDebugMode_
            || controller_ == nullptr
            || currentMode_ != GameMode::OnlineClient) {
            return;
        }
        if (controller_->snapshot().gameOver
            || controller_->currentPlayer() == pieceColorForSide(humanSide_)) {
            return;
        }
        onlineDebugWaiting_ = false;
        setOnlineGameActive(true);
        controller_->placeStone(movePosition.x, movePosition.y);
    });
}
