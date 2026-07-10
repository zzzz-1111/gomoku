#include "game_controller.h"
#include "game_rule.h"

GameController::GameController(QObject *parent)
    : QObject(parent)
{
    resetGame();
}

void GameController::resetGame()
{
    board_.clear();
    board_.resize(boardSize_);
    for (int y = 0; y < boardSize_; ++y) {
        board_[y].resize(boardSize_);
        for (int x = 0; x < boardSize_; ++x) {
            board_[y][x] = PieceColor::Empty;
        }
    }

    moveHistory_.clear();
    currentPlayer_ = PieceColor::Black;
    gameOver_ = false;

    emit boardChanged();
    emit currentPlayerChanged(currentPlayer_);
}

void GameController::setGameMode(GameMode mode)
{
    mode_ = mode;
}

GameMode GameController::gameMode() const
{
    return mode_;
}

void GameController::setBoardSize(int size)
{
    if (size < 5) {
        size = 5;
    }
    boardSize_ = size;
    resetGame();
}

int GameController::boardSize() const
{
    return boardSize_;
}

void GameController::setHumanSide(PlayerSide side)
{
    humanSide_ = side;
}

PlayerSide GameController::humanSide() const
{
    return humanSide_;
}

void GameController::setCurrentPlayer(PieceColor color)
{
    if (color != PieceColor::Black && color != PieceColor::White) {
        return;
    }

    if (currentPlayer_ == color) {
        return;
    }

    currentPlayer_ = color;
    emit currentPlayerChanged(currentPlayer_);
}

PieceColor GameController::currentPlayer() const
{
    return currentPlayer_;
}

const QVector<QVector<PieceColor>> &GameController::board() const
{
    return board_;
}

QVector<MoveInfo> GameController::moveHistory() const
{
    return moveHistory_;
}

bool GameController::canPlaceAt(int x, int y) const
{
    if (gameOver_) {
        return false;
    }
    return GameRule::isLegalMove(board_, x, y);
}

bool GameController::placeStone(int x, int y)
{
    if (!canPlaceAt(x, y)) {
        return false;
    }

    board_[y][x] = currentPlayer_;
    appendMove(x, y, currentPlayer_);

    const MoveInfo move = moveHistory_.back();
    emit boardChanged();
    emit moveApplied(move);

    if (checkGameOver(x, y)) {
        return true;
    }

    switchTurn();
    emit currentPlayerChanged(currentPlayer_);
    return true;
}

bool GameController::undoLastMove()
{
    if (moveHistory_.isEmpty()) {
        return false;
    }

    const MoveInfo lastMove = moveHistory_.takeLast();
    if (GameRule::isInsideBoard(lastMove.position.x, lastMove.position.y, boardSize_)) {
        board_[lastMove.position.y][lastMove.position.x] = PieceColor::Empty;
    }

    currentPlayer_ = lastMove.color;
    gameOver_ = false;
    emit boardChanged();
    emit currentPlayerChanged(currentPlayer_);
    return true;
}

bool GameController::canUndo() const
{
    return !moveHistory_.isEmpty();
}

void GameController::switchTurn()
{
    currentPlayer_ = (currentPlayer_ == PieceColor::Black) ? PieceColor::White : PieceColor::Black;
}

void GameController::appendMove(int x, int y, PieceColor color)
{
    MoveInfo move;
    move.position = {x, y};
    move.color = color;
    move.stepNumber = moveHistory_.size() + 1;
    move.score = 0;
    moveHistory_.push_back(move);
}

bool GameController::checkGameOver(int x, int y)
{
    if (GameRule::hasFiveInRow(board_, x, y)) {
        gameOver_ = true;
        emit gameFinished(GameRule::winnerAt(board_, x, y));
        return true;
    }

    if (GameRule::isBoardFull(board_)) {
        gameOver_ = true;
        emit drawDetected();
        return true;
    }

    return false;
}
