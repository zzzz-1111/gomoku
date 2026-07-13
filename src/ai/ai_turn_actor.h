#pragma once

#include "src/core/turn_actor.h"

class AIEngine;
class ITurnContext;

class AiTurnActor final : public ITurnActor
{
public:
    AiTurnActor();
    ~AiTurnActor() override;

    bool isAutomatic() const override;
    void onTurn(ITurnContext &context, const GameStateSnapshot &state, PieceColor side) override;

    void setDifficulty(AIDifficulty difficulty);
    void setSearchDepth(int depth);

private:
    AIEngine *engine();
    const AIEngine *engine() const;

    struct Impl;
    Impl *impl_ = nullptr;
};
