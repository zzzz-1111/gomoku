#pragma once

#include <QJsonObject>
#include <QString>

enum class MessageType
{
    Login,
    CreateRoom,
    JoinRoom,
    Ready,
    GameStart,
    Move,
    MoveResult,
    Chat,
    UndoRequest,
    UndoResponse,
    RestartRequest,
    GameOver,
    Disconnect
};

struct NetworkMessage
{
    MessageType type = MessageType::Login;
    QJsonObject payload;
};

QString messageTypeToString(MessageType type);
MessageType messageTypeFromString(const QString &type);
