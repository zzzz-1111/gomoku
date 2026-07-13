#pragma once

#include "src/common/gomoku_types.h"
#include "src/core/game_state.h"

class ITurnContext;

class ITurnActor
{
public:
    virtual ~ITurnActor() = default;

    virtual bool isAutomatic() const = 0;
    virtual void onTurn(ITurnContext &context,
                        const GameStateSnapshot &state,
                        PieceColor side) = 0;
};
