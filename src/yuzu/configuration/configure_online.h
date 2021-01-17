// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <QFutureWatcher>
#include <QImage>
#include <QString>
#include <QWidget>

namespace Core {
class OnlineInitiator;
}

namespace Ui {
class ConfigureOnline;
}

class QGraphicsScene;

class FriendsList;
class OnlineStatusMonitor;

class ConfigureOnline : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureOnline(QWidget* parent = nullptr);
    ~ConfigureOnline() override;

    void Initialize(Core::OnlineInitiator& online_initiator,
                    OnlineStatusMonitor* online_status_monitor, FriendsList* friend_list);

    void ApplyConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void RefreshTelemetryID();
    void OnLoginChanged();
    void VerifyLogin();
    void OnLoginVerified();
    void RefreshOnlineUserName();
    void OnOnlineUserNameRefreshed();
    void RefreshOnlineAvatar(std::chrono::seconds delay = std::chrono::seconds{0});
    void OnOnlineAvatarRefreshed();
    void SetUserName();
    void OnUserNameUploaded();
    void SetAvatar();
    void OnAvatarUploaded();

    void SetConfiguration();

    [[nodiscard]] std::string LocalYuzuUsername();
    [[nodiscard]] std::string LocalYuzuToken();

    std::unique_ptr<Ui::ConfigureOnline> ui;

    Core::OnlineInitiator* online_initiator = nullptr;
    OnlineStatusMonitor* online_status_monitor = nullptr;
    FriendsList* friend_list = nullptr;

    QGraphicsScene* profile_scene = nullptr;
    QString button_set_username_text;
    QString button_set_avatar_text;

    bool user_verified = true;
    QFutureWatcher<bool> verify_watcher;
    QFutureWatcher<std::optional<std::string>> online_username_watcher;
    QFutureWatcher<QImage> online_avatar_watcher;
    QFutureWatcher<int> upload_username_watcher;
    QFutureWatcher<int> upload_avatar_watcher;
};
