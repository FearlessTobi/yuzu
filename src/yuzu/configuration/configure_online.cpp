// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <QBuffer>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QIcon>
#include <QInputDialog>
#include <QMessageBox>

#include <QtConcurrent/QtConcurrentRun>

#include <httplib.h>

#include "common/logging/log.h"
#include "core/online_initiator.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "ui_configure_online.h"
#include "web_service/web_backend.h"
#include "web_service/web_result.h"
#include "yuzu/configuration/configure_online.h"
#include "yuzu/online/friends.h"
#include "yuzu/online/monitor.h"
#include "yuzu/online/online_util.h"
#include "yuzu/uisettings.h"

namespace {

constexpr int AVATAR_MIN_SIZE = 256;
constexpr char TOKEN_DELIMITER = ':';

[[nodiscard]] std::string GenerateDisplayToken(const std::string& username,
                                               const std::string& token) {
    if (username.empty() || token.empty()) {
        return {};
    }

    const std::string unencoded_display_token{username + TOKEN_DELIMITER + token};
    QByteArray b{unencoded_display_token.c_str()};
    QByteArray b64 = b.toBase64();
    return b64.toStdString();
}

[[nodiscard]] std::string UsernameFromDisplayToken(const std::string& display_token) {
    const std::string unencoded_display_token{
        QByteArray::fromBase64(display_token.c_str()).toStdString()};
    return unencoded_display_token.substr(0, unencoded_display_token.find(TOKEN_DELIMITER));
}

[[nodiscard]] std::string TokenFromDisplayToken(const std::string& display_token) {
    const std::string unencoded_display_token{
        QByteArray::fromBase64(display_token.c_str()).toStdString()};
    return unencoded_display_token.substr(unencoded_display_token.find(TOKEN_DELIMITER) + 1);
}

[[nodiscard]] std::optional<httplib::Headers> AuthorizationHeaders(
    Core::OnlineInitiator* online_initiator) {
    const std::optional id_token = online_initiator->LoadIdTokenApp("profile");
    if (!id_token) {
        return std::nullopt;
    }
    return httplib::Headers{
        {"Authorization", "Bearer " + id_token->token},
    };
}

} // Anonymous namespace

ConfigureOnline::ConfigureOnline(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureOnline>()), verify_watcher(this),
      online_username_watcher(this), online_avatar_watcher(this), upload_username_watcher(this),
      upload_avatar_watcher(this) {
    ui->setupUi(this);
}

ConfigureOnline::~ConfigureOnline() {
    // Qt doesn't delay destruction until watchers finish, we have to do it ourselves
    verify_watcher.waitForFinished();
    online_username_watcher.waitForFinished();
    online_avatar_watcher.waitForFinished();
    upload_username_watcher.waitForFinished();
    upload_avatar_watcher.waitForFinished();
}

void ConfigureOnline::Initialize(Core::OnlineInitiator& online_initiator_,
                                 OnlineStatusMonitor* online_status_monitor_,
                                 FriendsList* friend_list_) {
    online_initiator = &online_initiator_;
    online_status_monitor = online_status_monitor_;
    friend_list = friend_list_;

    profile_scene = new QGraphicsScene;
    ui->online_profile_image->setScene(profile_scene);

    ui->button_set_username->setEnabled(false);
    ui->button_set_avatar->setEnabled(false);

    connect(ui->button_regenerate_telemetry_id, &QPushButton::clicked, this,
            &ConfigureOnline::RefreshTelemetryID);
    connect(ui->button_verify_login, &QPushButton::clicked, this, &ConfigureOnline::VerifyLogin);
    connect(ui->button_set_username, &QPushButton::clicked, this, &ConfigureOnline::SetUserName);
    connect(ui->button_set_avatar, &QPushButton::clicked, this, &ConfigureOnline::SetAvatar);
    connect(&verify_watcher, &QFutureWatcher<bool>::finished, this,
            &ConfigureOnline::OnLoginVerified);
    connect(&online_username_watcher, &QFutureWatcher<std::optional<std::string>>::finished, this,
            &ConfigureOnline::OnOnlineUserNameRefreshed);
    connect(&online_avatar_watcher, &QFutureWatcher<QImage>::finished, this,
            &ConfigureOnline::OnOnlineAvatarRefreshed);
    connect(&upload_username_watcher, &QFutureWatcher<int>::finished, this,
            &ConfigureOnline::OnUserNameUploaded);
    connect(&upload_avatar_watcher, &QFutureWatcher<int>::finished, this,
            &ConfigureOnline::OnAvatarUploaded);

#ifndef USE_DISCORD_PRESENCE
    ui->discord_group->setVisible(false);
#endif

    SetConfiguration();
    RetranslateUI();
    RefreshOnlineUserName();
    RefreshOnlineAvatar();
}

void ConfigureOnline::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureOnline::RetranslateUI() {
    ui->retranslateUi(this);

    ui->telemetry_learn_more->setText(
        tr("<a href='https://yuzu-emu.org/help/feature/telemetry/'><span style=\"text-decoration: "
           "underline; color:#039be5;\">Learn more</span></a>"));

    ui->web_signup_link->setText(
        tr("<a href='https://profile.yuzu-emu.org/'><span style=\"text-decoration: underline; "
           "color:#039be5;\">Sign up</span></a>"));

    ui->web_token_info_link->setText(
        tr("<a href='https://yuzu-emu.org/wiki/yuzu-web-service/'><span style=\"text-decoration: "
           "underline; color:#039be5;\">What is my token?</span></a>"));

    ui->label_telemetry_id->setText(
        tr("Telemetry ID: 0x%1").arg(QString::number(Core::GetTelemetryId(), 16).toUpper()));
}

void ConfigureOnline::SetConfiguration() {
    ui->web_credentials_disclaimer->setWordWrap(true);

    ui->telemetry_learn_more->setOpenExternalLinks(true);
    ui->web_signup_link->setOpenExternalLinks(true);
    ui->web_token_info_link->setOpenExternalLinks(true);

    if (Settings::values.yuzu_username.empty()) {
        ui->username->setText(tr("Unspecified"));
    } else {
        ui->username->setText(QString::fromStdString(Settings::values.yuzu_username));
    }

    ui->toggle_telemetry->setChecked(Settings::values.enable_telemetry);
    ui->edit_token->setText(QString::fromStdString(
        GenerateDisplayToken(Settings::values.yuzu_username, Settings::values.yuzu_token)));

    // Connect after setting the values, to avoid calling OnLoginChanged now
    connect(ui->edit_token, &QLineEdit::textChanged, this, &ConfigureOnline::OnLoginChanged);

    user_verified = true;

    ui->toggle_discordrpc->setChecked(UISettings::values.enable_discord_presence);
}

void ConfigureOnline::ApplyConfiguration() {
    Settings::values.enable_telemetry = ui->toggle_telemetry->isChecked();
    UISettings::values.enable_discord_presence = ui->toggle_discordrpc->isChecked();
    if (user_verified) {
        Settings::values.yuzu_username = LocalYuzuUsername();
        Settings::values.yuzu_token = LocalYuzuToken();
    } else {
        QMessageBox::warning(
            this, tr("Token not verified"),
            tr("Token was not verified. The change to your token has not been saved."));
    }
}

std::string ConfigureOnline::LocalYuzuUsername() {
    return UsernameFromDisplayToken(ui->edit_token->text().toStdString());
}

std::string ConfigureOnline::LocalYuzuToken() {
    return TokenFromDisplayToken(ui->edit_token->text().toStdString());
}

void ConfigureOnline::RefreshTelemetryID() {
    const u64 new_telemetry_id{Core::RegenerateTelemetryId()};
    ui->label_telemetry_id->setText(
        tr("Telemetry ID: 0x%1").arg(QString::number(new_telemetry_id, 16).toUpper()));
}

void ConfigureOnline::OnLoginChanged() {
    if (ui->edit_token->text().isEmpty()) {
        user_verified = true;

        const QPixmap pixmap = QIcon::fromTheme(QStringLiteral("checked")).pixmap(16);
        ui->label_token_verified->setPixmap(pixmap);
    } else {
        user_verified = false;

        const QPixmap pixmap = QIcon::fromTheme(QStringLiteral("failed")).pixmap(16);
        ui->label_token_verified->setPixmap(pixmap);
    }
}

void ConfigureOnline::VerifyLogin() {
    ui->button_verify_login->setDisabled(true);
    ui->button_verify_login->setText(tr("Verifying..."));
    verify_watcher.setFuture(QtConcurrent::run(
        [username = UsernameFromDisplayToken(ui->edit_token->text().toStdString()),
         token = TokenFromDisplayToken(ui->edit_token->text().toStdString())] {
            return Core::VerifyLogin(username, token);
        }));
}

void ConfigureOnline::OnLoginVerified() {
    ui->button_verify_login->setEnabled(true);
    ui->button_verify_login->setText(tr("Verify"));

    if (verify_watcher.result()) {
        user_verified = true;

        const QPixmap pixmap = QIcon::fromTheme(QStringLiteral("checked")).pixmap(16);
        ui->label_token_verified->setPixmap(pixmap);
        ui->username->setText(
            QString::fromStdString(UsernameFromDisplayToken(ui->edit_token->text().toStdString())));
        online_status_monitor->DisableAirplaneMode();
        friend_list->Reload();

        Settings::values.yuzu_username = LocalYuzuUsername();
        Settings::values.yuzu_token = LocalYuzuToken();
    } else {
        const QPixmap pixmap = QIcon::fromTheme(QStringLiteral("failed")).pixmap(16);
        ui->label_token_verified->setPixmap(pixmap);
        ui->username->setText(tr("Unspecified"));
        QMessageBox::critical(this, tr("Verification failed"),
                              tr("Verification failed. Check that you have entered your token "
                                 "correctly, and that your internet connection is working."));
    }

    RefreshOnlineAvatar();
    RefreshOnlineUserName();
}

void ConfigureOnline::RefreshOnlineUserName() {
    if (Settings::values.yuzu_token.empty() || Settings::values.yuzu_username.empty() ||
        Settings::values.is_airplane_mode) {
        ui->label_online_username->setText(tr("Unspecified"));
        ui->label_online_username->setDisabled(true);
        return;
    }

    ui->label_online_username->setDisabled(true);
    ui->label_online_username->setText(tr("Refreshing..."));
    online_username_watcher.setFuture(QtConcurrent::run([this]() -> std::optional<std::string> {
        const std::optional headers = AuthorizationHeaders(online_initiator);
        if (!headers) {
            return std::nullopt;
        }
        httplib::SSLClient client(online_initiator->ProfileApiUrl());
        auto response = client.Get("/api/v1/username", *headers);
        if (!response || response->status != 200) {
            LOG_ERROR(Frontend, "Failed to query username from server");
            return std::nullopt;
        }
        return response->body;
    }));
}

void ConfigureOnline::OnOnlineUserNameRefreshed() {
    if (const std::optional<std::string> result = online_username_watcher.result(); result) {
        ui->label_online_username->setText(QString::fromStdString(*result));
        ui->label_online_username->setDisabled(false);
        ui->button_set_username->setEnabled(true);
    } else {
        ui->label_online_username->setText(tr("Unspecified"));
        ui->label_online_username->setDisabled(true);
        ui->button_set_username->setEnabled(false);
    }
}

void ConfigureOnline::RefreshOnlineAvatar(std::chrono::seconds delay) {
    profile_scene->clear();
    if (Settings::values.yuzu_token.empty() || Settings::values.yuzu_username.empty() ||
        Settings::values.is_airplane_mode) {
        return;
    }

    profile_scene->addItem(
        new QGraphicsPixmapItem(QIcon::fromTheme(QStringLiteral("portrait_sync")).pixmap(48)));

    online_avatar_watcher.setFuture(QtConcurrent::run([this, delay]() -> QImage {
        const std::optional headers = AuthorizationHeaders(online_initiator);
        if (!headers) {
            return {};
        }
        std::this_thread::sleep_for(delay);
        httplib::SSLClient client(online_initiator->ProfileApiUrl());
        auto response = client.Get("/api/v1/avatar/64/64", *headers);
        if (!response || response->status != 302) {
            LOG_ERROR(Frontend, "Failed to query avatar from server");
            return {};
        }
        const auto it = response->headers.find("Location");
        if (it == response->headers.end()) {
            LOG_ERROR(Frontend, "'Location' header missing in response");
            return {};
        }
        return DownloadImageUrl(it->second);
    }));
}

void ConfigureOnline::OnOnlineAvatarRefreshed() {
    profile_scene->clear();

    QImage image = online_avatar_watcher.result();
    if (image.isNull()) {
        QPixmap pixmap = QIcon::fromTheme(QStringLiteral("avatar-sync-error")).pixmap(48);
        profile_scene->addItem(new QGraphicsPixmapItem(pixmap));
        ui->button_set_avatar->setEnabled(false);
        return;
    }

    QPixmap pixmap =
        QPixmap::fromImage(image).scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    profile_scene->addItem(new QGraphicsPixmapItem(pixmap));
    ui->button_set_avatar->setEnabled(true);
}

void ConfigureOnline::SetUserName() {
    QString old_username = ui->label_online_username->text();
    bool ok = false;
    QString new_username = QInputDialog::getText(this, tr("Set Username"), tr("Username:"),
                                                 QLineEdit::Normal, old_username, &ok);
    if (!ok || old_username == new_username) {
        return;
    }

    button_set_username_text = ui->button_set_username->text();
    ui->button_set_username->setEnabled(false);
    ui->button_set_username->setText(tr("Uploading..."));

    upload_username_watcher.setFuture(QtConcurrent::run([this, name = new_username.toStdString()] {
        const std::optional headers = AuthorizationHeaders(online_initiator);
        if (!headers) {
            return -1;
        }
        httplib::SSLClient client(online_initiator->ProfileApiUrl());
        auto response = client.Put("/api/v1/username", *headers, name, "text/plain");
        return response ? response->status : -1;
    }));
}

void ConfigureOnline::OnUserNameUploaded() {
    ui->button_set_username->setEnabled(true);
    ui->button_set_username->setText(button_set_username_text);

    switch (upload_username_watcher.result()) {
    case 200:
        RefreshOnlineUserName();
        break;
    case 400:
        QMessageBox::critical(this, tr("Set Username"), tr("Invalid username."));
        break;
    default:
        QMessageBox::critical(this, tr("Set Username"), tr("Failed to update username."));
        break;
    }
}

void ConfigureOnline::SetAvatar() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select Avatar"), QString(),
        tr("JPEG Images (*.jpg *.jpeg);;PNG Images (*.png);;BMP Images (*.bmp)"));
    if (file.isEmpty()) {
        return;
    }

    QPixmap source(file);
    const QSize size = source.size();
    if (size.width() < AVATAR_MIN_SIZE || size.height() < AVATAR_MIN_SIZE) {
        const auto reply = QMessageBox::warning(
            this, tr("Select Avatar"),
            tr("Selected image is smaller than %1 pixels.\n"
               "Images with the same width and height and larger than %2 pixels are recommended. "
               "That said, yuzu will scale the image and add white borders.\n\n"
               "Do you want to proceed?")
                .arg(AVATAR_MIN_SIZE)
                .arg(AVATAR_MIN_SIZE),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    } else if (size.width() != size.height()) {
        const auto reply =
            QMessageBox::warning(this, tr("Select Avatar"),
                                 tr("Selected image is not squared.\n"
                                    "Images with the same width and height are recommended. That "
                                    "said, yuzu will adjust the image adding white borders.\n\n"
                                    "Do you want to proceed?"),
                                 QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    button_set_avatar_text = ui->button_set_avatar->text();
    ui->button_set_avatar->setEnabled(false);
    ui->button_set_avatar->setText(tr("Uploading..."));

    upload_avatar_watcher.setFuture(QtConcurrent::run([this, source] {
        const QSize size = source.size();
        const int max = std::max(size.width(), size.height());
        const int dim = std::max(max, AVATAR_MIN_SIZE);

        QPixmap pixmap(QSize(dim, dim));
        pixmap.fill(QColor(255, 255, 255));

        QTransform transform;
        const qreal scale = static_cast<qreal>(dim) / static_cast<qreal>(max);
        transform.scale(scale, scale);

        QPainter painter(&pixmap);
        painter.setTransform(transform);
        const QPoint draw_pos((dim - size.width() * scale) / 2, (dim - size.height() * scale) / 2);
        painter.drawPixmap(draw_pos / scale, source);

        QByteArray byte_array;
        QBuffer buffer(&byte_array);
        buffer.open(QIODevice::WriteOnly);
        pixmap.save(&buffer, "JPEG");
        std::string jpeg_string = byte_array.toStdString();

        const std::optional headers = AuthorizationHeaders(online_initiator);
        if (!headers) {
            return -1;
        }
        httplib::SSLClient client(online_initiator->ProfileApiUrl());
        auto response = client.Put("/api/v1/avatar", *headers, jpeg_string, "image/jpeg");
        return response ? response->status : -1;
    }));
}

void ConfigureOnline::OnAvatarUploaded() {
    ui->button_set_avatar->setEnabled(true);
    ui->button_set_avatar->setText(button_set_avatar_text);

    switch (upload_avatar_watcher.result()) {
    case 200:
    case 202:
        RefreshOnlineAvatar(std::chrono::seconds{5});
        break;
    case 400:
        QMessageBox::critical(this, tr("Set Avatar"), tr("Invalid avatar."));
        break;
    case 429:
        QMessageBox::critical(this, tr("Set Avatar"), tr("Avatar has been set too recently."));
        break;
    default:
        QMessageBox::critical(this, tr("Set Avatar"), tr("Failed to upload avatar."));
        break;
    }
}
