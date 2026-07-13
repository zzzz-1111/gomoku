#pragma once

#include <QVector>

#include "src/ai/board_evaluator.h"
#include "src/common/gomoku_types.h"

class AIEngine
{
public:
    AIEngine();

    void setDifficulty(AIDifficulty difficulty);
    void setSearchDepth(int depth);

    AIDifficulty difficulty() const;
    int searchDepth() const;

    MoveInfo chooseMove(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const;

private:
    QVector<BoardPosition> generateCandidates(const QVector<QVector<PieceColor>> &board) const;
    int evaluateState(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const;
    int negamax(QVector<QVector<PieceColor>> &board,
                PieceColor currentColor,
                PieceColor aiColor,
                int depth,
                int alpha,
                int beta,
                int ply) const;

    BoardEvaluator evaluator_;
    AIDifficulty difficulty_ = AIDifficulty::Normal;
    int searchDepth_ = 2;
};
