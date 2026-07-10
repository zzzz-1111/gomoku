#pragma once

#include <QDateTime>
#include <QString>

struct UserRecord
{
    int id = -1;
    QString name;
    int totalGames = 0;
    int wins = 0;
    int losses = 0;
    int draws = 0;
    QDateTime createdAt;
};

struct GameRecord
{
    int id = -1;
    QString mode;
    QString blackPlayer;
    QString whitePlayer;
    QString winner;
    int totalMoves = 0;
    QDateTime startTime;
    QDateTime endTime;
};

struct MoveRecord
{
    int id = -1;
    int gameId = -1;
    int stepNumber = 0;
    int playerColor = 0;
    int x = -1;
    int y = -1;
    int score = 0;
};
