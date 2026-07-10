#pragma once

enum class PieceColor
{
    Empty,
    Black,
    White
};

enum class GameMode
{
    LocalTwoPlayer,
    HumanVsAI,
    OnlineHost,
    OnlineClient,
    Replay
};

enum class PlayerSide
{
    Black,
    White
};

enum class AIStyle
{
    Attack,
    Defense,
    Balanced
};

enum class AIDifficulty
{
    Easy,
    Normal,
    Hard
};

struct BoardPosition
{
    int x = -1;
    int y = -1;

    bool isValid() const
    {
        return x >= 0 && y >= 0;
    }
};

struct MoveInfo
{
    BoardPosition position;
    PieceColor color = PieceColor::Empty;
    int stepNumber = 0;
    int score = 0;
};

struct GameSettings
{
    GameMode mode = GameMode::LocalTwoPlayer;
    PlayerSide humanSide = PlayerSide::Black;
    AIDifficulty aiDifficulty = AIDifficulty::Normal;
    AIStyle aiStyle = AIStyle::Balanced;
    bool showSuggestion = true;
    bool enableSound = true;
    int boardSize = 15;
};
