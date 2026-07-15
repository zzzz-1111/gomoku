#include "ai_engine.h"

#include <algorithm>
#include <limits>

#include "src/core/game_rule.h"

namespace {

constexpr int kWinScore = 100000000;
constexpr int kNegInf = std::numeric_limits<int>::min() / 4;
constexpr int kPosInf = std::numeric_limits<int>::max() / 4;

bool inside(const QVector<QVector<PieceColor>> &board, int x, int y)
{
    return y >= 0 && y < board.size() && x >= 0 && x < board[y].size();
}

PieceColor opponentOf(PieceColor color)
{
    return color == PieceColor::Black ? PieceColor::White : PieceColor::Black;
}

bool boardHasStone(const QVector<QVector<PieceColor>> &board)
{
    for (const auto &row : board) {
        for (PieceColor cell : row) {
            if (cell != PieceColor::Empty) {
                return true;
            }
        }
    }
    return false;
}

int candidateLimitForDepth(int depth)
{
    if (depth >= 8) {
        return 8;
    }
    if (depth >= 5) {
        return 10;
    }
    return 12;
}

BoardPosition boardCenter(const QVector<QVector<PieceColor>> &board)
{
    if (board.isEmpty()) {
        return {0, 0};
    }

    const int centerY = board.size() / 2;
    const int centerX = board[centerY].isEmpty() ? 0 : board[centerY].size() / 2;
    return {centerX, centerY};
}

}

AIEngine::AIEngine() = default;

void AIEngine::setDifficulty(AIDifficulty difficulty)
{
    difficulty_ = difficulty;

    switch (difficulty_) {
    case AIDifficulty::Easy:
        searchDepth_ = 2;
        break;
    case AIDifficulty::Normal:
        searchDepth_ = 3;
        break;
    case AIDifficulty::Hard:
        searchDepth_ = 4;
        break;
    }
}

void AIEngine::setSearchDepth(int depth)
{
    searchDepth_ = std::max(1, depth);
}

AIDifficulty AIEngine::difficulty() const
{
    return difficulty_;
}

int AIEngine::searchDepth() const
{
    return searchDepth_;
}

QVector<BoardPosition> AIEngine::generateCandidates(const QVector<QVector<PieceColor>> &board) const
{
    QVector<BoardPosition> candidates;
    if (board.isEmpty()) {
        return candidates;
    }

    if (!boardHasStone(board)) {
        candidates.push_back(boardCenter(board));
        return candidates;
    }

    QVector<QVector<bool>> marked(board.size());
    for (int y = 0; y < board.size(); ++y) {
        marked[y] = QVector<bool>(board[y].size(), false);
    }
    auto mark = [&](int x, int y) {
        if (!inside(board, x, y) || board[y][x] != PieceColor::Empty || marked[y][x]) {
            return;
        }
        marked[y][x] = true;
        candidates.push_back({x, y});
    };

    for (int y = 0; y < board.size(); ++y) {
        for (int x = 0; x < board[y].size(); ++x) {
            if (board[y][x] == PieceColor::Empty) {
                continue;
            }
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    mark(x + dx, y + dy);
                }
            }
        }
    }

    if (candidates.isEmpty()) {
        for (int y = 0; y < board.size(); ++y) {
            for (int x = 0; x < board[y].size(); ++x) {
                if (board[y][x] == PieceColor::Empty) {
                    candidates.push_back({x, y});
                }
            }
        }
    }

    return candidates;
}

int AIEngine::evaluateState(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const
{
    const PieceColor opponent = opponentOf(aiColor);
    return evaluator_.evaluateBoard(board, aiColor) - evaluator_.evaluateBoard(board, opponent);
}

int AIEngine::negamax(QVector<QVector<PieceColor>> &board,
                      PieceColor currentColor,
                      PieceColor aiColor,
                      int depth,
                      int alpha,
                      int beta,
                      int ply) const
{
    if (GameRule::isBoardFull(board)) {
        return 0;
    }

    if (depth <= 0) {
        const int staticScore = evaluateState(board, aiColor);
        return (currentColor == aiColor) ? staticScore : -staticScore;
    }

    QVector<BoardPosition> candidates = generateCandidates(board);
    if (candidates.isEmpty()) {
        const int staticScore = evaluateState(board, aiColor);
        return (currentColor == aiColor) ? staticScore : -staticScore;
    }

    struct RankedCandidate
    {
        BoardPosition position;
        int score = 0;
    };

    QVector<RankedCandidate> ranked;
    ranked.reserve(candidates.size());
    for (const BoardPosition &candidate : candidates) {
        RankedCandidate item;
        item.position = candidate;
        item.score = evaluator_.evaluatePoint(board, candidate.x, candidate.y, currentColor);
        ranked.push_back(item);
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedCandidate &lhs, const RankedCandidate &rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        if (lhs.position.y != rhs.position.y) {
            return lhs.position.y < rhs.position.y;
        }
        return lhs.position.x < rhs.position.x;
    });

    const int candidateLimit = candidateLimitForDepth(searchDepth_);
    if (ranked.size() > candidateLimit) {
        ranked.resize(candidateLimit);
    }

    const PieceColor nextColor = opponentOf(currentColor);
    int bestScore = kNegInf;

    for (const RankedCandidate &candidate : ranked) {
        const int x = candidate.position.x;
        const int y = candidate.position.y;
        board[y][x] = currentColor;

        int score = 0;
        if (GameRule::hasFiveInRow(board, x, y)) {
            score = kWinScore - ply;
        } else if (GameRule::isBoardFull(board)) {
            score = 0;
        } else {
            score = -negamax(board, nextColor, aiColor, depth - 1, -beta, -alpha, ply + 1);
        }

        board[y][x] = PieceColor::Empty;
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            break;
        }
    }

    return bestScore;
}

MoveInfo AIEngine::chooseMove(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const
{
    MoveInfo bestMove;
    bestMove.color = aiColor;

    QVector<BoardPosition> candidates = generateCandidates(board);
    if (candidates.isEmpty()) {
        bestMove.position = boardCenter(board);
        return bestMove;
    }

    struct RankedCandidate
    {
        BoardPosition position;
        int score = 0;
    };

    QVector<RankedCandidate> ranked;
    ranked.reserve(candidates.size());
    for (const BoardPosition &candidate : candidates) {
        RankedCandidate item;
        item.position = candidate;
        item.score = evaluator_.evaluatePoint(board, candidate.x, candidate.y, aiColor);
        ranked.push_back(item);
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedCandidate &lhs, const RankedCandidate &rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        if (lhs.position.y != rhs.position.y) {
            return lhs.position.y < rhs.position.y;
        }
        return lhs.position.x < rhs.position.x;
    });

    const int candidateLimit = candidateLimitForDepth(searchDepth_);
    if (ranked.size() > candidateLimit) {
        ranked.resize(candidateLimit);
    }

    QVector<QVector<PieceColor>> workingBoard = board;
    const PieceColor opponent = opponentOf(aiColor);

    int alpha = kNegInf;
    int bestScore = kNegInf;
    BoardPosition bestPosition = ranked.front().position;

    for (const RankedCandidate &candidate : ranked) {
        const int x = candidate.position.x;
        const int y = candidate.position.y;
        workingBoard[y][x] = aiColor;

        int score = 0;
        if (GameRule::hasFiveInRow(workingBoard, x, y)) {
            score = kWinScore;
        } else if (GameRule::isBoardFull(workingBoard)) {
            score = 0;
        } else if (searchDepth_ <= 1) {
            score = evaluateState(workingBoard, aiColor);
        } else {
            score = -negamax(workingBoard, opponent, aiColor, searchDepth_ - 1, -kPosInf, -alpha, 1);
        }

        workingBoard[y][x] = PieceColor::Empty;

        if (score > bestScore) {
            bestScore = score;
            bestPosition = candidate.position;
        }

        alpha = std::max(alpha, score);
    }

    bestMove.position = bestPosition;
    return bestMove;
}
