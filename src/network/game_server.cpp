#include "game_server.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtEndian>
#include <QtGlobal>

#include "src/common/config.h"

namespace {

constexpr int kFramePrefixSize = static_cast<int>(sizeof(quint32));

QString detectLocalIpv4Address()
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
    if (payload == nullptr || buffer.size() < kFramePrefixSize) {
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

}

GameServer::GameServer(QObject *parent)
    : QObject(parent)
    , server_(new QTcpServer(this))
    , localAddress_(detectLocalIpv4Address())
{
    connect(server_, &QTcpServer::newConnection, this, &GameServer::handleNewConnection);
}

bool GameServer::startListening(quint16 port)
{
    if (server_->isListening()) {
        stopListening();
    }

    if (!server_->listen(QHostAddress::AnyIPv4, port)) {
        emit serverError(server_->errorString());
        return false;
    }

    localAddress_ = detectLocalIpv4Address();
    listeningPort_ = server_->serverPort();
    emit serverStarted(listeningPort_);
    return true;
}

void GameServer::stopListening()
{
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
    clientAvatarData_.clear();
    authenticatedClients_.clear();
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

QString GameServer::localAddress() const
{
    return localAddress_;
}

void GameServer::setHostName(const QString &name)
{
    hostName_ = name.trimmed();
}

QString GameServer::hostName() const
{
    return hostName_;
}

void GameServer::setHostAvatarData(const QByteArray &data)
{
    hostAvatarData_ = data;
}

QByteArray GameServer::hostAvatarData() const
{
    return hostAvatarData_;
}

void GameServer::setBoardSize(int size)
{
    boardSize_ = qMax(5, size);
}

int GameServer::boardSize() const
{
    return boardSize_;
}

void GameServer::setHostSide(PlayerSide side)
{
    hostSide_ = side;
}

PlayerSide GameServer::hostSide() const
{
    return hostSide_;
}

bool GameServer::hasReadyClient() const
{
    for (QTcpSocket *socket : authenticatedClients_) {
        if (socket != nullptr && socket->state() == QAbstractSocket::ConnectedState) {
            return true;
        }
    }
    return false;
}

void GameServer::broadcastMessage(const NetworkMessage &message, QTcpSocket *excludeSocket)
{
    const QByteArray payload = QJsonDocument(networkMessageToJson(message)).toJson(QJsonDocument::Compact);
    const QByteArray frame = makeFrame(payload);
    for (QTcpSocket *socket : authenticatedClients_) {
        if (socket == nullptr || socket == excludeSocket
            || socket->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        socket->write(frame);
    }
}

void GameServer::handleNewConnection()
{
    while (server_->hasPendingConnections()) {
        QTcpSocket *socket = server_->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }

        if (!clients_.isEmpty()) {
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        clients_.push_back(socket);
        receiveBuffers_.insert(socket, QByteArray());
        clientNames_.insert(socket, socket->peerAddress().toString());
        clientAvatarData_.insert(socket, QByteArray());

        connect(socket, &QTcpSocket::readyRead, this, &GameServer::handleClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &GameServer::handleClientDisconnected);
#if QT_VERSION_MAJOR >= 6
        connect(socket, &QTcpSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
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
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        if (!document.isObject()) {
            continue;
        }

        const NetworkMessage message = networkMessageFromJson(document.object());
        if (message.type == MessageType::Login) {
            if (authenticatedClients_.contains(socket)) {
                continue;
            }

            QString playerName = message.payload.value(QStringLiteral("name")).toString().trimmed();
            if (playerName.isEmpty()) {
                playerName = QStringLiteral("Player");
            }
            const QByteArray avatarData = QByteArray::fromBase64(
                message.payload.value(QStringLiteral("avatar")).toString().toLatin1());

            clientNames_[socket] = playerName;
            clientAvatarData_[socket] = avatarData;
            authenticatedClients_.insert(socket);
            emit clientConnected(playerName, avatarData);

            NetworkMessage gameStart;
            gameStart.type = MessageType::GameStart;
            const PlayerSide clientSide = hostSide_ == PlayerSide::Black ? PlayerSide::White : PlayerSide::Black;
            gameStart.payload.insert(QStringLiteral("yourSide"), static_cast<int>(clientSide));
            gameStart.payload.insert(QStringLiteral("firstPlayer"), static_cast<int>(PieceColor::Black));
            gameStart.payload.insert(QStringLiteral("boardSize"), boardSize_);
            gameStart.payload.insert(QStringLiteral("hostName"), hostName_.isEmpty() ? QStringLiteral("Host") : hostName_);
            if (!hostAvatarData_.isEmpty()) {
                gameStart.payload.insert(QStringLiteral("hostAvatar"), QString::fromLatin1(hostAvatarData_.toBase64()));
            }
            writeMessage(socket, gameStart);
            continue;
        }

        if (message.type == MessageType::Move && authenticatedClients_.contains(socket)) {
            emit clientMessageReceived(socket, message);
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
    authenticatedClients_.remove(socket);
    receiveBuffers_.remove(socket);
    clientNames_.remove(socket);
    clientAvatarData_.remove(socket);
    clients_.removeAll(socket);
    emit clientDisconnected(playerName);
    socket->deleteLater();
}

void GameServer::writeMessage(QTcpSocket *socket, const NetworkMessage &message)
{
    if (socket == nullptr || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    const QByteArray payload = QJsonDocument(networkMessageToJson(message)).toJson(QJsonDocument::Compact);
    socket->write(makeFrame(payload));
}
