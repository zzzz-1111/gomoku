#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    static DatabaseManager &instance();

    bool open(const QString &filePath);
    void close();
    bool isOpen() const;
    bool ensureSchema();

    QSqlDatabase database() const;

private:
    DatabaseManager() = default;
    Q_DISABLE_COPY_MOVE(DatabaseManager)

    QSqlDatabase database_;
};
