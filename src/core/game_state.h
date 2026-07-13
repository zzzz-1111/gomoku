#pragma once

#include <QVector>

#include "src/common/gomoku_types.h"

struct GameStateSnapshot
{
    int boardSize = 15;
    GameMode mode = GameMode::LocalTwoPlayer;
    PlayerSide humanSide = PlayerSide::Black;
    PieceColor currentPlayer = PieceColor::Black;
    bool gameOver = false;
    QVector<QVector<PieceColor>> board;
    QVector<MoveInfo> moveHistory;
};
