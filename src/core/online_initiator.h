// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "common/common_types.h"

namespace Core {

class OnlineInitiator {
public:
    struct IdToken {
        std::string token;
        u64 id;
    };

    explicit OnlineInitiator();
    ~OnlineInitiator();

    OnlineInitiator(const OnlineInitiator&) = delete;
    OnlineInitiator& operator=(const OnlineInitiator&) = delete;

    OnlineInitiator(OnlineInitiator&&) = delete;
    OnlineInitiator& operator=(OnlineInitiator&&) = delete;

    void Connect();

    void Disconnect();

    void StartOnlineSession(u64 title_id);

    void EndOnlineSession();

    void WaitForCompletion() const;

    [[nodiscard]] bool IsConnected() const;

    [[nodiscard]] std::string ProfileApiUrl() const;

    [[nodiscard]] std::string FriendsApiUrl() const;

    [[nodiscard]] std::string TroubleshooterUrl() const;

    [[nodiscard]] std::string YuzuAccountsUrl() const;

    [[nodiscard]] std::string NotificationUrl() const;

    [[nodiscard]] std::optional<std::string_view> RewriteUrl(const std::string& url) const;

    [[nodiscard]] std::string ResolveUrl(std::string dns, bool use_nsd) const;

    [[nodiscard]] std::optional<IdToken> LoadIdToken(u64 title_id) const;

    [[nodiscard]] std::optional<IdToken> LoadIdTokenApp(std::string app_name) const;

private:
    std::optional<OnlineInitiator::IdToken> LoadIdTokenInternal(
        std::vector<std::pair<std::string, std::string>> input_headers) const;

    void AskServer();

    mutable std::mutex mutex;
    std::thread thread;

    std::mutex ask_mutex;
    std::condition_variable ask_condvar;

    std::unordered_map<std::string, std::string> url_rewrites;
    std::string profile_api_url;
    std::string friends_api_url;
    std::string troubleshooter_url;
    std::string yuzu_accounts_url;
    std::string notification_url;
    std::future<void> async_start_online_session;
    std::future<void> async_end_online_session;
    bool is_connected = false;
};

} // namespace Core
