#pragma once

#include <QDialog>
#include <QDateTime>
#include <QVector>

#include "src/data/models.h"

class QLabel;
class QPushButton;
class QTableWidget;
class RecordManager;

class HistoryDialog : public QDialog
{
public:
    explicit HistoryDialog(RecordManager *recordManager, QWidget *parent = nullptr);

private:
    void reload();
    void applyRecords();
    QString formatDateTime(const QDateTime &time) const;
    void openReplayForRow(int row);

    RecordManager *recordManager_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QPushButton *replayButton_ = nullptr;
    QTableWidget *table_ = nullptr;
    QVector<GameRecord> records_;
};
