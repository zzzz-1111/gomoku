#pragma once

#include <QObject>
#include <QVector>

#include "src/common/gomoku_types.h"
#include "src/data/models.h"

class RecordManager : public QObject
{
    Q_OBJECT

public:
    explicit RecordManager(QObject *parent = nullptr);

    bool saveGame(const GameRecord &game, const QVector<MoveRecord> &moves);
    QVector<GameRecord> recentGames(int limit = 20) const;
    QVector<MoveRecord> movesForGame(int gameId) const;
    bool loadGame(int gameId);

signals:
    void recordSaved(int gameId);
    void recordLoaded(int gameId);
};
