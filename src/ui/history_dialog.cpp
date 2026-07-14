#include "history_dialog.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QAbstractButton>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "src/data/models.h"
#include "src/record/record_manager.h"
#include "src/ui/replay_dialog.h"

HistoryDialog::HistoryDialog(RecordManager *recordManager, QWidget *parent)
    : QDialog(parent)
    , recordManager_(recordManager)
{
    setWindowTitle(QStringLiteral("历史记录"));
    setModal(true);
    resize(900, 520);
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "background: #f5f7f1;"
        "}"
        "QLabel#historyTitleLabel {"
        "font-size: 22px;"
        "font-weight: 700;"
        "color: #1f3a2b;"
        "}"
        "QLabel#historyEmptyLabel {"
        "font-size: 16px;"
        "color: #5f6f66;"
        "padding: 28px;"
        "background: rgba(255,255,255,0.72);"
        "border: 1px dashed rgba(31,58,43,0.20);"
        "border-radius: 16px;"
        "}"
        "QTableWidget {"
        "background: rgba(255,255,255,0.84);"
        "gridline-color: rgba(31,58,43,0.10);"
        "border: 1px solid rgba(31,58,43,0.16);"
        "border-radius: 14px;"
        "selection-background-color: rgba(31,58,43,0.16);"
        "selection-color: #1f3a2b;"
        "}"
        "QHeaderView::section {"
        "background: rgba(31,58,43,0.10);"
        "color: #1f3a2b;"
        "border: none;"
        "padding: 8px 10px;"
        "font-weight: 600;"
        "}"));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 18, 20, 18);

    auto *titleLabel = new QLabel(QStringLiteral("最近对局"), this);
    titleLabel->setObjectName(QStringLiteral("historyTitleLabel"));
    layout->addWidget(titleLabel);

    emptyLabel_ = new QLabel(QStringLiteral("暂无历史记录"), this);
    emptyLabel_->setObjectName(QStringLiteral("historyEmptyLabel"));
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setWordWrap(true);
    layout->addWidget(emptyLabel_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({
        QStringLiteral("结束时间"),
        QStringLiteral("模式"),
        QStringLiteral("黑方"),
        QStringLiteral("白方"),
        QStringLiteral("胜者"),
        QStringLiteral("步数"),
    });
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setAlternatingRowColors(true);
    table_->verticalHeader()->setVisible(false);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(table_, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    replayButton_ = buttonBox->addButton(QStringLiteral("回放"), QDialogButtonBox::ActionRole);
    auto *refreshButton = buttonBox->addButton(QStringLiteral("刷新"), QDialogButtonBox::ActionRole);
    buttonBox->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    replayButton_->setEnabled(false);
    connect(refreshButton, &QAbstractButton::clicked, this, &HistoryDialog::reload);
    connect(replayButton_, &QAbstractButton::clicked, this, [this]() {
        openReplayForRow(table_->currentRow());
    });
    connect(table_, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (item != nullptr) {
            openReplayForRow(item->row());
        }
    });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() {
        replayButton_->setEnabled(table_->currentRow() >= 0 && table_->currentRow() < records_.size());
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    reload();
}

QString HistoryDialog::formatDateTime(const QDateTime &time) const
{
    if (!time.isValid()) {
        return QStringLiteral("未知");
    }
    return time.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void HistoryDialog::reload()
{
    applyRecords();
}

void HistoryDialog::applyRecords()
{
    records_ = recordManager_ != nullptr
        ? recordManager_->recentGames(100)
        : QVector<GameRecord>();

    table_->setRowCount(records_.size());
    for (int row = 0; row < records_.size(); ++row) {
        const GameRecord &record = records_.at(row);

        auto setItem = [this, row](int column, const QString &text) {
            auto *item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignCenter);
            table_->setItem(row, column, item);
        };

        setItem(0, formatDateTime(record.endTime));
        setItem(1, record.mode);
        setItem(2, record.blackPlayer);
        setItem(3, record.whitePlayer);
        setItem(4, record.winner);
        setItem(5, QString::number(record.totalMoves));
        if (auto *item = table_->item(row, 0); item != nullptr) {
            item->setData(Qt::UserRole, record.id);
        }
    }

    const bool hasRecords = !records_.isEmpty();
    table_->setVisible(hasRecords);
    emptyLabel_->setVisible(!hasRecords);
    replayButton_->setEnabled(hasRecords);
    if (hasRecords) {
        table_->selectRow(0);
    }
}

void HistoryDialog::openReplayForRow(int row)
{
    if (row < 0 || row >= records_.size() || recordManager_ == nullptr) {
        return;
    }

    const GameRecord &record = records_.at(row);
    const QVector<MoveRecord> moves = recordManager_->movesForGame(record.id);
    ReplayDialog dialog(record, moves, this);
    dialog.exec();
}
