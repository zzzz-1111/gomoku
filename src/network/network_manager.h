#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QtGlobal>
#include <QString>
#include <QVector>

#include "src/common/config.h"
#include "src/network/game_room.h"
#include "src/network/protocol.h"

class QTcpSocket;
class QUdpSocket;

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    void setPlayerName(const QString &name);
    QString playerName() const;

    bool connectToHost(const QString &host, quint16 port);
    bool connectToRoom(const GameRoom &room);
    void disconnectFromHost();
    bool isConnected() const;

    void sendMessage(const NetworkMessage &message);

    bool startDiscovery(quint16 port = gomoku_config::kDefaultDiscoveryPort);
    void stopDiscovery();
    bool isDiscovering() const;
    QVector<GameRoom> discoveredRooms() const;
    void clearDiscoveredRooms();

signals:
    void connectedToServer();
    void disconnectedFromServer();
    void messageReceived(const NetworkMessage &message);
    void connectionError(const QString &errorText);
    void discoveryStarted(quint16 port);
    void discoveryStopped();
    void roomDiscovered(const GameRoom &room);
    void roomsChanged();

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleReadyRead();
    void handleSocketError();
    void handleDiscoveryReadyRead();

private:
    void writeFrame(const QByteArray &payload);
    bool registerDiscoveredRoom(const GameRoom &room);
    GameRoom parseDiscoveryPayload(const QByteArray &payload, const QString &senderAddress) const;

    QTcpSocket *socket_ = nullptr;
    QUdpSocket *discoverySocket_ = nullptr;
    QHash<QString, GameRoom> discoveredRooms_;
    QString playerName_;
    QByteArray socketBuffer_;
    quint16 discoveryPort_ = gomoku_config::kDefaultDiscoveryPort;
};
