#pragma once

#include <QByteArray>
#include <QObject>
#include <QtGlobal>
#include <QString>

#include "src/network/protocol.h"

class QTcpSocket;

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    void setPlayerName(const QString &name);
    QString playerName() const;
    void setAvatarData(const QByteArray &data);
    QByteArray avatarData() const;

    bool connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;

    void sendMessage(const NetworkMessage &message);

signals:
    void connectedToServer();
    void disconnectedFromServer();
    void messageReceived(const NetworkMessage &message);
    void connectionError(const QString &errorText);

private slots:
    void handleConnected();
    void handleDisconnected();
    void handleReadyRead();
    void handleSocketError();

private:
    void writeFrame(const QByteArray &payload);

    QTcpSocket *socket_ = nullptr;
    QString playerName_;
    QByteArray avatarData_;
    QByteArray socketBuffer_;
};
