#include "game_server.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QTimer>
#include <QtEndian>
#include <QUdpSocket>
#include <QUuid>
#include <QTcpServer>
#include <QTcpSocket>

#include "src/common/config.h"
#include "src/network/protocol.h"

namespace {

constexpr int kFramePrefixSize = static_cast<int>(sizeof(quint32));

QString localIpv4Address()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &networkInterface : interfaces) {
        const auto flags = networkInterface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : networkInterface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
                return address.toString();
            }
        }
    }
    return QStringLiteral("127.0.0.1");
}

bool extractFrame(QByteArray &buffer, QByteArray *payload)
{
    if (payload == nullptr) {
        return false;
    }
    if (buffer.size() < kFramePrefixSize) {
        return false;
    }

    const quint32 size = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(buffer.constData()));
    if (buffer.size() < kFramePrefixSize + static_cast<int>(size)) {
        return false;
    }

    *payload = buffer.mid(kFramePrefixSize, static_cast<int>(size));
    buffer.remove(0, kFramePrefixSize + static_cast<int>(size));
    return true;
}

void writeFrame(QTcpSocket *socket, const QByteArray &payload)
{
    if (socket == nullptr) {
        return;
    }

    const QByteArray frame = [payload]() {
        QByteArray data;
        data.resize(kFramePrefixSize);
        qToBigEndian<quint32>(static_cast<quint32>(payload.size()), reinterpret_cast<uchar *>(data.data()));
        data.append(payload);
        return data;
    }();
    socket->write(frame);
}

} // namespace

GameServer::GameServer(QObject *parent)
    : QObject(parent)
    , server_(new QTcpServer(this))
    , broadcastSocket_(new QUdpSocket(this))
    , broadcastTimer_(new QTimer(this))
{
    room_.reset();
    room_.setHostName(QHostInfo::localHostName());
    room_.setHostAddress(localIpv4Address());
    room_.setBoardSize(gomoku_config::kBoardSize);
    room_.setCurrentMode(GameMode::OnlineHost);

    connect(server_, &QTcpServer::newConnection, this, &GameServer::handleNewConnection);
    connect(broadcastTimer_, &QTimer::timeout, this, &GameServer::broadcastRoomInfo);
    broadcastTimer_->setInterval(gomoku_config::kRoomBroadcastIntervalMs);
}

bool GameServer::startListening(quint16 port)
{
    if (server_->isListening()) {
        stopListening();
    }

    ensureRoomDefaults();

    if (!server_->listen(QHostAddress::AnyIPv4, port)) {
        emit serverError(server_->errorString());
        return false;
    }

    listeningPort_ = server_->serverPort();
    room_.setHostPort(listeningPort_);
    room_.setCreatedAt(QDateTime::currentDateTime());
    room_.setLastSeenAt(room_.createdAt());

    if (roomCode_.isEmpty()) {
        roomCode_ = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toUpper();
    }
    room_.setRoomId(roomCode_);

    broadcastTimer_->start();
    broadcastRoomInfo();
    emit serverStarted(listeningPort_);
    return true;
}

void GameServer::stopListening()
{
    if (broadcastTimer_->isActive()) {
        broadcastTimer_->stop();
    }

    if (server_->isListening()) {
        server_->close();
    }

    for (QTcpSocket *socket : clients_) {
        if (socket != nullptr) {
            socket->disconnect(this);
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }
    clients_.clear();
    receiveBuffers_.clear();
    clientNames_.clear();
    authenticatedClients_.clear();
    room_.setGuestName(QString());
    listeningPort_ = 0;
    emit serverStopped();
}

bool GameServer::isListening() const
{
    return server_->isListening();
}

quint16 GameServer::listeningPort() const
{
    return listeningPort_;
}

QString GameServer::roomCode() const
{
    return roomCode_;
}

int GameServer::connectedClientCount() const
{
    return clients_.size();
}

int GameServer::readyClientCount() const
{
    int count = 0;
    for (QTcpSocket *socket : authenticatedClients_) {
        if (socket != nullptr && socket->state() == QAbstractSocket::ConnectedState) {
            ++count;
        }
    }
    return count;
}

bool GameServer::hasReadyClient() const
{
    return readyClientCount() > 0;
}

void GameServer::setHostName(const QString &name)
{
    room_.setHostName(name);
}

QString GameServer::hostName() const
{
    return room_.hostName();
}

void GameServer::setBoardSize(int size)
{
    room_.setBoardSize(size);
}

int GameServer::boardSize() const
{
    return room_.boardSize();
}

void GameServer::setHostSide(PlayerSide side)
{
    if (side == PlayerSide::Black || side == PlayerSide::White) {
        hostSide_ = side;
    }
}

PlayerSide GameServer::hostSide() const
{
    return hostSide_;
}

void GameServer::setCurrentMode(GameMode mode)
{
    room_.setCurrentMode(mode);
}

GameMode GameServer::currentMode() const
{
    return room_.currentMode();
}

void GameServer::setDiscoveryPort(quint16 port)
{
    discoveryPort_ = port;
}

quint16 GameServer::discoveryPort() const
{
    return discoveryPort_;
}

GameRoom GameServer::room() const
{
    return room_;
}

void GameServer::broadcastMessage(const NetworkMessage &message, QTcpSocket *excludeSocket)
{
    // 未完成 LOGIN 的 TCP 连接不属于对局，不能接收任何落子消息。
    if (authenticatedClients_.isEmpty()) {
        return;
    }

    const QByteArray payload = QJsonDocument(networkMessageToJson(message)).toJson(QJsonDocument::Compact);
    for (QTcpSocket *socket : authenticatedClients_) {
        if (socket == nullptr || socket == excludeSocket
            || socket->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        writeFrame(socket, payload);
    }
}

void GameServer::handleNewConnection()
{
    while (server_->hasPendingConnections()) {
        QTcpSocket *socket = server_->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }

        // 当前协议只定义了房主和一名客人。允许多个客户端会让所有客人都成为白方，
        // 从而造成回合和棋盘状态冲突。
        if (!clients_.isEmpty()) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        clients_.push_back(socket);
        receiveBuffers_.insert(socket, QByteArray());
        clientNames_.insert(socket, socket->peerAddress().toString());

        connect(socket, &QTcpSocket::readyRead, this, &GameServer::handleClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &GameServer::handleClientDisconnected);
#if QT_VERSION_MAJOR >= 6
        connect(socket,
                &QTcpSocket::errorOccurred,
                this,
                [this, socket](QAbstractSocket::SocketError) {
                    const QString errorText = socket->errorString();
                    if (!errorText.isEmpty()) {
                        emit serverError(errorText);
                    }
                });
#else
        connect(socket,
                QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
                this,
                [this, socket](QAbstractSocket::SocketError) {
                    const QString errorText = socket->errorString();
                    if (!errorText.isEmpty()) {
                        emit serverError(errorText);
                    }
                });
#endif
    }
}

void GameServer::handleClientReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    QByteArray &buffer = receiveBuffers_[socket];
    buffer.append(socket->readAll());

    QByteArray payload;
    while (extractFrame(buffer, &payload)) {
        QJsonDocument document = QJsonDocument::fromJson(payload);
        if (!document.isObject()) {
            continue;
        }

        const NetworkMessage message = networkMessageFromJson(document.object());
        switch (message.type) {
        case MessageType::Login: {
            // TCP 连接建立并不代表玩家已经准备好。只有收到一次合法 LOGIN 后，
            // 才把该连接标记为“已就绪”，并允许联机对局正式开始。
            if (authenticatedClients_.contains(socket)) {
                break;
            }

            QString playerName = message.payload.value(QStringLiteral("name")).toString().trimmed();
            if (playerName.isEmpty()) {
                playerName = QStringLiteral("Player");
            }

            clientNames_[socket] = playerName;
            authenticatedClients_.insert(socket);
            room_.setGuestName(playerName);

            // 该信号与 MainWindow 位于同一线程，默认使用直接连接。
            // MainWindow 会先重置主机棋盘并进入正式对局，然后这里再发送 GAME_START。
            emit clientConnected(playerName);

            NetworkMessage gameStart;
            gameStart.type = MessageType::GameStart;
            const PlayerSide clientSide = hostSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black;
            gameStart.payload.insert(QStringLiteral("yourSide"), static_cast<int>(clientSide));
            gameStart.payload.insert(QStringLiteral("firstPlayer"), static_cast<int>(PieceColor::Black));
            gameStart.payload.insert(QStringLiteral("mode"), static_cast<int>(GameMode::OnlineClient));
            gameStart.payload.insert(QStringLiteral("boardSize"), room_.boardSize());
            gameStart.payload.insert(QStringLiteral("roomCode"), room_.roomId());
            writeFrame(socket, QJsonDocument(networkMessageToJson(gameStart)).toJson(QJsonDocument::Compact));
            break;
        }
        case MessageType::Disconnect:
            socket->disconnectFromHost();
            break;
        case MessageType::Move:
            if (authenticatedClients_.contains(socket)) {
                emit clientMessageReceived(socket, message);
            }
            break;
        default:
            break;
        }
    }
}

void GameServer::handleClientDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (socket == nullptr) {
        return;
    }

    const QString playerName = clientNames_.value(socket, socket->peerAddress().toString());

    // 先清理状态，再发信号。这样 MainWindow 在处理断开事件时看到的
    // hasReadyClient()/readyClientCount() 已经是准确值，不会出现短暂误判。
    authenticatedClients_.remove(socket);
    receiveBuffers_.remove(socket);
    clientNames_.remove(socket);
    clients_.removeAll(socket);
    room_.setGuestName(QString());

    emit clientDisconnected(playerName);
    socket->deleteLater();
}

void GameServer::broadcastRoomInfo()
{
    if (!server_->isListening()) {
        return;
    }

    const QByteArray payload = buildRoomBroadcastPayload();
    if (payload.isEmpty()) {
        return;
    }

    bool sent = false;
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &networkInterface : interfaces) {
        const auto flags = networkInterface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)
            || !flags.testFlag(QNetworkInterface::CanBroadcast)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : networkInterface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol
                || entry.broadcast().isNull()) {
                continue;
            }
            if (broadcastSocket_->writeDatagram(payload, entry.broadcast(), discoveryPort_) >= 0) {
                sent = true;
            }
        }
    }

    // 某些系统没有提供子网广播地址，保留全局广播作为兜底。
    if (!sent) {
        broadcastSocket_->writeDatagram(payload, QHostAddress::Broadcast, discoveryPort_);
    }
    emit roomBroadcasted(room_);
}

void GameServer::ensureRoomDefaults()
{
    if (room_.hostName().isEmpty()) {
        room_.setHostName(QHostInfo::localHostName());
    }
    if (room_.hostAddress().isEmpty()) {
        room_.setHostAddress(localIpv4Address());
    }
    if (room_.boardSize() < 5) {
        room_.setBoardSize(gomoku_config::kBoardSize);
    }
    room_.setCurrentMode(GameMode::OnlineHost);
}

QByteArray GameServer::buildRoomBroadcastPayload() const
{
    if (!room_.isReady()) {
        return {};
    }

    QJsonObject object;
    object.insert(QStringLiteral("kind"), QStringLiteral("room_announcement"));
    object.insert(QStringLiteral("roomCode"), room_.roomId());
    object.insert(QStringLiteral("hostName"), room_.hostName());
    object.insert(QStringLiteral("hostAddress"), room_.hostAddress());
    object.insert(QStringLiteral("hostPort"), static_cast<int>(room_.hostPort()));
    object.insert(QStringLiteral("boardSize"), room_.boardSize());
    object.insert(QStringLiteral("mode"), static_cast<int>(room_.currentMode()));
    object.insert(QStringLiteral("createdAt"), room_.createdAt().toString(Qt::ISODate));
    object.insert(QStringLiteral("connectedClients"), connectedClientCount());

    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
