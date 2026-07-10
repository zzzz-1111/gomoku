#include "chessboard_widget.h"

#include <QImage>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPointF>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <queue>

namespace {

constexpr int kBoardPadding = 28;
constexpr int kLabelGap = 18;

QString columnLabel(int index)
{
    return QString(QChar(static_cast<char>('A' + index)));
}

bool isStarPoint(int boardSize, int x, int y)
{
    if (boardSize < 15) {
        return false;
    }

    const int near = 3;
    const int center = boardSize / 2;
    const int far = boardSize - 1 - near;
    return (x == near || x == center || x == far) && (y == near || y == center || y == far);
}

QPixmap makeTransparentPixmap(const QString &path)
{
    QImage image(path);
    if (image.isNull()) {
        return {};
    }

    image = image.convertToFormat(QImage::Format_ARGB32);
    const QColor background = image.pixelColor(0, 0);
    auto isBackground = [&](const QColor &color) {
        return color.alpha() > 0
            && qAbs(color.red() - background.red()) <= 25
            && qAbs(color.green() - background.green()) <= 25
            && qAbs(color.blue() - background.blue()) <= 25;
    };

    const int width = image.width();
    const int height = image.height();
    QVector<uchar> visited(width * height, 0);
    auto indexOf = [width](int x, int y) {
        return y * width + x;
    };

    std::queue<QPoint> queue;
    auto tryPush = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return;
        }
        const int index = indexOf(x, y);
        if (visited[index]) {
            return;
        }
        const QColor color = image.pixelColor(x, y);
        if (!isBackground(color)) {
            return;
        }
        visited[index] = 1;
        queue.push(QPoint(x, y));
    };

    for (int x = 0; x < width; ++x) {
        tryPush(x, 0);
        tryPush(x, height - 1);
    }
    for (int y = 0; y < height; ++y) {
        tryPush(0, y);
        tryPush(width - 1, y);
    }

    while (!queue.empty()) {
        const QPoint point = queue.front();
        queue.pop();
        image.setPixelColor(point.x(), point.y(), QColor(0, 0, 0, 0));

        tryPush(point.x() + 1, point.y());
        tryPush(point.x() - 1, point.y());
        tryPush(point.x(), point.y() + 1);
        tryPush(point.x(), point.y() - 1);
    }

    return QPixmap::fromImage(image);
}

QPixmap fallbackStonePixmap(PieceColor color, int diameter)
{
    QPixmap pixmap(diameter, diameter);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPointF center(diameter / 2.0, diameter / 2.0);
    QRadialGradient gradient(center, diameter / 2.0);
    if (color == PieceColor::Black) {
        gradient.setColorAt(0.0, QColor("#666666"));
        gradient.setColorAt(1.0, QColor("#111111"));
    } else {
        gradient.setColorAt(0.0, QColor("#ffffff"));
        gradient.setColorAt(1.0, QColor("#d0d0d0"));
    }
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor("#5c4528"), 1));
    painter.drawEllipse(QRectF(1, 1, diameter - 2, diameter - 2));

    return pixmap;
}

} // namespace

ChessBoardWidget::ChessBoardWidget(QWidget *parent)
    : QWidget(parent)
    , boardTexture_(":/assets/board/board_texture.png")
    , blackStone_(makeTransparentPixmap(":/assets/board/stone_black.png"))
    , whiteStone_(makeTransparentPixmap(":/assets/board/stone_white.png"))
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setBoardSize(boardSize_);
}

void ChessBoardWidget::setBoardSize(int size)
{
    if (size < 5) {
        size = 5;
    }

    if (boardSize_ == size && board_.size() == size) {
        return;
    }

    boardSize_ = size;
    board_.resize(boardSize_);
    for (int y = 0; y < boardSize_; ++y) {
        board_[y].resize(boardSize_);
        for (int x = 0; x < boardSize_; ++x) {
            board_[y][x] = PieceColor::Empty;
        }
    }

    clearMarkers();
    update();
}

void ChessBoardWidget::setBoard(const QVector<QVector<PieceColor>> &board)
{
    if (board.isEmpty()) {
        return;
    }

    board_ = board;
    boardSize_ = board_.size();
    update();
}

void ChessBoardWidget::setLastMove(const BoardPosition &position)
{
    lastMove_ = position;
    update();
}

void ChessBoardWidget::setSuggestedMove(const BoardPosition &position)
{
    suggestedMove_ = position;
    update();
}

void ChessBoardWidget::clearMarkers()
{
    lastMove_ = {};
    suggestedMove_ = {};
    hoverCell_ = {};
    hasHoverCell_ = false;
    update();
}

QSize ChessBoardWidget::minimumSizeHint() const
{
    return {480, 480};
}

QSize ChessBoardWidget::sizeHint() const
{
    return {640, 640};
}

void ChessBoardWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (boardSize_ < 2) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect board = boardRect();
    painter.fillRect(rect(), QColor("#f5f0e6"));

    if (!boardTexture_.isNull()) {
        painter.drawPixmap(board, boardTexture_);
    } else {
        QLinearGradient bgGradient(board.topLeft(), board.bottomRight());
        bgGradient.setColorAt(0.0, QColor("#ead0a5"));
        bgGradient.setColorAt(1.0, QColor("#c99962"));
        painter.setBrush(bgGradient);
        painter.setPen(QPen(QColor("#8b6738"), 2));
        painter.drawRoundedRect(board.adjusted(-8, -8, 8, 8), 12, 12);
    }

    const int spacing = cellSize();
    const QPoint topLeft = board.topLeft();
    const QPoint bottomRight = board.bottomRight();

    painter.setPen(QPen(QColor(66, 46, 20, 180), 1));
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < boardSize_; ++i) {
        const int x = topLeft.x() + i * spacing;
        const int y = topLeft.y() + i * spacing;
        painter.drawLine(QPoint(x, topLeft.y()), QPoint(x, bottomRight.y()));
        painter.drawLine(QPoint(topLeft.x(), y), QPoint(bottomRight.x(), y));
    }

    painter.setBrush(QColor("#5a3d20"));
    painter.setPen(Qt::NoPen);
    for (int y = 0; y < boardSize_; ++y) {
        for (int x = 0; x < boardSize_; ++x) {
            if (!isStarPoint(boardSize_, x, y)) {
                continue;
            }
            const QPoint center = cellToPoint(x, y);
            painter.drawEllipse(center, 4, 4);
        }
    }

    QFont labelFont = font();
    labelFont.setPointSize(std::max(8, font().pointSize() - 1));
    painter.setFont(labelFont);
    painter.setPen(QColor("#4d3820"));

    for (int i = 0; i < boardSize_; ++i) {
        const QPoint columnPoint = cellToPoint(i, 0);
        const QPoint rowPoint = cellToPoint(0, i);
        painter.drawText(QRect(columnPoint.x() - 10, board.top() - kLabelGap, 20, 16),
                         Qt::AlignCenter, columnLabel(i));
        painter.drawText(QRect(board.left() - kLabelGap - 10, rowPoint.y() - 8, 20, 16),
                         Qt::AlignCenter, QString::number(i + 1));
    }

    auto drawMarker = [&](const BoardPosition &position, const QColor &color, int radius) {
        if (!position.isValid()) {
            return;
        }
        if (position.x < 0 || position.y < 0 || position.x >= boardSize_ || position.y >= boardSize_) {
            return;
        }
        const QPoint center = cellToPoint(position.x, position.y);
        QPen pen(color);
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);
    };

    drawMarker(lastMove_, QColor("#2c7be5"), spacing / 3);
    drawMarker(suggestedMove_, QColor("#d97706"), spacing / 4);

    if (hasHoverCell_) {
        drawMarker(hoverCell_, QColor("#2a9d8f"), spacing / 4);
    }

    const int stoneDiameter = std::max(24, spacing - 6);
    const QPixmap blackStone = blackStone_.isNull()
        ? fallbackStonePixmap(PieceColor::Black, stoneDiameter)
        : blackStone_.scaled(stoneDiameter, stoneDiameter, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPixmap whiteStone = whiteStone_.isNull()
        ? fallbackStonePixmap(PieceColor::White, stoneDiameter)
        : whiteStone_.scaled(stoneDiameter, stoneDiameter, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    for (int y = 0; y < boardSize_; ++y) {
        for (int x = 0; x < boardSize_; ++x) {
            const PieceColor piece = board_.value(y).value(x, PieceColor::Empty);
            if (piece == PieceColor::Empty) {
                continue;
            }

            const QPoint center = cellToPoint(x, y);
            const QRect stoneRect(center.x() - stoneDiameter / 2,
                                  center.y() - stoneDiameter / 2,
                                  stoneDiameter,
                                  stoneDiameter);
            const QPixmap &stonePixmap = (piece == PieceColor::Black) ? blackStone : whiteStone;
            painter.drawPixmap(stoneRect, stonePixmap);
        }
    }
}

void ChessBoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const BoardPosition position = pointToCell(event->position().toPoint());
    if (!position.isValid()) {
        return;
    }

    emit cellClicked(position.x, position.y);
}

void ChessBoardWidget::mouseMoveEvent(QMouseEvent *event)
{
    const BoardPosition position = pointToCell(event->position().toPoint());
    if (position.isValid()) {
        if (!hasHoverCell_ || hoverCell_.x != position.x || hoverCell_.y != position.y) {
            hoverCell_ = position;
            hasHoverCell_ = true;
            emit cellHovered(position.x, position.y);
            update();
        }
        return;
    }

    if (hasHoverCell_) {
        hasHoverCell_ = false;
        emit hoverLeftBoard();
        update();
    }
}

void ChessBoardWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);

    if (hasHoverCell_) {
        hasHoverCell_ = false;
        emit hoverLeftBoard();
        update();
    }
}

void ChessBoardWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

BoardPosition ChessBoardWidget::pointToCell(const QPoint &point) const
{
    const QRect board = boardRect();
    const int spacing = cellSize();
    if (spacing <= 0) {
        return {};
    }

    const int x = qRound((point.x() - board.left()) / static_cast<double>(spacing));
    const int y = qRound((point.y() - board.top()) / static_cast<double>(spacing));

    if (x < 0 || y < 0 || x >= boardSize_ || y >= boardSize_) {
        return {};
    }

    const QPoint center = cellToPoint(x, y);
    const int hitRadius = std::max(12, spacing / 2 - 2);
    if ((center - point).manhattanLength() > hitRadius) {
        return {};
    }

    return {x, y};
}

QPoint ChessBoardWidget::cellToPoint(int x, int y) const
{
    const QRect board = boardRect();
    const int spacing = cellSize();
    return {board.left() + x * spacing, board.top() + y * spacing};
}

QRect ChessBoardWidget::boardRect() const
{
    const int spacing = cellSize();
    const int boardSpan = spacing * (boardSize_ - 1);
    const int left = (width() - boardSpan) / 2;
    const int top = (height() - boardSpan) / 2;
    const int minLeft = kBoardPadding + kLabelGap;
    const int minTop = kBoardPadding + kLabelGap;
    const int maxLeft = std::max(minLeft, width() - boardSpan - kBoardPadding);
    const int maxTop = std::max(minTop, height() - boardSpan - kBoardPadding);
    return {std::clamp(left, minLeft, maxLeft), std::clamp(top, minTop, maxTop), boardSpan, boardSpan};
}

int ChessBoardWidget::cellSize() const
{
    const int usableWidth = std::max(1, width() - 2 * kBoardPadding - kLabelGap);
    const int usableHeight = std::max(1, height() - 2 * kBoardPadding - kLabelGap);
    const int span = std::max(1, boardSize_ - 1);
    return std::max(12, std::min(usableWidth, usableHeight) / span);
}
