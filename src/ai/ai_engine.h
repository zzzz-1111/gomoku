#pragma once

#include <QVector>

#include "src/ai/board_evaluator.h"
#include "src/common/gomoku_types.h"

class AIEngine
{
public:
    AIEngine();

    void setDifficulty(AIDifficulty difficulty);
    void setStyle(AIStyle style);
    void setSearchDepth(int depth);

    AIDifficulty difficulty() const;
    AIStyle style() const;
    int searchDepth() const;

    MoveInfo chooseMove(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const;

private:
    QVector<BoardPosition> generateCandidates(const QVector<QVector<PieceColor>> &board) const;
    int searchBestScore(const QVector<QVector<PieceColor>> &board, PieceColor aiColor, int depth) const;

    BoardEvaluator evaluator_;
    AIDifficulty difficulty_ = AIDifficulty::Normal;
    AIStyle style_ = AIStyle::Balanced;
    int searchDepth_ = 2;
};
