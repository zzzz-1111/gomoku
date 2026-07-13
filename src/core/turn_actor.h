#pragma once

#include "src/common/gomoku_types.h"
#include "src/core/game_state.h"

class GameController;

class ITurnActor
{
public:
    virtual ~ITurnActor() = default;

    virtual bool isAutomatic() const = 0;
    virtual void onTurn(GameController &controller,
                        const GameStateSnapshot &state,
                        PieceColor side) = 0;
};
