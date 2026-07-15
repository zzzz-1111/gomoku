#pragma once

#include <QByteArray>
#include <QPixmap>
#include <QString>

class QSize;
class QWidget;

namespace UserProfile {

QString loadOrPromptAccountName(QWidget *parent);
QString defaultAvatarResourcePath();
QString aiAvatarResourcePath();
QString resolveLocalAvatarPath();
QPixmap loadAvatarPixmap(const QString &path, const QSize &targetSize);
QPixmap loadAvatarPixmapFromBytes(const QByteArray &bytes, const QSize &targetSize);
QByteArray encodeAvatarForNetwork(const QString &path);
bool showProfileSettingsDialog(QWidget *parent, QString *accountName, QString *avatarPath);

}
