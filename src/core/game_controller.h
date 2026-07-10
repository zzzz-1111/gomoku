#pragma once

#include <QObject>
#include <QVector>

#include "src/common/gomoku_types.h"

class GameController : public QObject
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

    void setCurrentPlayer(PieceColor color);
    PieceColor currentPlayer() const;

    const QVector<QVector<PieceColor>> &board() const;
    QVector<MoveInfo> moveHistory() const;

    bool canPlaceAt(int x, int y) const;
    bool placeStone(int x, int y);
    bool undoLastMove();
    bool canUndo() const;

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

    int boardSize_ = 15;
    GameMode mode_ = GameMode::LocalTwoPlayer;
    PlayerSide humanSide_ = PlayerSide::Black;
    PieceColor currentPlayer_ = PieceColor::Black;
    bool gameOver_ = false;
    QVector<QVector<PieceColor>> board_;
    QVector<MoveInfo> moveHistory_;
};
