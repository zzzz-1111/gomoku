#pragma once

#include <QDateTime>
#include <QString>

#include "src/common/gomoku_types.h"

class GameRoom
{
public:
    GameRoom();

    void setRoomId(const QString &roomId);
    QString roomId() const;

    void setHostName(const QString &name);
    QString hostName() const;

    void setGuestName(const QString &name);
    QString guestName() const;

    void setBoardSize(int size);
    int boardSize() const;

    void setCurrentMode(GameMode mode);
    GameMode currentMode() const;

    void setCreatedAt(const QDateTime &time);
    QDateTime createdAt() const;

    bool isReady() const;
    void reset();

private:
    QString roomId_;
    QString hostName_;
    QString guestName_;
    int boardSize_ = 15;
    GameMode currentMode_ = GameMode::OnlineHost;
    QDateTime createdAt_;
};
