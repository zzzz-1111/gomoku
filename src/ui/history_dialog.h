#pragma once

#include <QDialog>
#include <QDateTime>

class QLabel;
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

    RecordManager *recordManager_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QTableWidget *table_ = nullptr;
};
