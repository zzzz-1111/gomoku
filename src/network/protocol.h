#pragma once

#include <QJsonObject>
#include <QString>

enum class MessageType
{
    Login,
    GameStart,
    Move,
    RestartRequest
};

struct NetworkMessage
{
    MessageType type = MessageType::Login;
    QJsonObject payload;
};

QString messageTypeToString(MessageType type);
MessageType messageTypeFromString(const QString &type);
QJsonObject networkMessageToJson(const NetworkMessage &message);
NetworkMessage networkMessageFromJson(const QJsonObject &object);
