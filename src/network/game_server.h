#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QtGlobal>
#include <QString>
#include <QVector>

class QUdpSocket;
class QTimer;
class QTcpServer;
class QTcpSocket;

#include "src/common/config.h"
#include "src/network/game_room.h"

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
    void setHostName(const QString &name);
    QString hostName() const;
    void setBoardSize(int size);
    int boardSize() const;
    void setCurrentMode(GameMode mode);
    GameMode currentMode() const;
    void setDiscoveryPort(quint16 port);
    quint16 discoveryPort() const;
    GameRoom room() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected(const QString &playerName);
    void clientDisconnected(const QString &playerName);
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
    GameRoom room_;
    quint16 listeningPort_ = 0;
    quint16 discoveryPort_ = gomoku_config::kDefaultDiscoveryPort;
    QString roomCode_;
};
