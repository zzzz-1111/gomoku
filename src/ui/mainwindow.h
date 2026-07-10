#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    void setupHomePage();
    void setupGamePage();
    void refreshGameInfo();
    void syncBoardFromController();
    void showGameOverPrompt(const QString &title, const QString &message);

    Ui::MainWindow *ui = nullptr;
    ChessBoardWidget *boardWidget_ = nullptr;
    GameController *controller_ = nullptr;
};

#endif // MAINWINDOW_H
