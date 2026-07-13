#pragma once

#include <QVector>

#include "src/common/gomoku_types.h"

class GameRule
{
public:
    static bool isInsideBoard(int x, int y, int boardSize = 15);
    static bool isCellEmpty(const QVector<QVector<PieceColor>> &board, int x, int y);
    static bool isLegalMove(const QVector<QVector<PieceColor>> &board, int x, int y);
    static bool hasFiveInRow(const QVector<QVector<PieceColor>> &board, int x, int y);
    static bool isBoardFull(const QVector<QVector<PieceColor>> &board);
    static PieceColor winnerAt(const QVector<QVector<PieceColor>> &board, int x, int y);
};
