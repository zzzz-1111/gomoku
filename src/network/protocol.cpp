#include "protocol.h"

#include <QJsonValue>

namespace {

QString typeToString(MessageType type)
{
    switch (type) {
    case MessageType::Login:
        return QStringLiteral("LOGIN");
    case MessageType::CreateRoom:
        return QStringLiteral("CREATE_ROOM");
    case MessageType::JoinRoom:
        return QStringLiteral("JOIN_ROOM");
    case MessageType::Ready:
        return QStringLiteral("READY");
    case MessageType::GameStart:
        return QStringLiteral("GAME_START");
    case MessageType::Move:
        return QStringLiteral("MOVE");
    case MessageType::MoveResult:
        return QStringLiteral("MOVE_RESULT");
    case MessageType::RestartRequest:
        return QStringLiteral("RESTART_REQUEST");
    case MessageType::GameOver:
        return QStringLiteral("GAME_OVER");
    case MessageType::Disconnect:
        return QStringLiteral("DISCONNECT");
    }
    return QStringLiteral("LOGIN");
}

} // namespace

QString messageTypeToString(MessageType type)
{
    return typeToString(type);
}

MessageType messageTypeFromString(const QString &type)
{
    if (type == QStringLiteral("CREATE_ROOM")) {
        return MessageType::CreateRoom;
    }
    if (type == QStringLiteral("JOIN_ROOM")) {
        return MessageType::JoinRoom;
    }
    if (type == QStringLiteral("READY")) {
        return MessageType::Ready;
    }
    if (type == QStringLiteral("GAME_START")) {
        return MessageType::GameStart;
    }
    if (type == QStringLiteral("MOVE")) {
        return MessageType::Move;
    }
    if (type == QStringLiteral("MOVE_RESULT")) {
        return MessageType::MoveResult;
    }
    if (type == QStringLiteral("RESTART_REQUEST")) {
        return MessageType::RestartRequest;
    }
    if (type == QStringLiteral("GAME_OVER")) {
        return MessageType::GameOver;
    }
    if (type == QStringLiteral("DISCONNECT")) {
        return MessageType::Disconnect;
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
