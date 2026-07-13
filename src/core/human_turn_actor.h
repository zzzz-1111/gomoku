#pragma once

#include "src/core/turn_actor.h"

class HumanTurnActor final : public ITurnActor
{
public:
    bool isAutomatic() const override
    {
        return false;
    }

    void onTurn(ITurnContext &, const GameStateSnapshot &, PieceColor) override
    {
    }
};
