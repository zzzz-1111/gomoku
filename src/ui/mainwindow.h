#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

#include "src/common/gomoku_types.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ChessBoardWidget;
class GameController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void startGame(GameMode mode, PlayerSide humanSide = PlayerSide::Black, AIDifficulty aiDifficulty = AIDifficulty::Normal);
    void configureTurnActors(GameMode mode, PlayerSide humanSide, AIDifficulty aiDifficulty);
    void setupHomePage();
    void setupGamePage();
    bool showAiSettingsDialog(PlayerSide *humanSide, AIDifficulty *aiDifficulty);
    void refreshGameInfo();
    void syncBoardFromController();
    void showGameOverPrompt(const QString &title, const QString &message);
    QString modeText(GameMode mode) const;
    QString playerText(PieceColor color) const;

    Ui::MainWindow *ui = nullptr;
    ChessBoardWidget *boardWidget_ = nullptr;
    GameController *controller_ = nullptr;
    GameMode currentMode_ = GameMode::LocalTwoPlayer;
    PlayerSide humanSide_ = PlayerSide::Black;
};

#endif // MAINWINDOW_H
