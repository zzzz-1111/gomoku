#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QPixmap>
#include <QVector>
#include <QWidget>

#include "src/common/gomoku_types.h"

class QEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

class ChessBoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChessBoardWidget(QWidget *parent = nullptr);

    void setBoardSize(int size);
    void setBoard(const QVector<QVector<PieceColor>> &board);
    void setLastMove(const BoardPosition &position);
    void setSuggestedMove(const BoardPosition &position);
    void clearMarkers();
    void setMoveInputEnabled(bool enabled);
    bool isMoveInputEnabled() const;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    void cellClicked(int x, int y);
    void cellHovered(int x, int y);
    void hoverLeftBoard();
    void moveInputRejected();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    BoardPosition pointToCell(const QPoint &point) const;
    QPoint cellToPoint(int x, int y) const;
    QRect boardRect() const;
    int cellSize() const;

    int boardSize_ = 15;
    QVector<QVector<PieceColor>> board_;
    BoardPosition lastMove_;
    BoardPosition suggestedMove_;
    BoardPosition hoverCell_;
    bool hasHoverCell_ = false;
    bool moveInputEnabled_ = true;
    QPixmap boardTexture_;
    QPixmap blackStone_;
    QPixmap whiteStone_;
};
