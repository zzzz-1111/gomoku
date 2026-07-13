#include "ai_turn_actor.h"

#include "ai_engine.h"
#include "src/core/game_controller.h"

struct AiTurnActor::Impl
{
    AIEngine engine;
};

AiTurnActor::AiTurnActor()
    : impl_(new Impl)
{
}

AiTurnActor::~AiTurnActor()
{
    delete impl_;
}

bool AiTurnActor::isAutomatic() const
{
    return true;
}

void AiTurnActor::onTurn(GameController &controller, const GameStateSnapshot &state, PieceColor side)
{
    if (state.gameOver) {
        return;
    }

    const MoveInfo move = impl_->engine.chooseMove(state.board, side);
    if (!move.position.isValid()) {
        return;
    }

    controller.placeStone(move.position.x, move.position.y);
}

void AiTurnActor::setDifficulty(AIDifficulty difficulty)
{
    impl_->engine.setDifficulty(difficulty);
}

void AiTurnActor::setSearchDepth(int depth)
{
    impl_->engine.setSearchDepth(depth);
}

AIEngine *AiTurnActor::engine()
{
    return &impl_->engine;
}

const AIEngine *AiTurnActor::engine() const
{
    return &impl_->engine;
}
