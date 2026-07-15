#include "network_manager.h"

#include <QAbstractSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
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

}

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

void NetworkManager::writeFrame(const QByteArray &payload)
{
    const QByteArray frame = makeFrame(payload);
    socket_->write(frame);
}
