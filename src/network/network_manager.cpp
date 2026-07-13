#include "network_manager.h"

#include <algorithm>
#include <QAbstractSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QtEndian>
#include <QUdpSocket>
#include <QTcpSocket>

namespace {

constexpr int kFramePrefixSize = static_cast<int>(sizeof(quint32));

QByteArray makeFrame(const QByteArray &payload)
{
    QByteArray frame;
    frame.resize(kFramePrefixSize);
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()), reinterpret_cast<uchar *>(frame.data()));
    frame.append(payload);
    return frame;
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

QString fallbackRoomKey(const GameRoom &room)
{
    if (!room.roomId().isEmpty()) {
        return room.roomId();
    }
    return QStringLiteral("%1:%2").arg(room.hostAddress()).arg(room.hostPort());
}

} // namespace

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
{
    connect(socket_, &QTcpSocket::connected, this, &NetworkManager::handleConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &NetworkManager::handleDisconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &NetworkManager::handleReadyRead);
#if QT_VERSION_MAJOR >= 6
    connect(socket_, &QTcpSocket::errorOccurred, this, &NetworkManager::handleSocketError);
#else
    connect(socket_,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this,
            &NetworkManager::handleSocketError);
#endif
}

void NetworkManager::setPlayerName(const QString &name)
{
    playerName_ = name;
}

QString NetworkManager::playerName() const
{
    return playerName_;
}

void NetworkManager::setAvatarData(const QByteArray &data)
{
    avatarData_ = data;
}

QByteArray NetworkManager::avatarData() const
{
    return avatarData_;
}

bool NetworkManager::connectToHost(const QString &host, quint16 port)
{
    if (host.isEmpty() || port == 0) {
        emit connectionError(QStringLiteral("主机地址或端口无效"));
        return false;
    }

    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }

    socket_->connectToHost(host, port);
    return true;
}

bool NetworkManager::connectToRoom(const GameRoom &room)
{
    return connectToHost(room.hostAddress(), room.hostPort());
}

void NetworkManager::disconnectFromHost()
{
    socket_->disconnectFromHost();
}

bool NetworkManager::isConnected() const
{
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void NetworkManager::sendMessage(const NetworkMessage &message)
{
    if (!isConnected()) {
        return;
    }

    writeFrame(QJsonDocument(networkMessageToJson(message)).toJson(QJsonDocument::Compact));
}

bool NetworkManager::startDiscovery(quint16 port)
{
    stopDiscovery();

    discoverySocket_ = new QUdpSocket(this);
    discoveryPort_ = port;

    const bool bound = discoverySocket_->bind(QHostAddress::AnyIPv4,
                                              discoveryPort_,
                                              QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!bound) {
        emit connectionError(discoverySocket_->errorString());
        discoverySocket_->deleteLater();
        discoverySocket_ = nullptr;
        return false;
    }

    connect(discoverySocket_, &QUdpSocket::readyRead, this, &NetworkManager::handleDiscoveryReadyRead);
    emit discoveryStarted(discoveryPort_);
    return true;
}

void NetworkManager::stopDiscovery()
{
    if (discoverySocket_ != nullptr) {
        discoverySocket_->close();
        discoverySocket_->deleteLater();
        discoverySocket_ = nullptr;
        emit discoveryStopped();
    }
}

bool NetworkManager::isDiscovering() const
{
    return discoverySocket_ != nullptr;
}

QVector<GameRoom> NetworkManager::discoveredRooms() const
{
    QVector<GameRoom> rooms;
    rooms.reserve(discoveredRooms_.size());
    const auto values = discoveredRooms_.values();
    for (const GameRoom &room : values) {
        rooms.push_back(room);
    }
    std::sort(rooms.begin(), rooms.end(), [](const GameRoom &lhs, const GameRoom &rhs) {
        if (lhs.roomId() != rhs.roomId()) {
            return lhs.roomId() < rhs.roomId();
        }
        if (lhs.hostName() != rhs.hostName()) {
            return lhs.hostName() < rhs.hostName();
        }
        if (lhs.hostAddress() != rhs.hostAddress()) {
            return lhs.hostAddress() < rhs.hostAddress();
        }
        return lhs.hostPort() < rhs.hostPort();
    });
    return rooms;
}

void NetworkManager::clearDiscoveredRooms()
{
    discoveredRooms_.clear();
    emit roomsChanged();
}

void NetworkManager::handleConnected()
{
    NetworkMessage login;
    login.type = MessageType::Login;
    login.payload.insert(QStringLiteral("name"), playerName_.isEmpty() ? QStringLiteral("Player") : playerName_);
    if (!avatarData_.isEmpty()) {
        login.payload.insert(QStringLiteral("avatar"), QString::fromLatin1(avatarData_.toBase64()));
    }
    sendMessage(login);
    emit connectedToServer();
}

void NetworkManager::handleDisconnected()
{
    socketBuffer_.clear();
    emit disconnectedFromServer();
}

void NetworkManager::handleReadyRead()
{
    socketBuffer_.append(socket_->readAll());

    QByteArray payload;
    while (extractFrame(socketBuffer_, &payload)) {
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        if (!document.isObject()) {
            continue;
        }

        emit messageReceived(networkMessageFromJson(document.object()));
    }
}

void NetworkManager::handleSocketError()
{
    const QString errorText = socket_->errorString();
    if (!errorText.isEmpty()) {
        emit connectionError(errorText);
    }
}

void NetworkManager::handleDiscoveryReadyRead()
{
    if (discoverySocket_ == nullptr) {
        return;
    }

    while (discoverySocket_->hasPendingDatagrams()) {
        QNetworkDatagram datagram = discoverySocket_->receiveDatagram();
        const GameRoom room = parseDiscoveryPayload(datagram.data(), datagram.senderAddress().toString());
        if (!room.isReady()) {
            continue;
        }
        registerDiscoveredRoom(room);
    }
}

void NetworkManager::writeFrame(const QByteArray &payload)
{
    const QByteArray frame = makeFrame(payload);
    socket_->write(frame);
}

bool NetworkManager::registerDiscoveredRoom(const GameRoom &room)
{
    const QString key = fallbackRoomKey(room);
    const bool isNew = !discoveredRooms_.contains(key);

    GameRoom stored = room;
    stored.setLastSeenAt(QDateTime::currentDateTime());
    discoveredRooms_.insert(key, stored);

    if (isNew) {
        emit roomDiscovered(stored);
        emit roomsChanged();
        return true;
    }
    return false;
}

GameRoom NetworkManager::parseDiscoveryPayload(const QByteArray &payload, const QString &senderAddress) const
{
    GameRoom room;

    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return room;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("kind")).toString() != QStringLiteral("room_announcement")) {
        return room;
    }

    room.setRoomId(object.value(QStringLiteral("roomCode")).toString());
    room.setHostName(object.value(QStringLiteral("hostName")).toString());

    // 广播包中的 hostAddress 可能来自 VPN、虚拟网卡或已经失效的网卡。
    // UDP 数据报的来源地址才是当前客户端真正能看到的房主地址，因此优先使用它。
    const QString announcedAddress = object.value(QStringLiteral("hostAddress")).toString();
    room.setHostAddress(senderAddress.isEmpty() ? announcedAddress : senderAddress);
    room.setHostPort(static_cast<quint16>(object.value(QStringLiteral("hostPort")).toInt()));
    room.setBoardSize(object.value(QStringLiteral("boardSize")).toInt(gomoku_config::kBoardSize));
    room.setCurrentMode(static_cast<GameMode>(object.value(QStringLiteral("mode")).toInt(static_cast<int>(GameMode::OnlineHost))));

    const QString createdAtText = object.value(QStringLiteral("createdAt")).toString();
    if (!createdAtText.isEmpty()) {
        room.setCreatedAt(QDateTime::fromString(createdAtText, Qt::ISODate));
    }
    room.setLastSeenAt(QDateTime::currentDateTime());
    return room;
}
