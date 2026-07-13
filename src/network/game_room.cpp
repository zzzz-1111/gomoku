#include "game_room.h"

GameRoom::GameRoom() = default;

void GameRoom::setRoomId(const QString &roomId)
{
    roomId_ = roomId;
}

QString GameRoom::roomId() const
{
    return roomId_;
}

void GameRoom::setHostName(const QString &name)
{
    hostName_ = name;
}

QString GameRoom::hostName() const
{
    return hostName_;
}

void GameRoom::setHostAddress(const QString &address)
{
    hostAddress_ = address;
}

QString GameRoom::hostAddress() const
{
    return hostAddress_;
}

void GameRoom::setHostPort(quint16 port)
{
    hostPort_ = port;
}

quint16 GameRoom::hostPort() const
{
    return hostPort_;
}

void GameRoom::setGuestName(const QString &name)
{
    guestName_ = name;
}

QString GameRoom::guestName() const
{
    return guestName_;
}

void GameRoom::setBoardSize(int size)
{
    boardSize_ = size;
}

int GameRoom::boardSize() const
{
    return boardSize_;
}

void GameRoom::setCurrentMode(GameMode mode)
{
    currentMode_ = mode;
}

GameMode GameRoom::currentMode() const
{
    return currentMode_;
}

void GameRoom::setCreatedAt(const QDateTime &time)
{
    createdAt_ = time;
}

QDateTime GameRoom::createdAt() const
{
    return createdAt_;
}

void GameRoom::setLastSeenAt(const QDateTime &time)
{
    lastSeenAt_ = time;
}

QDateTime GameRoom::lastSeenAt() const
{
    return lastSeenAt_;
}

bool GameRoom::isReady() const
{
    return !roomId_.isEmpty() && !hostName_.isEmpty() && !hostAddress_.isEmpty() && hostPort_ != 0;
}

void GameRoom::reset()
{
    roomId_.clear();
    hostName_.clear();
    hostAddress_.clear();
    hostPort_ = 0;
    guestName_.clear();
    boardSize_ = 15;
    currentMode_ = GameMode::OnlineHost;
    createdAt_ = QDateTime();
    lastSeenAt_ = QDateTime();
}
