#include "record_manager.h"

#include <algorithm>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>

#include "src/data/database_manager.h"

namespace {

QString normalizedName(const QString &name, const QString &fallback)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? fallback : trimmed;
}

QString dateTimeToString(const QDateTime &time)
{
    const QDateTime normalized = time.isValid() ? time : QDateTime::currentDateTime();
    return normalized.toString(Qt::ISODateWithMs);
}

QDateTime stringToDateTime(const QString &text)
{
    QDateTime time = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!time.isValid()) {
        time = QDateTime::fromString(text, Qt::ISODate);
    }
    return time;
}

QString fallbackHistoryPath()
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::currentPath();
    }

    QDir dir(baseDir);
    if (!dir.exists() && !QDir().mkpath(baseDir)) {
        return {};
    }

    return dir.filePath(QStringLiteral("gomoku_history_fallback.json"));
}

QJsonObject moveToJson(const MoveRecord &move)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), move.id);
    object.insert(QStringLiteral("gameId"), move.gameId);
    object.insert(QStringLiteral("stepNumber"), move.stepNumber);
    object.insert(QStringLiteral("playerColor"), move.playerColor);
    object.insert(QStringLiteral("x"), move.x);
    object.insert(QStringLiteral("y"), move.y);
    object.insert(QStringLiteral("score"), move.score);
    return object;
}

MoveRecord moveFromJson(const QJsonObject &object)
{
    MoveRecord move;
    move.id = object.value(QStringLiteral("id")).toInt();
    move.gameId = object.value(QStringLiteral("gameId")).toInt();
    move.stepNumber = object.value(QStringLiteral("stepNumber")).toInt();
    move.playerColor = object.value(QStringLiteral("playerColor")).toInt();
    move.x = object.value(QStringLiteral("x")).toInt();
    move.y = object.value(QStringLiteral("y")).toInt();
    move.score = object.value(QStringLiteral("score")).toInt();
    return move;
}

QJsonObject gameToJson(const GameRecord &game, const QVector<MoveRecord> &moves)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), game.id);
    object.insert(QStringLiteral("mode"), game.mode);
    object.insert(QStringLiteral("blackPlayer"), game.blackPlayer);
    object.insert(QStringLiteral("whitePlayer"), game.whitePlayer);
    object.insert(QStringLiteral("winner"), game.winner);
    object.insert(QStringLiteral("totalMoves"), game.totalMoves);
    object.insert(QStringLiteral("startTime"), dateTimeToString(game.startTime));
    object.insert(QStringLiteral("endTime"), dateTimeToString(game.endTime));

    QJsonArray moveArray;
    for (const MoveRecord &move : moves) {
        moveArray.append(moveToJson(move));
    }
    object.insert(QStringLiteral("moves"), moveArray);
    return object;
}

bool writeFallbackHistory(const QJsonArray &games)
{
    const QString path = fallbackHistoryPath();
    if (path.isEmpty()) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning().noquote() << "Open fallback history failed:" << file.errorString();
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("games"), games);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning().noquote() << "Commit fallback history failed:" << file.errorString();
        return false;
    }

    return true;
}

QJsonArray loadFallbackGamesRaw()
{
    const QString path = fallbackHistoryPath();
    if (path.isEmpty() || !QFile::exists(path)) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "Open fallback history for read failed:" << file.errorString();
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    return document.object().value(QStringLiteral("games")).toArray();
}

QVector<GameRecord> loadFallbackGames()
{
    QVector<GameRecord> games;
    const QJsonArray array = loadFallbackGamesRaw();
    games.reserve(array.size());

    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        GameRecord game;
        game.id = object.value(QStringLiteral("id")).toInt();
        game.mode = object.value(QStringLiteral("mode")).toString();
        game.blackPlayer = object.value(QStringLiteral("blackPlayer")).toString();
        game.whitePlayer = object.value(QStringLiteral("whitePlayer")).toString();
        game.winner = object.value(QStringLiteral("winner")).toString();
        game.totalMoves = object.value(QStringLiteral("totalMoves")).toInt();
        game.startTime = stringToDateTime(object.value(QStringLiteral("startTime")).toString());
        game.endTime = stringToDateTime(object.value(QStringLiteral("endTime")).toString());
        games.push_back(game);
    }

    return games;
}

QVector<MoveRecord> loadFallbackMovesForGame(int gameId)
{
    QVector<MoveRecord> moves;
    const QJsonArray array = loadFallbackGamesRaw();
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("id")).toInt() != gameId) {
            continue;
        }
        const QJsonArray moveArray = object.value(QStringLiteral("moves")).toArray();
        moves.reserve(moveArray.size());
        for (const QJsonValue &moveValue : moveArray) {
            if (moveValue.isObject()) {
                moves.push_back(moveFromJson(moveValue.toObject()));
            }
        }
        break;
    }
    return moves;
}

bool appendFallbackGame(const GameRecord &game, const QVector<MoveRecord> &moves)
{
    QJsonArray games = loadFallbackGamesRaw();
    int nextId = -1;
    for (const QJsonValue &value : games) {
        if (!value.isObject()) {
            continue;
        }
        const int id = value.toObject().value(QStringLiteral("id")).toInt();
        if (id <= nextId) {
            nextId = id - 1;
        }
    }

    GameRecord stored = game;
    stored.id = nextId;
    QVector<MoveRecord> storedMoves = moves;
    for (MoveRecord &move : storedMoves) {
        move.gameId = stored.id;
    }

    games.append(gameToJson(stored, storedMoves));
    if (!writeFallbackHistory(games)) {
        return false;
    }
    return true;
}

QVector<GameRecord> sortAndTrimGames(QVector<GameRecord> games, int limit)
{
    std::sort(games.begin(), games.end(), [](const GameRecord &lhs, const GameRecord &rhs) {
        if (lhs.endTime != rhs.endTime) {
            return lhs.endTime > rhs.endTime;
        }
        return lhs.id > rhs.id;
    });
    if (limit > 0 && games.size() > limit) {
        games.resize(limit);
    }
    return games;
}

bool updateUserStats(QSqlDatabase database,
                     const QString &name,
                     int totalDelta,
                     int winDelta,
                     int lossDelta,
                     int drawDelta)
{
    const QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        return true;
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT id FROM users WHERE name = ?"));
    query.addBindValue(normalized);
    if (!query.exec()) {
        qWarning().noquote() << "Select user failed:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        const int userId = query.value(0).toInt();
        QSqlQuery update(database);
        update.prepare(QStringLiteral(
            "UPDATE users "
            "SET total_games = total_games + ?, "
            "wins = wins + ?, "
            "losses = losses + ?, "
            "draws = draws + ? "
            "WHERE id = ?"));
        update.addBindValue(totalDelta);
        update.addBindValue(winDelta);
        update.addBindValue(lossDelta);
        update.addBindValue(drawDelta);
        update.addBindValue(userId);
        if (!update.exec()) {
            qWarning().noquote() << "Update user failed:" << update.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO users (name, total_games, wins, losses, draws, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(normalized);
    insert.addBindValue(totalDelta);
    insert.addBindValue(winDelta);
    insert.addBindValue(lossDelta);
    insert.addBindValue(drawDelta);
    insert.addBindValue(dateTimeToString(QDateTime::currentDateTime()));
    if (!insert.exec()) {
        qWarning().noquote() << "Insert user failed:" << insert.lastError().text();
        return false;
    }
    return true;
}

bool storeMoves(QSqlDatabase database, qint64 gameId, const QVector<MoveRecord> &moves)
{
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO moves (game_id, step_number, player_color, x, y, score) "
        "VALUES (?, ?, ?, ?, ?, ?)"));

    for (const MoveRecord &move : moves) {
        query.bindValue(0, gameId);
        query.bindValue(1, move.stepNumber);
        query.bindValue(2, move.playerColor);
        query.bindValue(3, move.x);
        query.bindValue(4, move.y);
        query.bindValue(5, move.score);
        if (!query.exec()) {
            qWarning().noquote() << "Insert move failed:" << query.lastError().text();
            return false;
        }
    }

    return true;
}

} // namespace

RecordManager::RecordManager(QObject *parent)
    : QObject(parent)
{
    DatabaseManager::instance().ensureSchema();
}

bool RecordManager::saveGame(const GameRecord &game, const QVector<MoveRecord> &moves)
{
    const QString blackPlayer = normalizedName(game.blackPlayer, QStringLiteral("黑方玩家"));
    const QString whitePlayer = normalizedName(game.whitePlayer, QStringLiteral("白方玩家"));
    const QString winner = normalizedName(game.winner, QStringLiteral("平局"));
    const int totalMoves = game.totalMoves > 0 ? game.totalMoves : moves.size();
    const QString startTime = dateTimeToString(game.startTime);
    const QString endTime = dateTimeToString(game.endTime);

    if (DatabaseManager::instance().ensureSchema()) {
        QSqlDatabase database = DatabaseManager::instance().database();
        if (database.isOpen() && database.transaction()) {
            QSqlQuery insertGame(database);
            insertGame.prepare(QStringLiteral(
                "INSERT INTO games (mode, black_player, white_player, winner, total_moves, start_time, end_time) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)"));
            insertGame.addBindValue(normalizedName(game.mode, QStringLiteral("未知模式")));
            insertGame.addBindValue(blackPlayer);
            insertGame.addBindValue(whitePlayer);
            insertGame.addBindValue(winner);
            insertGame.addBindValue(totalMoves);
            insertGame.addBindValue(startTime);
            insertGame.addBindValue(endTime);

            bool dbSaved = false;
            if (insertGame.exec()) {
                const qint64 gameId = insertGame.lastInsertId().toLongLong();
                if (gameId > 0
                    && storeMoves(database, gameId, moves)
                    && updateUserStats(database, blackPlayer, 1, winner == blackPlayer ? 1 : 0, winner == whitePlayer ? 1 : 0, winner == QStringLiteral("平局") ? 1 : 0)
                    && updateUserStats(database, whitePlayer, 1, winner == whitePlayer ? 1 : 0, winner == blackPlayer ? 1 : 0, winner == QStringLiteral("平局") ? 1 : 0)
                    && database.commit()) {
                    dbSaved = true;
                    emit recordSaved(static_cast<int>(gameId));
                } else {
                    qWarning().noquote() << "Database save failed, falling back to file";
                    database.rollback();
                }
            } else {
                qWarning().noquote() << "Insert game failed:" << insertGame.lastError().text();
                database.rollback();
            }

            if (dbSaved) {
                return true;
            }
        } else if (!database.isOpen()) {
            qWarning().noquote() << "History database is not open, falling back to file";
        } else {
            qWarning().noquote() << "Begin transaction failed, falling back to file:" << database.lastError().text();
        }
    }

    if (appendFallbackGame(game, moves)) {
        const QJsonArray fallbackGames = loadFallbackGamesRaw();
        int savedId = -1;
        for (const QJsonValue &value : fallbackGames) {
            if (!value.isObject()) {
                continue;
            }
            const QJsonObject object = value.toObject();
            if (object.value(QStringLiteral("mode")).toString() == game.mode
                && object.value(QStringLiteral("blackPlayer")).toString() == blackPlayer
                && object.value(QStringLiteral("whitePlayer")).toString() == whitePlayer
                && object.value(QStringLiteral("winner")).toString() == winner
                && object.value(QStringLiteral("totalMoves")).toInt() == totalMoves
                && object.value(QStringLiteral("startTime")).toString() == startTime
                && object.value(QStringLiteral("endTime")).toString() == endTime) {
                savedId = object.value(QStringLiteral("id")).toInt();
            }
        }
        emit recordSaved(savedId);
        return true;
    }

    return false;
}

QVector<GameRecord> RecordManager::recentGames(int limit) const
{
    if (limit <= 0) {
        limit = 20;
    }

    QVector<GameRecord> games;

    if (DatabaseManager::instance().ensureSchema()) {
        QSqlDatabase database = DatabaseManager::instance().database();
        if (database.isOpen()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT id, mode, black_player, white_player, winner, total_moves, start_time, end_time "
                "FROM games ORDER BY datetime(end_time) DESC, id DESC LIMIT ?"));
            query.addBindValue(limit);
            if (query.exec()) {
                while (query.next()) {
                    GameRecord game;
                    game.id = query.value(0).toInt();
                    game.mode = query.value(1).toString();
                    game.blackPlayer = query.value(2).toString();
                    game.whitePlayer = query.value(3).toString();
                    game.winner = query.value(4).toString();
                    game.totalMoves = query.value(5).toInt();
                    game.startTime = stringToDateTime(query.value(6).toString());
                    game.endTime = stringToDateTime(query.value(7).toString());
                    games.push_back(game);
                }
            } else {
                qWarning().noquote() << "Query recent games failed:" << query.lastError().text();
            }
        }
    }

    QVector<GameRecord> fallbackGames = loadFallbackGames();
    games.append(fallbackGames);
    return sortAndTrimGames(std::move(games), limit);
}

QVector<MoveRecord> RecordManager::movesForGame(int gameId) const
{
    QVector<MoveRecord> moves;
    if (gameId == 0) {
        return moves;
    }

    if (DatabaseManager::instance().ensureSchema()) {
        QSqlDatabase database = DatabaseManager::instance().database();
        if (database.isOpen()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral(
                "SELECT id, game_id, step_number, player_color, x, y, score "
                "FROM moves WHERE game_id = ? ORDER BY step_number ASC, id ASC"));
            query.addBindValue(gameId);
            if (query.exec()) {
                while (query.next()) {
                    MoveRecord move;
                    move.id = query.value(0).toInt();
                    move.gameId = query.value(1).toInt();
                    move.stepNumber = query.value(2).toInt();
                    move.playerColor = query.value(3).toInt();
                    move.x = query.value(4).toInt();
                    move.y = query.value(5).toInt();
                    move.score = query.value(6).toInt();
                    moves.push_back(move);
                }
            } else {
                qWarning().noquote() << "Query game moves failed:" << query.lastError().text();
            }
        }
    }

    if (moves.isEmpty()) {
        moves = loadFallbackMovesForGame(gameId);
    }

    return moves;
}

bool RecordManager::loadGame(int gameId)
{
    if (gameId == 0) {
        return false;
    }

    bool found = false;

    if (DatabaseManager::instance().ensureSchema()) {
        QSqlDatabase database = DatabaseManager::instance().database();
        if (database.isOpen()) {
            QSqlQuery query(database);
            query.prepare(QStringLiteral("SELECT id FROM games WHERE id = ?"));
            query.addBindValue(gameId);
            if (query.exec() && query.next()) {
                found = true;
            }
        }
    }

    if (!found) {
        const QVector<GameRecord> fallbackGames = loadFallbackGames();
        for (const GameRecord &game : fallbackGames) {
            if (game.id == gameId) {
                found = true;
                break;
            }
        }
    }

    if (found) {
        emit recordLoaded(gameId);
    }
    return found;
}
