#pragma once

#include <QVector>

#include "src/common/gomoku_types.h"

class BoardEvaluator
{
public:
    int evaluateBoard(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const;
    int evaluatePoint(const QVector<QVector<PieceColor>> &board, int x, int y, PieceColor aiColor) const;

private:
    int scoreLine(const QVector<QVector<PieceColor>> &board,
                  int x,
                  int y,
                  int dx,
                  int dy,
                  PieceColor color) const;
};
