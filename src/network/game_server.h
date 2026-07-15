#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QSet>
#include <QVector>

#include "src/common/gomoku_types.h"
#include "src/network/protocol.h"

class QTcpServer;
class QTcpSocket;

class GameServer : public QObject
{
    Q_OBJECT

public:
    explicit GameServer(QObject *parent = nullptr);

    bool startListening(quint16 port);
    void stopListening();
    bool isListening() const;
    quint16 listeningPort() const;
    QString localAddress() const;

    void setHostName(const QString &name);
    QString hostName() const;
    void setHostAvatarData(const QByteArray &data);
    QByteArray hostAvatarData() const;
    void setBoardSize(int size);
    int boardSize() const;
    void setHostSide(PlayerSide side);
    PlayerSide hostSide() const;
    bool hasReadyClient() const;
    void broadcastMessage(const NetworkMessage &message, QTcpSocket *excludeSocket = nullptr);

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected(const QString &playerName, const QByteArray &avatarData);
    void clientDisconnected(const QString &playerName);
    void clientMessageReceived(QTcpSocket *clientSocket, const NetworkMessage &message);
    void serverError(const QString &errorText);

private slots:
    void handleNewConnection();
    void handleClientReadyRead();
    void handleClientDisconnected();

private:
    void writeMessage(QTcpSocket *socket, const NetworkMessage &message);

    QTcpServer *server_ = nullptr;
    QVector<QTcpSocket *> clients_;
    QHash<QTcpSocket *, QByteArray> receiveBuffers_;
    QHash<QTcpSocket *, QString> clientNames_;
    QHash<QTcpSocket *, QByteArray> clientAvatarData_;
    QSet<QTcpSocket *> authenticatedClients_;
    QString hostName_;
    QByteArray hostAvatarData_;
    QString localAddress_;
    quint16 listeningPort_ = 0;
    int boardSize_ = 15;
    PlayerSide hostSide_ = PlayerSide::Black;
};
