#include "src/ui/match_intro_overlay.h"

#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSequentialAnimationGroup>
#include <QTimer>

namespace {

constexpr int kAvatarSize = 112;
constexpr int kNameWidth = 210;
constexpr int kNameHeight = 34;
constexpr int kCenterGap = 28;
constexpr int kVsImageSize = 118;

QPixmap scaledAvatar(const QPixmap &source)
{
    if (source.isNull()) {
        return {};
    }
    return source.scaled(QSize(kAvatarSize, kAvatarSize),
                         Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);
}

}

MatchIntroOverlay::MatchIntroOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral(
        "MatchIntroOverlay {"
        "background: rgba(12, 22, 16, 178);"
        "}"
        "QLabel {"
        "color: #f7fbf4;"
        "background: transparent;"
        "}"
        "QLabel#introTitleLabel {"
        "font-family: \"华文行楷\";"
        "font-size: 34px;"
        "font-weight: 500;"
        "}"
        "QLabel#introNameLabel {"
        "font-size: 15px;"
        "font-weight: 600;"
        "}"));

    opacityEffect_ = new QGraphicsOpacityEffect(this);
    opacityEffect_->setOpacity(0.0);
    setGraphicsEffect(opacityEffect_);

    leftAvatarLabel_ = new QLabel(this);
    rightAvatarLabel_ = new QLabel(this);
    leftNameLabel_ = new QLabel(this);
    rightNameLabel_ = new QLabel(this);
    titleLabel_ = new QLabel(QStringLiteral("对局开始"), this);
    vsImageLabel_ = new QLabel(this);

    leftNameLabel_->setObjectName(QStringLiteral("introNameLabel"));
    rightNameLabel_->setObjectName(QStringLiteral("introNameLabel"));
    titleLabel_->setObjectName(QStringLiteral("introTitleLabel"));

    for (QLabel *avatarLabel : {leftAvatarLabel_, rightAvatarLabel_}) {
        avatarLabel->setFixedSize(kAvatarSize, kAvatarSize);
        avatarLabel->setAlignment(Qt::AlignCenter);
        avatarLabel->setScaledContents(true);
        avatarLabel->setStyleSheet(QStringLiteral(
            "border: 2px solid rgba(255,255,255,0.82);"
            "border-radius: 18px;"
            "background: rgba(255,255,255,0.18);"));
    }

    for (QLabel *nameLabel : {leftNameLabel_, rightNameLabel_}) {
        nameLabel->setFixedSize(kNameWidth, kNameHeight);
        nameLabel->setAlignment(Qt::AlignCenter);
    }

    titleLabel_->setFixedSize(220, 48);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->hide();

    vsImageLabel_->setFixedSize(kVsImageSize, kVsImageSize);
    vsImageLabel_->setAlignment(Qt::AlignCenter);
    const QPixmap vsPixmap(QStringLiteral(":/assets/backgrounds/vs.png"));
    if (vsPixmap.isNull()) {
        vsImageLabel_->setText(QStringLiteral("VS"));
        vsImageLabel_->setStyleSheet(QStringLiteral("font-size: 40px; font-weight: 800; color: #fff1b8;"));
    } else {
        vsImageLabel_->setPixmap(vsPixmap.scaled(vsImageLabel_->size(),
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
    }
    vsImageLabel_->hide();
}

void MatchIntroOverlay::play(const IntroPlayerInfo &leftPlayer,
                             const IntroPlayerInfo &rightPlayer,
                             std::function<void()> finished)
{
    finished_ = std::move(finished);
    applyPlayer(leftAvatarLabel_, leftNameLabel_, leftPlayer);
    applyPlayer(rightAvatarLabel_, rightNameLabel_, rightPlayer);

    if (parentWidget() != nullptr) {
        setGeometry(parentWidget()->rect());
    }
    raise();
    show();
    titleLabel_->hide();
    vsImageLabel_->hide();
    layoutAtProgress(0.0);

    auto *fadeIn = new QPropertyAnimation(opacityEffect_, "opacity", this);
    fadeIn->setDuration(180);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    const QRect leftStart = leftAvatarLabel_->geometry();
    const QRect rightStart = rightAvatarLabel_->geometry();
    layoutAtProgress(1.0);
    const QRect leftEnd = leftAvatarLabel_->geometry();
    const QRect rightEnd = rightAvatarLabel_->geometry();
    leftAvatarLabel_->setGeometry(leftStart);
    rightAvatarLabel_->setGeometry(rightStart);
    leftNameLabel_->move(leftStart.center().x() - kNameWidth / 2, leftStart.bottom() + 12);
    rightNameLabel_->move(rightStart.center().x() - kNameWidth / 2, rightStart.bottom() + 12);

    auto *slide = new QParallelAnimationGroup(this);
    auto *leftMove = new QPropertyAnimation(leftAvatarLabel_, "geometry", slide);
    leftMove->setDuration(580);
    leftMove->setStartValue(leftStart);
    leftMove->setEndValue(leftEnd);
    leftMove->setEasingCurve(QEasingCurve::OutCubic);
    auto *rightMove = new QPropertyAnimation(rightAvatarLabel_, "geometry", slide);
    rightMove->setDuration(580);
    rightMove->setStartValue(rightStart);
    rightMove->setEndValue(rightEnd);
    rightMove->setEasingCurve(QEasingCurve::OutCubic);
    slide->addAnimation(leftMove);
    slide->addAnimation(rightMove);

    auto updateNamePositions = [this]() {
        leftNameLabel_->move(leftAvatarLabel_->geometry().center().x() - kNameWidth / 2,
                             leftAvatarLabel_->geometry().bottom() + 12);
        rightNameLabel_->move(rightAvatarLabel_->geometry().center().x() - kNameWidth / 2,
                              rightAvatarLabel_->geometry().bottom() + 12);
    };
    connect(leftMove, &QPropertyAnimation::valueChanged, this, updateNamePositions);
    connect(rightMove, &QPropertyAnimation::valueChanged, this, updateNamePositions);

    auto *flash = new QPropertyAnimation(vsImageLabel_, "geometry", this);
    const QRect vsRect(width() / 2 - kVsImageSize / 2,
                       height() / 2 - kVsImageSize / 2,
                       kVsImageSize,
                       kVsImageSize);
    flash->setDuration(300);
    flash->setStartValue(vsRect.adjusted(16, 16, -16, -16));
    flash->setEndValue(vsRect);
    flash->setEasingCurve(QEasingCurve::OutBack);

    auto *fadeOut = new QPropertyAnimation(opacityEffect_, "opacity", this);
    fadeOut->setDuration(280);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    auto *sequence = new QSequentialAnimationGroup(this);
    sequence->addAnimation(fadeIn);
    sequence->addAnimation(slide);
    sequence->addPause(120);
    sequence->addAnimation(flash);
    sequence->addPause(120);
    sequence->addAnimation(fadeOut);

    connect(flash, &QPropertyAnimation::stateChanged, this, [this](QAbstractAnimation::State state) {
        if (state == QAbstractAnimation::Running) {
            titleLabel_->show();
            vsImageLabel_->show();
            titleLabel_->raise();
            vsImageLabel_->raise();
        }
    });
    connect(sequence, &QSequentialAnimationGroup::finished, this, [this]() {
        if (finished_) {
            finished_();
        }
        close();
    });

    sequence->start(QAbstractAnimation::DeleteWhenStopped);
}

void MatchIntroOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (isVisible()) {
        layoutAtProgress(1.0);
    }
}

void MatchIntroOverlay::applyPlayer(QLabel *avatarLabel,
                                    QLabel *nameLabel,
                                    const IntroPlayerInfo &player)
{
    avatarLabel->setPixmap(scaledAvatar(player.avatar));
    nameLabel->setText(QStringLiteral("%1  %2").arg(player.sideText, player.name));
}

void MatchIntroOverlay::layoutAtProgress(qreal progress)
{
    const int y = height() / 2 - kAvatarSize / 2 - 12;
    const int centerX = width() / 2;
    const int leftEndX = centerX - kCenterGap - kAvatarSize;
    const int rightEndX = centerX + kCenterGap;
    const int leftStartX = -kAvatarSize - 24;
    const int rightStartX = width() + 24;

    const int leftX = leftStartX + static_cast<int>((leftEndX - leftStartX) * progress);
    const int rightX = rightStartX + static_cast<int>((rightEndX - rightStartX) * progress);

    leftAvatarLabel_->setGeometry(leftX, y, kAvatarSize, kAvatarSize);
    rightAvatarLabel_->setGeometry(rightX, y, kAvatarSize, kAvatarSize);
    leftNameLabel_->move(leftX + kAvatarSize / 2 - kNameWidth / 2, y + kAvatarSize + 12);
    rightNameLabel_->move(rightX + kAvatarSize / 2 - kNameWidth / 2, y + kAvatarSize + 12);
    titleLabel_->move(width() / 2 - titleLabel_->width() / 2, y - 64);
    vsImageLabel_->move(width() / 2 - vsImageLabel_->width() / 2,
                        y + kAvatarSize / 2 - vsImageLabel_->height() / 2);
}
