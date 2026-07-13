#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>
#include <functional>

struct IntroPlayerInfo
{
    QString name;
    QString sideText;
    QPixmap avatar;
};

class QLabel;
class QGraphicsOpacityEffect;

class MatchIntroOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit MatchIntroOverlay(QWidget *parent = nullptr);

    void play(const IntroPlayerInfo &leftPlayer,
              const IntroPlayerInfo &rightPlayer,
              std::function<void()> finished = {});

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyPlayer(QLabel *avatarLabel, QLabel *nameLabel, const IntroPlayerInfo &player);
    void layoutAtProgress(qreal progress);

    QLabel *leftAvatarLabel_ = nullptr;
    QLabel *rightAvatarLabel_ = nullptr;
    QLabel *leftNameLabel_ = nullptr;
    QLabel *rightNameLabel_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    QLabel *vsImageLabel_ = nullptr;
    QGraphicsOpacityEffect *opacityEffect_ = nullptr;
    std::function<void()> finished_;
};
