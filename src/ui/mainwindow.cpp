#include "mainwindow.h"

#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QBuffer>
#include <QDir>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QFormLayout>
#include <QIcon>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QLineEdit>
#include <QImage>
#include <QImageReader>
#include <QSettings>
#include <QStandardPaths>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QLabel>
#include <QHBoxLayout>
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

QString defaultAvatarResourcePath()
{
    return QStringLiteral(":/assets/avatars/default_avatar.png");
}

QString aiAvatarResourcePath()
{
    return QStringLiteral(":/assets/avatars/ai_avatar.png");
}

QString customAvatarStoragePath()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        return {};
    }

    QDir dir(baseDir);
    if (!dir.exists(QStringLiteral("avatar")) && !dir.mkpath(QStringLiteral("avatar"))) {
        return {};
    }

    return dir.filePath(QStringLiteral("avatar/custom_avatar.png"));
}

QString resolveLocalAvatarPath()
{
    const QString customPath = customAvatarStoragePath();
    if (!customPath.isEmpty() && QFileInfo::exists(customPath)) {
        return customPath;
    }
    return defaultAvatarResourcePath();
}

QPixmap loadAvatarPixmap(const QString &path, const QSize &targetSize)
{
    QPixmap pixmap;
    if (!path.isEmpty()) {
        pixmap.load(path);
    }
    if (pixmap.isNull()) {
        pixmap.load(defaultAvatarResourcePath());
    }
    if (targetSize.isValid() && !targetSize.isEmpty()) {
        pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    return pixmap;
}

QByteArray encodeAvatarForNetwork(const QString &path)
{
    QImage image(path);
    if (image.isNull()) {
        image.load(defaultAvatarResourcePath());
    }
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

QPixmap loadAvatarPixmapFromBytes(const QByteArray &bytes, const QSize &targetSize)
{
    QPixmap pixmap;
    if (!bytes.isEmpty()) {
        pixmap.loadFromData(bytes);
    }
    if (pixmap.isNull()) {
        pixmap.load(defaultAvatarResourcePath());
    }
    if (targetSize.isValid() && !targetSize.isEmpty()) {
        pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    return pixmap;
}

bool saveAvatarCopy(const QString &sourcePath, QString *errorMessage)
{
    const QString targetPath = customAvatarStoragePath();
    if (targetPath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建头像保存目录");
        }
        return false;
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = reader.errorString().isEmpty()
                ? QStringLiteral("读取头像失败")
                : reader.errorString();
        }
        return false;
    }

    if (!image.save(targetPath, "PNG")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("保存头像失败");
        }
        return false;
    }

    return true;
}

bool clearSavedAvatar(QString *errorMessage)
{
    const QString targetPath = customAvatarStoragePath();
    if (targetPath.isEmpty()) {
        return true;
    }

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("删除自定义头像失败");
        }
        return false;
    }
    return true;
}

bool showProfileSettingsDialog(QWidget *parent, QString *accountName, QString *avatarPath)
{
    if (parent == nullptr || accountName == nullptr || avatarPath == nullptr) {
        return false;
    }

    const QString originalAccountName = accountName->trimmed();
    const QString originalAvatarPath = *avatarPath;
    QString stagedAvatarSourcePath = QFileInfo::exists(originalAvatarPath) && originalAvatarPath != defaultAvatarResourcePath()
        ? originalAvatarPath
        : QString();
    bool stagedUseDefaultAvatar = stagedAvatarSourcePath.isEmpty();

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("设置"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *titleLabel = new QLabel(QStringLiteral("账号设置"), &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600; color: #1f3a2b;"));
    layout->addWidget(titleLabel);

    auto *nameRow = new QHBoxLayout;
    auto *nameLabel = new QLabel(QStringLiteral("昵称"), &dialog);
    nameLabel->setMinimumWidth(64);
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("用于联机对战显示"));
    nameEdit->setText(originalAccountName);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(nameEdit);
    layout->addLayout(nameRow);

    auto *previewLabel = new QLabel(&dialog);
    previewLabel->setFixedSize(120, 120);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet(QStringLiteral(
        "background: rgba(255,255,255,0.70);"
        "border: 1px solid rgba(31,58,43,0.18);"
        "border-radius: 16px;"));
    layout->addWidget(previewLabel, 0, Qt::AlignHCenter);

    auto *pathLabel = new QLabel(&dialog);
    pathLabel->setWordWrap(true);
    pathLabel->setStyleSheet(QStringLiteral("color: #4b5c52;"));
    layout->addWidget(pathLabel);

    auto refreshPreview = [&]() {
        const QString previewPath = stagedUseDefaultAvatar
            ? defaultAvatarResourcePath()
            : stagedAvatarSourcePath;
        previewLabel->setPixmap(loadAvatarPixmap(previewPath, previewLabel->size()));
        if (stagedUseDefaultAvatar) {
            pathLabel->setText(QStringLiteral("当前头像：默认头像"));
        } else {
            pathLabel->setText(QStringLiteral("当前头像：自定义头像"));
        }
    };

    auto *buttonRow = new QHBoxLayout;
    auto *uploadButton = new QPushButton(QStringLiteral("上传头像"), &dialog);
    auto *resetButton = new QPushButton(QStringLiteral("恢复默认"), &dialog);
    buttonRow->addWidget(uploadButton);
    buttonRow->addWidget(resetButton);
    layout->addLayout(buttonRow);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttonBox);

    QObject::connect(uploadButton, &QPushButton::clicked, &dialog, [&]() {
        const QString fileName = QFileDialog::getOpenFileName(&dialog,
                                                              QStringLiteral("选择头像"),
                                                              QString(),
                                                              QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (fileName.isEmpty()) {
            return;
        }

        stagedAvatarSourcePath = fileName;
        stagedUseDefaultAvatar = false;
        refreshPreview();
    });

    QObject::connect(resetButton, &QPushButton::clicked, &dialog, [&]() {
        stagedAvatarSourcePath.clear();
        stagedUseDefaultAvatar = true;
        refreshPreview();
    });

    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    refreshPreview();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString enteredName = nameEdit->text().trimmed();
    if (enteredName.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("保存失败"), QStringLiteral("昵称不能为空"));
        return false;
    }

    const bool nameChanged = enteredName != originalAccountName;
    bool avatarChanged = false;

    if (stagedUseDefaultAvatar) {
        if (originalAvatarPath != defaultAvatarResourcePath()) {
            QString errorMessage;
            if (!clearSavedAvatar(&errorMessage)) {
                QMessageBox::warning(parent, QStringLiteral("保存失败"), errorMessage);
                return false;
            }
            *avatarPath = defaultAvatarResourcePath();
            avatarChanged = true;
        }
    } else if (stagedAvatarSourcePath != originalAvatarPath) {
        QString errorMessage;
        if (!saveAvatarCopy(stagedAvatarSourcePath, &errorMessage)) {
            QMessageBox::warning(parent, QStringLiteral("保存失败"), errorMessage);
            return false;
        }
        *avatarPath = customAvatarStoragePath();
        avatarChanged = true;
    }

    if (nameChanged) {
        *accountName = enteredName;
        QSettings settings;
        settings.setValue(QStringLiteral("user/accountName"), *accountName);
    }

    return nameChanged || avatarChanged;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , controller_(new GameController(this))
    , recordManager_(new RecordManager(this))
    , networkManager_(new NetworkManager(this))
    , gameServer_(new GameServer(this))
{
    ui->setupUi(this);
    localAccountName_ = loadOrPromptLocalAccountName(this);
    localAvatarPath_ = resolveLocalAvatarPath();
    networkManager_->setPlayerName(localAccountName_);
    networkManager_->setAvatarData(encodeAvatarForNetwork(localAvatarPath_));
    gameServer_->setHostAvatarData(encodeAvatarForNetwork(localAvatarPath_));

    auto *previewShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), this);
    connect(previewShortcut, &QShortcut::activated, this, [this]() {
        playOnlineMatchIntroPreview();
    });

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
        saveCurrentGameResult(winner);
        showGameOverPrompt(QStringLiteral("对局结束"),
                           QStringLiteral("%1获胜").arg(colorName(winner)));
    });

    connect(controller_, &GameController::drawDetected, this, [this]() {
        saveCurrentGameResult(PieceColor::Empty);
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
        if (matchIntroOverlay_ != nullptr) {
            matchIntroOverlay_->close();
        }
        remoteAccountName_.clear();
        remoteAvatarBytes_.clear();
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

            // 只有收到 GAME_START 才把客户端切换到可落子状态。
            setOnlineGameActive(true);
            startGame(GameMode::OnlineClient, localSide);
            controller_->setCurrentPlayer(firstPlayer);
            ui->networkStatusLabel->setText(QStringLiteral("状态：联机对局已开始"));
            statusBar()->showMessage(QStringLiteral("联机对局已开始"), 2000);
            playOnlineMatchIntro();
            return;
        }

        if (message.type == MessageType::RestartRequest) {
            const bool accepted = message.payload.value(QStringLiteral("accepted")).toBool(false);
            if (accepted) {
                setOnlineGameActive(true);
                beginActiveMatchRecord();
                controller_->resetGame();
                ui->stackedWidget->setCurrentWidget(ui->gamePage);
                syncBoardFromController();
                refreshGameInfo();
                statusBar()->showMessage(QStringLiteral("已开始下一局"), 2000);
            } else {
                setOnlineGameActive(false);
                ui->stackedWidget->setCurrentWidget(ui->networkPage);
                statusBar()->showMessage(QStringLiteral("主机已结束本局"), 2500);
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
        if (matchIntroOverlay_ != nullptr) {
            matchIntroOverlay_->close();
        }
        remoteAccountName_.clear();
        remoteAvatarBytes_.clear();
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

    connect(gameServer_, &GameServer::clientConnected, this, [this](const QString &playerName, const QByteArray &avatarData) {
        // 只有收到客户端 LOGIN 后才设置为 true；单纯建立 TCP 连接不算加入。
        hostPlayerJoined_ = true;
        onlineGameStarted_ = true;
        remoteAccountName_ = playerName;
        remoteAvatarBytes_ = avatarData;
        startGame(GameMode::OnlineHost, humanSide_);
        updateBoardMoveInput();
        ui->networkStatusLabel->setText(QStringLiteral("状态：%1 已加入，对局开始").arg(playerName));
        statusBar()->showMessage(QStringLiteral("%1 已加入，双方可以开始落子").arg(playerName), 2500);
        playOnlineMatchIntro();
    });
    connect(gameServer_, &GameServer::clientDisconnected, this, [this](const QString &playerName) {
        // 对手离开后立即锁住主机输入。
        hostPlayerJoined_ = false;
        if (matchIntroOverlay_ != nullptr) {
            matchIntroOverlay_->close();
        }
        remoteAccountName_.clear();
        remoteAvatarBytes_.clear();
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
    const bool onlineMode = mode == GameMode::OnlineHost || mode == GameMode::OnlineClient;
    const bool waitingForClient = mode == GameMode::OnlineHost && !onlineGameStarted_;
    if (!waitingForClient) {
        beginActiveMatchRecord();
    }
    controller_->resetGame();
    ui->stackedWidget->setCurrentWidget(ui->gamePage);
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
        if (!networkManager_->isDiscovering()) {
            networkManager_->clearDiscoveredRooms();
            networkManager_->startDiscovery();
        }
        refreshDiscoveredRooms();
        ui->networkStatusLabel->setText(QStringLiteral("状态：准备扫描"));
        ui->stackedWidget->setCurrentWidget(ui->networkPage);
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        const bool profileChanged = showProfileSettingsDialog(this, &localAccountName_, &localAvatarPath_);
        if (profileChanged) {
            networkManager_->setPlayerName(localAccountName_);
            networkManager_->setAvatarData(encodeAvatarForNetwork(localAvatarPath_));
            gameServer_->setHostAvatarData(encodeAvatarForNetwork(localAvatarPath_));
            if (currentMode_ == GameMode::OnlineHost && gameServer_ != nullptr && gameServer_->isListening()) {
                gameServer_->setHostName(localAccountName_);
                refreshDiscoveredRooms();
            }
            refreshGameInfo();
            refreshOnlineIdentityLabels();
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

        gameServer_->setHostName(localAccountName_);
        gameServer_->setHostAvatarData(encodeAvatarForNetwork(localAvatarPath_));
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
                statusBar()->showMessage(QStringLiteral("已返回主机对局"), 2000);
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
        ui->playerIconLabel->setPixmap(loadAvatarPixmapFromBytes(remoteAvatarBytes_, ui->playerIconLabel->size()));
    } else {
        const QString avatarPath = QFileInfo::exists(localAvatarPath_)
            ? localAvatarPath_
            : defaultAvatarResourcePath();
        ui->playerIconLabel->setPixmap(loadAvatarPixmap(avatarPath, ui->playerIconLabel->size()));
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

QString MainWindow::displayNameForSide(PlayerSide side) const
{
    switch (currentMode_) {
    case GameMode::HumanVsAI:
        if (side == humanSide_) {
            return localAccountName_.isEmpty() ? QStringLiteral("玩家") : localAccountName_;
        }
        return QStringLiteral("AI玩家");
    case GameMode::OnlineHost:
        if (side == humanSide_) {
            return localAccountName_.isEmpty() ? QStringLiteral("主机") : localAccountName_;
        }
        return remoteAccountName_.isEmpty() ? QStringLiteral("对方玩家") : remoteAccountName_;
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
        record.score = move.score;
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

void MainWindow::playOnlineMatchIntro()
{
    const QString avatarPath = QFileInfo::exists(localAvatarPath_)
        ? localAvatarPath_
        : defaultAvatarResourcePath();

    IntroPlayerInfo localPlayer;
    localPlayer.name = localAccountName_.isEmpty() ? QStringLiteral("Player") : localAccountName_;
    localPlayer.sideText = sideName(humanSide_);
    localPlayer.avatar = loadAvatarPixmap(avatarPath, QSize(128, 128));

    const PlayerSide remoteSide = humanSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black;
    IntroPlayerInfo remotePlayer;
    remotePlayer.name = remoteAccountName_.isEmpty() ? QStringLiteral("对方玩家") : remoteAccountName_;
    remotePlayer.sideText = sideName(remoteSide);
    remotePlayer.avatar = loadAvatarPixmapFromBytes(remoteAvatarBytes_, QSize(128, 128));

    const bool localIsBlack = humanSide_ == PlayerSide::Black;
    const IntroPlayerInfo &leftPlayer = localIsBlack ? localPlayer : remotePlayer;
    const IntroPlayerInfo &rightPlayer = localIsBlack ? remotePlayer : localPlayer;

    playMatchIntro(leftPlayer, rightPlayer);
}

void MainWindow::playAiMatchIntro()
{
    const QString avatarPath = QFileInfo::exists(localAvatarPath_)
        ? localAvatarPath_
        : defaultAvatarResourcePath();

    IntroPlayerInfo humanPlayer;
    humanPlayer.name = localAccountName_.isEmpty() ? QStringLiteral("玩家") : localAccountName_;
    humanPlayer.sideText = sideName(humanSide_);
    humanPlayer.avatar = loadAvatarPixmap(avatarPath, QSize(128, 128));

    const PlayerSide aiSide = humanSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black;
    IntroPlayerInfo aiPlayer;
    aiPlayer.name = QStringLiteral("AI");
    aiPlayer.sideText = sideName(aiSide);
    aiPlayer.avatar = loadAvatarPixmap(aiAvatarResourcePath(), QSize(128, 128));

    const bool humanIsBlack = humanSide_ == PlayerSide::Black;
    const IntroPlayerInfo &leftPlayer = humanIsBlack ? humanPlayer : aiPlayer;
    const IntroPlayerInfo &rightPlayer = humanIsBlack ? aiPlayer : humanPlayer;

    playMatchIntro(leftPlayer, rightPlayer);
}

void MainWindow::playOnlineMatchIntroPreview()
{
    const QString avatarPath = QFileInfo::exists(localAvatarPath_)
        ? localAvatarPath_
        : defaultAvatarResourcePath();

    IntroPlayerInfo localPlayer;
    localPlayer.name = localAccountName_.isEmpty() ? QStringLiteral("预览玩家") : localAccountName_;
    localPlayer.sideText = sideName(humanSide_);
    localPlayer.avatar = loadAvatarPixmap(avatarPath, QSize(128, 128));

    IntroPlayerInfo remotePlayer;
    remotePlayer.name = QStringLiteral("预览对手");
    remotePlayer.sideText = sideName(humanSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black);
    remotePlayer.avatar = loadAvatarPixmap(
        humanSide_ == PlayerSide::Black ? QStringLiteral(":/assets/avatars/white.png")
                                        : QStringLiteral(":/assets/avatars/black.png"),
        QSize(128, 128));

    const bool localIsBlack = humanSide_ == PlayerSide::Black;
    const IntroPlayerInfo &leftPlayer = localIsBlack ? localPlayer : remotePlayer;
    const IntroPlayerInfo &rightPlayer = localIsBlack ? remotePlayer : localPlayer;

    playMatchIntro(leftPlayer, rightPlayer);
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
    box.setInformativeText(QStringLiteral("是否再来一局？"));
    QPushButton *yesButton = box.addButton(QStringLiteral("是"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("否"), QMessageBox::RejectRole);
    box.exec();

    const bool restart = box.clickedButton() == yesButton;

    NetworkMessage decision;
    decision.type = MessageType::RestartRequest;
    decision.payload.insert(QStringLiteral("accepted"), restart);
    gameServer_->broadcastMessage(decision);

    if (restart) {
        setOnlineGameActive(true);
        beginActiveMatchRecord();
        controller_->resetGame();
        ui->stackedWidget->setCurrentWidget(ui->gamePage);
        syncBoardFromController();
        refreshGameInfo();
        statusBar()->showMessage(QStringLiteral("已开始下一局"), 2000);
        return;
    }

    setOnlineGameActive(false);
    ui->stackedWidget->setCurrentWidget(ui->networkPage);
    refreshDiscoveredRooms();
    statusBar()->showMessage(QStringLiteral("已返回联机页面"), 2500);
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
        beginActiveMatchRecord();
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


