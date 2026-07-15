#include "database_manager.h"

#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>

namespace {

QString defaultDatabasePath()
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::currentPath();
    }

    QDir dir(baseDir);
    if (!dir.exists() && !QDir().mkpath(baseDir)) {
        return {};
    }

    return dir.filePath(QStringLiteral("gomoku_history.sqlite"));
}

void enableForeignKeys(QSqlDatabase &database)
{
    if (!database.isOpen()) {
        return;
    }

    QSqlQuery pragma(database);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
}

}

DatabaseManager &DatabaseManager::instance()
{
    static DatabaseManager manager;
    return manager;
}

bool DatabaseManager::open(const QString &filePath)
{
    if (database_.isOpen()) {
        return true;
    }

    const QString path = filePath.isEmpty() ? defaultDatabasePath() : filePath;
    if (path.isEmpty()) {
        qWarning() << "Failed to resolve database path";
        return false;
    }

    if (!database_.isValid()) {
        database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                              QStringLiteral("gomoku_history_connection"));
    }

    database_.setDatabaseName(path);
    if (!database_.open()) {
        qWarning().noquote() << "Failed to open history database:" << database_.lastError().text();
        return false;
    }

    enableForeignKeys(database_);
    return true;
}

void DatabaseManager::close()
{
    if (database_.isOpen()) {
        database_.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return database_.isOpen();
}

bool DatabaseManager::ensureSchema()
{
    if (!database_.isOpen() && !open(QString())) {
        return false;
    }

    QSqlQuery query(database_);

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "name TEXT NOT NULL UNIQUE,"
            "total_games INTEGER NOT NULL DEFAULT 0,"
            "wins INTEGER NOT NULL DEFAULT 0,"
            "losses INTEGER NOT NULL DEFAULT 0,"
            "draws INTEGER NOT NULL DEFAULT 0,"
            "created_at TEXT NOT NULL)"))) {
        qWarning().noquote() << "Create users table failed:" << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS games ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "mode TEXT NOT NULL,"
            "black_player TEXT NOT NULL,"
            "white_player TEXT NOT NULL,"
            "winner TEXT NOT NULL,"
            "total_moves INTEGER NOT NULL DEFAULT 0,"
            "start_time TEXT NOT NULL,"
            "end_time TEXT NOT NULL)"))) {
        qWarning().noquote() << "Create games table failed:" << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS moves ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "game_id INTEGER NOT NULL,"
            "step_number INTEGER NOT NULL,"
            "player_color INTEGER NOT NULL,"
            "x INTEGER NOT NULL,"
            "y INTEGER NOT NULL,"
            "FOREIGN KEY(game_id) REFERENCES games(id) ON DELETE CASCADE)"))) {
        qWarning().noquote() << "Create moves table failed:" << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_games_end_time ON games(end_time DESC, id DESC)"))) {
        qWarning().noquote() << "Create games index failed:" << query.lastError().text();
        return false;
    }

    if (!query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_moves_game_id ON moves(game_id, step_number)"))) {
        qWarning().noquote() << "Create moves index failed:" << query.lastError().text();
        return false;
    }

    return true;
}

QSqlDatabase DatabaseManager::database() const
{
    return database_;
}
