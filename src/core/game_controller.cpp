#include "game_controller.h"
#include "src/core/game_rule.h"

GameController::GameController(QObject *parent)
    : QObject(parent)
{
    resetGame();
}

void GameController::resetGame()
{
    ++revision_;
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
    requestCurrentTurnAction();
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

void GameController::setTurnActor(PieceColor color, std::unique_ptr<ITurnActor> actor)
{
    if (color == PieceColor::Black) {
        blackActor_ = std::move(actor);
    } else if (color == PieceColor::White) {
        whiteActor_ = std::move(actor);
    }
}

void GameController::clearTurnActors()
{
    blackActor_.reset();
    whiteActor_.reset();
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
    requestCurrentTurnAction();
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

GameStateSnapshot GameController::snapshot() const
{
    GameStateSnapshot state;
    state.boardSize = boardSize_;
    state.mode = mode_;
    state.humanSide = humanSide_;
    state.currentPlayer = currentPlayer_;
    state.gameOver = gameOver_;
    state.revision = revision_;
    state.board = board_;
    state.moveHistory = moveHistory_;
    return state;
}

quint64 GameController::stateRevision() const
{
    return revision_;
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
    requestCurrentTurnAction();
    return true;
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

ITurnActor *GameController::turnActorFor(PieceColor color) const
{
    if (color == PieceColor::Black) {
        return blackActor_.get();
    }
    if (color == PieceColor::White) {
        return whiteActor_.get();
    }
    return nullptr;
}

void GameController::requestCurrentTurnAction()
{
    if (gameOver_) {
        return;
    }

    ITurnActor *actor = turnActorFor(currentPlayer_);
    if (actor == nullptr || !actor->isAutomatic()) {
        return;
    }

    const PieceColor side = currentPlayer_;
    if (gameOver_ || currentPlayer_ != side) {
        return;
    }

    ITurnActor *actorNow = turnActorFor(side);
    if (actorNow == nullptr || !actorNow->isAutomatic()) {
        return;
    }

    actorNow->onTurn(*this, snapshot(), side);
}
