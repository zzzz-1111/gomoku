#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>
#include <QString>
#include <functional>

#include "src/common/gomoku_types.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ChessBoardWidget;
class GameController;
class GameServer;
class MatchIntroOverlay;
class NetworkManager;
class QTcpSocket;
struct IntroPlayerInfo;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void startGame(GameMode mode, PlayerSide humanSide = PlayerSide::Black, AIDifficulty aiDifficulty = AIDifficulty::Normal);
    void prepareOnlineHostWaiting();
    void setOnlineGameActive(bool active);
    void updateBoardMoveInput();
    bool hostMayPlaceStone() const;
    void configureTurnActors(GameMode mode, PlayerSide humanSide, AIDifficulty aiDifficulty);
    void setupHomePage();
    void setupGamePage();
    void setupNetworkPage();
    bool showAiSettingsDialog(PlayerSide *humanSide, AIDifficulty *aiDifficulty);
    void refreshGameInfo();
    void refreshDiscoveredRooms();
    void syncBoardFromController();
    void refreshOnlineIdentityLabels();
    void playMatchIntro(const IntroPlayerInfo &leftPlayer,
                        const IntroPlayerInfo &rightPlayer,
                        std::function<void()> finished = {});
    void playAiMatchIntro();
    void playOnlineMatchIntro();
    void playOnlineMatchIntroPreview();
    void showGameOverPrompt(const QString &title, const QString &message);
    void showPendingOnlineHostGameOverPrompt();
    QString modeText(GameMode mode) const;
    QString playerText(PieceColor color) const;

    Ui::MainWindow *ui = nullptr;
    ChessBoardWidget *boardWidget_ = nullptr;
    GameController *controller_ = nullptr;
    NetworkManager *networkManager_ = nullptr;
    GameServer *gameServer_ = nullptr;
    MatchIntroOverlay *matchIntroOverlay_ = nullptr;
    GameMode currentMode_ = GameMode::LocalTwoPlayer;
    PlayerSide humanSide_ = PlayerSide::Black;
    QString localAccountName_;
    QString localAvatarPath_;
    QString remoteAccountName_;
    QByteArray remoteAvatarBytes_;
    bool applyingNetworkMove_ = false;
    bool onlineGameStarted_ = false;
    bool hostPlayerJoined_ = false;
    bool pendingOnlineHostGameOverPrompt_ = false;
    QString pendingGameOverTitle_;
    QString pendingGameOverMessage_;
};

#endif // MAINWINDOW_H
