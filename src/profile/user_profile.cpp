#include "src/profile/user_profile.h"

#include <QBuffer>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

QString customAvatarStoragePath()
{
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        return {};
    }

    QDir dir(baseDir);
    if (!dir.exists(QStringLiteral("avatar")) && !dir.mkpath(QStringLiteral("avatar"))) {
        return {};
    }

    return dir.filePath(QStringLiteral("avatar/custom_avatar.png"));
}

bool saveAvatarCopy(const QString &sourcePath, QString *errorMessage)
{
    const QString targetPath = customAvatarStoragePath();
    if (targetPath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("无法创建头像保存目录");
        }
        return false;
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        if (errorMessage != nullptr) {
            *errorMessage = reader.errorString().isEmpty()
                ? QStringLiteral("读取头像失败")
                : reader.errorString();
        }
        return false;
    }

    if (!image.save(targetPath, "PNG")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("保存头像失败");
        }
        return false;
    }

    return true;
}

bool clearSavedAvatar(QString *errorMessage)
{
    const QString targetPath = customAvatarStoragePath();
    if (targetPath.isEmpty()) {
        return true;
    }

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("删除自定义头像失败");
        }
        return false;
    }
    return true;
}

}

namespace UserProfile {

QString loadOrPromptAccountName(QWidget *parent)
{
    QSettings settings;
    const QString storedName = settings.value(QStringLiteral("user/accountName")).toString().trimmed();
    if (!storedName.isEmpty()) {
        return storedName;
    }

    bool accepted = false;
    const QString input = QInputDialog::getText(parent,
                                                QStringLiteral("登录"),
                                                QStringLiteral("请输入账号名称"),
                                                QLineEdit::Normal,
                                                QString(),
                                                &accepted);
    const QString accountName = accepted && !input.trimmed().isEmpty()
        ? input.trimmed()
        : QStringLiteral("Player");
    settings.setValue(QStringLiteral("user/accountName"), accountName);
    return accountName;
}

QString defaultAvatarResourcePath()
{
    return QStringLiteral(":/assets/avatars/default_avatar.png");
}

QString aiAvatarResourcePath()
{
    return QStringLiteral(":/assets/avatars/ai_avatar.png");
}

QString resolveLocalAvatarPath()
{
    const QString customPath = customAvatarStoragePath();
    if (!customPath.isEmpty() && QFileInfo::exists(customPath)) {
        return customPath;
    }
    return defaultAvatarResourcePath();
}

QPixmap loadAvatarPixmap(const QString &path, const QSize &targetSize)
{
    QPixmap pixmap;
    if (!path.isEmpty()) {
        pixmap.load(path);
    }
    if (pixmap.isNull()) {
        pixmap.load(defaultAvatarResourcePath());
    }
    if (targetSize.isValid() && !targetSize.isEmpty()) {
        pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    return pixmap;
}

QByteArray encodeAvatarForNetwork(const QString &path)
{
    QImage image(path);
    if (image.isNull()) {
        image.load(defaultAvatarResourcePath());
    }
    if (image.isNull()) {
        return {};
    }

    const QImage scaled = image.scaled(QSize(128, 128),
                                       Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "PNG");
    return bytes;
}

QPixmap loadAvatarPixmapFromBytes(const QByteArray &bytes, const QSize &targetSize)
{
    QPixmap pixmap;
    if (!bytes.isEmpty()) {
        pixmap.loadFromData(bytes);
    }
    if (pixmap.isNull()) {
        pixmap.load(defaultAvatarResourcePath());
    }
    if (targetSize.isValid() && !targetSize.isEmpty()) {
        pixmap = pixmap.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    }
    return pixmap;
}

bool showProfileSettingsDialog(QWidget *parent, QString *accountName, QString *avatarPath)
{
    if (parent == nullptr || accountName == nullptr || avatarPath == nullptr) {
        return false;
    }

    const QString originalAccountName = accountName->trimmed();
    const QString originalAvatarPath = *avatarPath;
    QString stagedAvatarSourcePath = QFileInfo::exists(originalAvatarPath) && originalAvatarPath != defaultAvatarResourcePath()
        ? originalAvatarPath
        : QString();
    bool stagedUseDefaultAvatar = stagedAvatarSourcePath.isEmpty();

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("设置"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *titleLabel = new QLabel(QStringLiteral("账号设置"), &dialog);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600; color: #1f3a2b;"));
    layout->addWidget(titleLabel);

    auto *nameRow = new QHBoxLayout;
    auto *nameLabel = new QLabel(QStringLiteral("昵称"), &dialog);
    nameLabel->setMinimumWidth(64);
    auto *nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(QStringLiteral("用于联机对战显示"));
    nameEdit->setText(originalAccountName);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(nameEdit);
    layout->addLayout(nameRow);

    auto *previewLabel = new QLabel(&dialog);
    previewLabel->setFixedSize(120, 120);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet(QStringLiteral(
        "background: rgba(255,255,255,0.70);"
        "border: 1px solid rgba(31,58,43,0.18);"
        "border-radius: 16px;"));
    layout->addWidget(previewLabel, 0, Qt::AlignHCenter);

    auto *pathLabel = new QLabel(&dialog);
    pathLabel->setWordWrap(true);
    pathLabel->setStyleSheet(QStringLiteral("color: #4b5c52;"));
    layout->addWidget(pathLabel);

    auto refreshPreview = [&]() {
        const QString previewPath = stagedUseDefaultAvatar
            ? defaultAvatarResourcePath()
            : stagedAvatarSourcePath;
        previewLabel->setPixmap(loadAvatarPixmap(previewPath, previewLabel->size()));
        if (stagedUseDefaultAvatar) {
            pathLabel->setText(QStringLiteral("当前头像：默认头像"));
        } else {
            pathLabel->setText(QStringLiteral("当前头像：自定义头像"));
        }
    };

    auto *buttonRow = new QHBoxLayout;
    auto *uploadButton = new QPushButton(QStringLiteral("上传头像"), &dialog);
    auto *resetButton = new QPushButton(QStringLiteral("恢复默认"), &dialog);
    buttonRow->addWidget(uploadButton);
    buttonRow->addWidget(resetButton);
    layout->addLayout(buttonRow);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttonBox);

    QObject::connect(uploadButton, &QPushButton::clicked, &dialog, [&]() {
        const QString fileName = QFileDialog::getOpenFileName(&dialog,
                                                              QStringLiteral("选择头像"),
                                                              QString(),
                                                              QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
        if (fileName.isEmpty()) {
            return;
        }

        stagedAvatarSourcePath = fileName;
        stagedUseDefaultAvatar = false;
        refreshPreview();
    });

    QObject::connect(resetButton, &QPushButton::clicked, &dialog, [&]() {
        stagedAvatarSourcePath.clear();
        stagedUseDefaultAvatar = true;
        refreshPreview();
    });

    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    refreshPreview();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString enteredName = nameEdit->text().trimmed();
    if (enteredName.isEmpty()) {
        QMessageBox::warning(parent, QStringLiteral("保存失败"), QStringLiteral("昵称不能为空"));
        return false;
    }

    const bool nameChanged = enteredName != originalAccountName;
    bool avatarChanged = false;

    if (stagedUseDefaultAvatar) {
        if (originalAvatarPath != defaultAvatarResourcePath()) {
            QString errorMessage;
            if (!clearSavedAvatar(&errorMessage)) {
                QMessageBox::warning(parent, QStringLiteral("保存失败"), errorMessage);
                return false;
            }
            *avatarPath = defaultAvatarResourcePath();
            avatarChanged = true;
        }
    } else if (stagedAvatarSourcePath != originalAvatarPath) {
        QString errorMessage;
        if (!saveAvatarCopy(stagedAvatarSourcePath, &errorMessage)) {
            QMessageBox::warning(parent, QStringLiteral("保存失败"), errorMessage);
            return false;
        }
        *avatarPath = customAvatarStoragePath();
        avatarChanged = true;
    }

    if (nameChanged) {
        *accountName = enteredName;
        QSettings settings;
        settings.setValue(QStringLiteral("user/accountName"), *accountName);
    }

    return nameChanged || avatarChanged;
}

}
