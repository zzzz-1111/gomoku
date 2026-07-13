#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QtGlobal>
#include <QString>
#include <QSet>
#include <QVector>

class QUdpSocket;
class QTimer;
class QTcpServer;
class QTcpSocket;

#include "src/common/config.h"
#include "src/network/game_room.h"
#include "src/network/protocol.h"

class GameServer : public QObject
{
    Q_OBJECT

public:
    explicit GameServer(QObject *parent = nullptr);

    bool startListening(quint16 port);
    void stopListening();
    bool isListening() const;
    quint16 listeningPort() const;

    QString roomCode() const;
    int connectedClientCount() const;
    int readyClientCount() const;
    bool hasReadyClient() const;
    void setHostName(const QString &name);
    QString hostName() const;
    void setHostAvatarData(const QByteArray &data);
    QByteArray hostAvatarData() const;
    void setBoardSize(int size);
    int boardSize() const;
    void setHostSide(PlayerSide side);
    PlayerSide hostSide() const;
    void broadcastMessage(const NetworkMessage &message, QTcpSocket *excludeSocket = nullptr);
    void setCurrentMode(GameMode mode);
    GameMode currentMode() const;
    void setDiscoveryPort(quint16 port);
    quint16 discoveryPort() const;
    GameRoom room() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected(const QString &playerName, const QByteArray &avatarData);
    void clientDisconnected(const QString &playerName);
    void clientMessageReceived(QTcpSocket *clientSocket, const NetworkMessage &message);
    void serverError(const QString &errorText);
    void roomBroadcasted(const GameRoom &room);

private slots:
    void handleNewConnection();
    void handleClientReadyRead();
    void handleClientDisconnected();
    void broadcastRoomInfo();

private:
    void ensureRoomDefaults();
    QByteArray buildRoomBroadcastPayload() const;

    QTcpServer *server_ = nullptr;
    QUdpSocket *broadcastSocket_ = nullptr;
    QTimer *broadcastTimer_ = nullptr;
    QVector<QTcpSocket *> clients_;
    QHash<QTcpSocket *, QByteArray> receiveBuffers_;
    QHash<QTcpSocket *, QString> clientNames_;
    QHash<QTcpSocket *, QByteArray> clientAvatarData_;
    QSet<QTcpSocket *> authenticatedClients_;
    GameRoom room_;
    quint16 listeningPort_ = 0;
    quint16 discoveryPort_ = gomoku_config::kDefaultDiscoveryPort;
    QString roomCode_;
    QByteArray hostAvatarData_;
    PlayerSide hostSide_ = PlayerSide::Black;
};
