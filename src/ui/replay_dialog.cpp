#include "replay_dialog.h"

#include <algorithm>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "src/common/config.h"
#include "src/ui/chessboard_widget.h"

namespace {

PieceColor pieceColorFromRecord(int value)
{
    switch (static_cast<PieceColor>(value)) {
    case PieceColor::Black:
        return PieceColor::Black;
    case PieceColor::White:
        return PieceColor::White;
    case PieceColor::Empty:
    default:
        return PieceColor::Empty;
    }
}

QString winnerText(const QString &winner)
{
    return winner.isEmpty() ? QStringLiteral("未知") : winner;
}

} // namespace

ReplayDialog::ReplayDialog(const GameRecord &game, const QVector<MoveRecord> &moves, QWidget *parent)
    : QDialog(parent)
    , game_(game)
    , moves_(moves)
{
    setWindowTitle(QStringLiteral("棋局回放"));
    setModal(true);
    resize(860, 840);

    const int boardSize = replayBoardSize();
    board_.resize(boardSize);
    for (int y = 0; y < boardSize; ++y) {
        board_[y].fill(PieceColor::Empty, boardSize);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(18, 16, 18, 16);

    summaryLabel_ = new QLabel(gameSummaryText(), this);
    summaryLabel_->setWordWrap(true);
    summaryLabel_->setStyleSheet(QStringLiteral(
        "font-family: \"华文行楷\";"
        "font-size: 18px;"
        "font-weight: 600;"
        "color: #1f3a2b;"));
    layout->addWidget(summaryLabel_);

    progressLabel_ = new QLabel(QStringLiteral("准备回放"), this);
    progressLabel_->setStyleSheet(QStringLiteral(
        "font-family: \"华文行楷\";"
        "font-size: 16px;"
        "font-weight: 400;"
        "color: #1f3a2b;"));
    layout->addWidget(progressLabel_);

    boardWidget_ = new ChessBoardWidget(this);
    boardWidget_->setBoardSize(boardSize);
    boardWidget_->setBoard(board_);
    boardWidget_->setMoveInputEnabled(false);
    layout->addWidget(boardWidget_, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttonBox->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    timer_ = new QTimer(this);
    timer_->setInterval(gomoku_config::kDefaultReplayDelayMs);
    connect(timer_, &QTimer::timeout, this, &ReplayDialog::advanceMove);

    if (moves_.isEmpty()) {
        progressLabel_->setText(QStringLiteral("这局没有可回放的落子"));
        return;
    }

    resetBoard();
    timer_->start();
}

QString ReplayDialog::gameSummaryText() const
{
    const QString mode = game_.mode.isEmpty() ? QStringLiteral("未知模式") : game_.mode;
    const QString blackPlayer = game_.blackPlayer.isEmpty() ? QStringLiteral("黑方") : game_.blackPlayer;
    const QString whitePlayer = game_.whitePlayer.isEmpty() ? QStringLiteral("白方") : game_.whitePlayer;
    return QStringLiteral("模式：%1    黑方：%2    白方：%3    胜者：%4")
        .arg(mode, blackPlayer, whitePlayer, winnerText(game_.winner));
}

void ReplayDialog::resetBoard()
{
    const int boardSize = board_.size();
    for (int y = 0; y < boardSize; ++y) {
        for (int x = 0; x < boardSize; ++x) {
            board_[y][x] = PieceColor::Empty;
        }
    }
    currentIndex_ = 0;
    boardWidget_->setBoard(board_);
    progressLabel_->setText(QStringLiteral("正在回放第 0 / %1 手").arg(moves_.size()));
}

int ReplayDialog::replayBoardSize() const
{
    int maxCoord = gomoku_config::kBoardSize - 1;
    for (const MoveRecord &move : moves_) {
        maxCoord = std::max(maxCoord, std::max(move.x, move.y));
    }
    return std::max(gomoku_config::kBoardSize, maxCoord + 1);
}

void ReplayDialog::advanceMove()
{
    if (currentIndex_ >= moves_.size()) {
        timer_->stop();
        progressLabel_->setText(QStringLiteral("回放结束，共 %1 手").arg(moves_.size()));
        return;
    }

    const MoveRecord &move = moves_.at(currentIndex_);
    const PieceColor color = pieceColorFromRecord(move.playerColor);
    if (move.y >= 0 && move.y < board_.size() && move.x >= 0 && move.x < board_.at(move.y).size()) {
        board_[move.y][move.x] = color;
    }

    boardWidget_->setBoard(board_);
    ++currentIndex_;
    progressLabel_->setText(QStringLiteral("正在回放第 %1 / %2 手")
                                .arg(currentIndex_)
                                .arg(moves_.size()));
}
