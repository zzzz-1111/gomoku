#include "protocol.h"

#include <QJsonValue>

namespace {

QString typeToString(MessageType type)
{
    switch (type) {
    case MessageType::Login:
        return QStringLiteral("LOGIN");
    case MessageType::GameStart:
        return QStringLiteral("GAME_START");
    case MessageType::Move:
        return QStringLiteral("MOVE");
    case MessageType::RestartRequest:
        return QStringLiteral("RESTART_REQUEST");
    }
    return QStringLiteral("LOGIN");
}

}

QString messageTypeToString(MessageType type)
{
    return typeToString(type);
}

MessageType messageTypeFromString(const QString &type)
{
    if (type == QStringLiteral("GAME_START")) {
        return MessageType::GameStart;
    }
    if (type == QStringLiteral("MOVE")) {
        return MessageType::Move;
    }
    if (type == QStringLiteral("RESTART_REQUEST")) {
        return MessageType::RestartRequest;
    }
    return MessageType::Login;
}

QJsonObject networkMessageToJson(const NetworkMessage &message)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), messageTypeToString(message.type));
    object.insert(QStringLiteral("payload"), message.payload);
    return object;
}

NetworkMessage networkMessageFromJson(const QJsonObject &object)
{
    NetworkMessage message;
    message.type = messageTypeFromString(object.value(QStringLiteral("type")).toString());
    message.payload = object.value(QStringLiteral("payload")).toObject();
    return message;
}
