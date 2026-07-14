#pragma once

#include <QDialog>
#include <QVector>

#include "src/common/gomoku_types.h"
#include "src/data/models.h"

class ChessBoardWidget;
class QLabel;
class QTimer;

class ReplayDialog : public QDialog
{
public:
    explicit ReplayDialog(const GameRecord &game, const QVector<MoveRecord> &moves, QWidget *parent = nullptr);

private:
    void resetBoard();
    void advanceMove();
    int replayBoardSize() const;
    QString gameSummaryText() const;

    GameRecord game_;
    QVector<MoveRecord> moves_;
    QVector<QVector<PieceColor>> board_;
    ChessBoardWidget *boardWidget_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QLabel *progressLabel_ = nullptr;
    QTimer *timer_ = nullptr;
    int currentIndex_ = 0;
};
