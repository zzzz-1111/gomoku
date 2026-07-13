#include "game_rule.h"

namespace {

bool inside(int x, int y, int boardSize)
{
    return x >= 0 && y >= 0 && x < boardSize && y < boardSize;
}

int countDirection(const QVector<QVector<PieceColor>> &board, int x, int y, int dx, int dy, PieceColor color)
{
    const int size = board.size();
    int count = 0;
    int cx = x + dx;
    int cy = y + dy;
    while (inside(cx, cy, size) && board[cy][cx] == color) {
        ++count;
        cx += dx;
        cy += dy;
    }
    return count;
}

} // namespace

bool GameRule::isInsideBoard(int x, int y, int boardSize)
{
    return inside(x, y, boardSize);
}

bool GameRule::isCellEmpty(const QVector<QVector<PieceColor>> &board, int x, int y)
{
    if (!isInsideBoard(x, y, board.size())) {
        return false;
    }
    return board[y][x] == PieceColor::Empty;
}

bool GameRule::isLegalMove(const QVector<QVector<PieceColor>> &board, int x, int y)
{
    return isCellEmpty(board, x, y);
}

bool GameRule::hasFiveInRow(const QVector<QVector<PieceColor>> &board, int x, int y)
{
    if (!isInsideBoard(x, y, board.size())) {
        return false;
    }

    const PieceColor color = board[y][x];
    if (color == PieceColor::Empty) {
        return false;
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
        const int count = 1
            + countDirection(board, x, y, dx, dy, color)
            + countDirection(board, x, y, -dx, -dy, color);
        if (count >= 5) {
            return true;
        }
    }

    return false;
}

bool GameRule::isBoardFull(const QVector<QVector<PieceColor>> &board)
{
    for (const auto &row : board) {
        for (PieceColor cell : row) {
            if (cell == PieceColor::Empty) {
                return false;
            }
        }
    }
    return true;
}

PieceColor GameRule::winnerAt(const QVector<QVector<PieceColor>> &board, int x, int y)
{
    if (!hasFiveInRow(board, x, y)) {
        return PieceColor::Empty;
    }
    return board[y][x];
}
