#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
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
class MatchIntroOverlay;
class RecordManager;
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
    void setOnlineGameActive(bool active);
    void updateBoardMoveInput();
    void configureTurnActors(GameMode mode, PlayerSide humanSide, AIDifficulty aiDifficulty);
    void setupHomePage();
    void setupGamePage();
    bool showAiSettingsDialog(PlayerSide *humanSide, AIDifficulty *aiDifficulty);
    void refreshGameInfo();
    void syncBoardFromController();
    void beginActiveMatchRecord();
    void saveCurrentGameResult(PieceColor winner);
    QString displayNameForSide(PlayerSide side) const;
    void showHistoryDialog();
    void playMatchIntro(const IntroPlayerInfo &leftPlayer,
                        const IntroPlayerInfo &rightPlayer,
                        std::function<void()> finished = {});
    void playAiMatchIntro();
    void playOnlineMatchIntro();
    void playOnlineMatchIntroPreview();
    void showGameOverPrompt(const QString &title, const QString &message);
    QString modeText(GameMode mode) const;
    QString playerText(PieceColor color) const;

    Ui::MainWindow *ui = nullptr;
    ChessBoardWidget *boardWidget_ = nullptr;
    GameController *controller_ = nullptr;
    RecordManager *recordManager_ = nullptr;
    NetworkManager *networkManager_ = nullptr;
    MatchIntroOverlay *matchIntroOverlay_ = nullptr;
    GameMode currentMode_ = GameMode::LocalTwoPlayer;
    PlayerSide humanSide_ = PlayerSide::Black;
    QString localAccountName_;
    QString localAvatarPath_;
    QString remoteAccountName_;
    QByteArray remoteAvatarBytes_;
    QDateTime currentGameStartTime_;
    bool currentGameResultRecorded_ = false;
    bool applyingNetworkMove_ = false;
    bool onlineGameStarted_ = false;
};

#endif // MAINWINDOW_H
