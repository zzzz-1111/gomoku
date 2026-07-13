#pragma once

#include <QObject>
#include <QtGlobal>
#include <QString>
#include <QVector>
#include <memory>

#include "src/common/gomoku_types.h"
#include "src/core/runtime/game_state.h"
#include "src/core/runtime/turn_context.h"
#include "src/core/actor/turn_actor.h"

class GameController : public QObject, public ITurnContext
{
    Q_OBJECT

public:
    explicit GameController(QObject *parent = nullptr);

    void resetGame();
    void setGameMode(GameMode mode);
    GameMode gameMode() const;

    void setBoardSize(int size);
    int boardSize() const;

    void setHumanSide(PlayerSide side);
    PlayerSide humanSide() const;

    void setTurnActor(PieceColor color, std::unique_ptr<ITurnActor> actor);
    void clearTurnActors();

    void setCurrentPlayer(PieceColor color);
    PieceColor currentPlayer() const override;

    const QVector<QVector<PieceColor>> &board() const;
    QVector<MoveInfo> moveHistory() const;
    GameStateSnapshot snapshot() const;
    quint64 stateRevision() const override;

    bool canPlaceAt(int x, int y) const;
    bool placeStone(int x, int y) override;

signals:
    void boardChanged();
    void currentPlayerChanged(PieceColor color);
    void moveApplied(const MoveInfo &move);
    void gameFinished(PieceColor winner);
    void drawDetected();
    void tipRequested(const QString &text);

private:
    void switchTurn();
    void appendMove(int x, int y, PieceColor color);
    bool checkGameOver(int x, int y);
    void requestCurrentTurnAction();
    ITurnActor *turnActorFor(PieceColor color) const;

    int boardSize_ = 15;
    GameMode mode_ = GameMode::LocalTwoPlayer;
    PlayerSide humanSide_ = PlayerSide::Black;
    PieceColor currentPlayer_ = PieceColor::Black;
    bool gameOver_ = false;
    QVector<QVector<PieceColor>> board_;
    QVector<MoveInfo> moveHistory_;
    std::unique_ptr<ITurnActor> blackActor_;
    std::unique_ptr<ITurnActor> whiteActor_;
    quint64 revision_ = 0;
};
