#pragma once

#include <QtGlobal>

#include "src/common/gomoku_types.h"

class ITurnContext
{
public:
    virtual ~ITurnContext() = default;

    virtual quint64 stateRevision() const = 0;
    virtual PieceColor currentPlayer() const = 0;
    virtual bool placeStone(int x, int y) = 0;
};
