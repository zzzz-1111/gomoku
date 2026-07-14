#include "board_evaluator.h"

#include <algorithm>
#include <cmath>

#include "src/core/game_rule.h"

namespace {

bool inside(const QVector<QVector<PieceColor>> &board, int x, int y)
{
    return y >= 0 && y < board.size() && x >= 0 && x < board[y].size();
}

int countOneSide(const QVector<QVector<PieceColor>> &board, int x, int y, int dx, int dy, PieceColor color)
{
    int count = 0;
    int cx = x + dx;
    int cy = y + dy;
    while (inside(board, cx, cy) && board[cy][cx] == color) {
        ++count;
        cx += dx;
        cy += dy;
    }
    return count;
}

bool isOpenEnd(const QVector<QVector<PieceColor>> &board,
               int x,
               int y,
               int dx,
               int dy,
               PieceColor color)
{
    int cx = x + dx;
    int cy = y + dy;
    while (inside(board, cx, cy) && board[cy][cx] == color) {
        cx += dx;
        cy += dy;
    }
    return inside(board, cx, cy) && board[cy][cx] == PieceColor::Empty;
}

int patternScore(int length, int openEnds)
{
    if (length >= 5) {
        return 1000000;
    }
    if (length == 4 && openEnds == 2) {
        return 120000;
    }
    if (length == 4 && openEnds == 1) {
        return 20000;
    }
    if (length == 3 && openEnds == 2) {
        return 5000;
    }
    if (length == 3 && openEnds == 1) {
        return 1000;
    }
    if (length == 2 && openEnds == 2) {
        return 300;
    }
    if (length == 2 && openEnds == 1) {
        return 80;
    }
    if (length == 1 && openEnds == 2) {
        return 20;
    }
    return length * 5 + openEnds * 2;
}

} // namespace

int BoardEvaluator::evaluateBoard(const QVector<QVector<PieceColor>> &board, PieceColor aiColor) const
{
    int score = 0;
    for (int y = 0; y < board.size(); ++y) {
        for (int x = 0; x < board[y].size(); ++x) {
            if (board[y][x] != aiColor) {
                continue;
            }

            const int directions[][2] = {
                {1, 0},
                {0, 1},
                {1, 1},
                {1, -1},
            };

            for (const auto &direction : directions) {
                const int dx = direction[0];
                const int dy = direction[1];
                const int px = x - dx;
                const int py = y - dy;
                if (inside(board, px, py) && board[py][px] == aiColor) {
                    continue;
                }

                score += scoreLine(board, x, y, dx, dy, aiColor);
            }
        }
    }
    return score;
}

int BoardEvaluator::evaluatePoint(const QVector<QVector<PieceColor>> &board, int x, int y, PieceColor aiColor) const
{
    if (!inside(board, x, y) || board[y][x] != PieceColor::Empty) {
        return -1000000;
    }

    QVector<QVector<PieceColor>> placed = board;
    placed[y][x] = aiColor;
    if (GameRule::hasFiveInRow(placed, x, y)) {
        return 1000000;
    }

    const PieceColor opponent = (aiColor == PieceColor::Black) ? PieceColor::White : PieceColor::Black;
    const int directions[][2] = {
        {1, 0},
        {0, 1},
        {1, 1},
        {1, -1},
    };

    int score = 0;
    for (const auto &direction : directions) {
        const int dx = direction[0];
        const int dy = direction[1];

        const int aiCount = 1 + countOneSide(placed, x, y, dx, dy, aiColor)
                            + countOneSide(placed, x, y, -dx, -dy, aiColor);
        const int aiOpenEnds = (isOpenEnd(placed, x, y, dx, dy, aiColor) ? 1 : 0)
                       + (isOpenEnd(placed, x, y, -dx, -dy, aiColor) ? 1 : 0);
        score += patternScore(aiCount, aiOpenEnds);

        const int oppCount = 1 + countOneSide(placed, x, y, dx, dy, opponent)
                             + countOneSide(placed, x, y, -dx, -dy, opponent);
        const int oppOpenEnds = (isOpenEnd(placed, x, y, dx, dy, opponent) ? 1 : 0)
                    + (isOpenEnd(placed, x, y, -dx, -dy, opponent) ? 1 : 0);
        score += patternScore(oppCount, oppOpenEnds) / 2;
    }

    const int centerY = board.size() / 2;
    const int centerX = board[y].size() / 2;
    const int distance = std::abs(x - centerX) + std::abs(y - centerY);
    score += std::max(0, 40 - distance * 2);
    return score;
}

int BoardEvaluator::scoreLine(const QVector<QVector<PieceColor>> &board,
                              int x,
                              int y,
                              int dx,
                              int dy,
                              PieceColor color) const
{
    const int length = 1 + countOneSide(board, x, y, dx, dy, color) + countOneSide(board, x, y, -dx, -dy, color);
    const int openEnds = (isOpenEnd(board, x, y, dx, dy, color) ? 1 : 0)
                       + (isOpenEnd(board, x, y, -dx, -dy, color) ? 1 : 0);
    return patternScore(length, openEnds);
}
