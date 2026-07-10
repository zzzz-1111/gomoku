#pragma once

#include <QObject>
#include <QtGlobal>
#include <QString>
#include <QVector>

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

    QString roomCode() const;
    int connectedClientCount() const;

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void clientConnected(const QString &playerName);
    void clientDisconnected(const QString &playerName);
    void serverError(const QString &errorText);

private slots:
    void handleNewConnection();
    void handleClientDisconnected();

private:
    QTcpServer *server_ = nullptr;
    QVector<QTcpSocket *> clients_;
    QString roomCode_;
};
