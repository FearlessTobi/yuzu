// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <httplib.h>

#include "common/logging/log.h"
#include "core/online_initiator.h"
#include "core/settings.h"
#include "web_service/web_backend.h"
#include "web_service/web_result.h"

namespace Core {

static constexpr char INITIATOR_URL[] = "initiator.raptor.network";

OnlineInitiator::OnlineInitiator() {
    Connect();
}

OnlineInitiator::~OnlineInitiator() {
    std::scoped_lock lock{mutex};
    if (thread.joinable()) {
        thread.join();
    }
}

void OnlineInitiator::Connect() {
    {
        std::scoped_lock lock{mutex};
        if (Settings::values.is_airplane_mode) {
            return;
        }
        if (is_connected) {
            return;
        }
        if (thread.joinable()) {
            thread.join();
        }
        thread = std::thread(&OnlineInitiator::AskServer, this);
    }

    std::unique_lock lock{ask_mutex};
    ask_condvar.wait(lock);
}

void OnlineInitiator::Disconnect() {
    std::scoped_lock lock{mutex};
    is_connected = false;
}

void OnlineInitiator::StartOnlineSession(u64 title_id) {}

void OnlineInitiator::EndOnlineSession() {}

void OnlineInitiator::WaitForCompletion() const {
    std::scoped_lock lock{mutex};
}

bool OnlineInitiator::IsConnected() const {
    std::scoped_lock lock{mutex};
    return is_connected;
}

std::string OnlineInitiator::ProfileApiUrl() const {
    std::scoped_lock lock{mutex};
    return profile_api_url;
}

std::string OnlineInitiator::FriendsApiUrl() const {
    std::scoped_lock lock{mutex};
    return friends_api_url;
}

std::string OnlineInitiator::TroubleshooterUrl() const {
    std::scoped_lock lock{mutex};
    return troubleshooter_url;
}

std::string OnlineInitiator::YuzuAccountsUrl() const {
    std::scoped_lock lock{mutex};
    return yuzu_accounts_url;
}

std::string OnlineInitiator::NotificationUrl() const {
    std::scoped_lock lock{mutex};
    return notification_url;
}

std::optional<std::string_view> OnlineInitiator::RewriteUrl(const std::string& url) const {
    std::scoped_lock lock{mutex};
    const auto it = url_rewrites.find(url);
    if (it == url_rewrites.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::string OnlineInitiator::ResolveUrl(std::string dns, bool use_nsd) const {
    if (auto rewrite = RewriteUrl(dns)) {
        LOG_INFO(Core, "Rewrite '{}' to '{}'", dns, *rewrite);
        dns = std::move(*rewrite);
    }
    if (dns.find("nintendo") != std::string::npos || dns.find('%') != std::string::npos) {
        // Avoid connecting to Nintendo servers
        LOG_INFO(Core, "Trying to connect to Nintendo's server or environment server '{}'", dns);
        dns = "127.0.0.1";
    }
    return dns;
}

std::optional<OnlineInitiator::IdToken> OnlineInitiator::LoadIdTokenInternal(
    std::vector<std::pair<std::string, std::string>> input_headers) const {
    std::scoped_lock lock{mutex};
    if (!is_connected) {
        LOG_ERROR(Network, "Trying to load id token pair when online is not connected");
        return std::nullopt;
    }
    if (Settings::values.yuzu_username.empty() || Settings::values.yuzu_token.empty()) {
        LOG_ERROR(Network, "No yuzu user name or token configured");
        return std::nullopt;
    }

    WebService::Client web_client(Settings::values.web_api_url, Settings::values.yuzu_username,
                                  Settings::values.yuzu_token);
    const WebService::WebResult web_result = web_client.GetInternalJWT();
    if (web_result.result_code != WebService::WebResult::Code::Success) {
        LOG_ERROR(Network, "Failed to obtain internal token from the web service");
        return std::nullopt;
    }

    const std::string_view jwt_token = web_result.returned_data;
    httplib::Headers headers;
    for (const auto [key, value] : input_headers) {
        headers.emplace(key, value);
    }
    headers.emplace("Authorization", fmt::format("Bearer {}", jwt_token));

    httplib::SSLClient client(yuzu_accounts_url);
    const auto response = client.Post("/api/v1/token", headers, "", "");
    if (!response) {
        LOG_ERROR(Network, "Failed to request online token from server");
        return std::nullopt;
    }

    switch (response->status) {
    case 200:
        break;
    case 400:
        LOG_ERROR(Network, "Game has no online functionality");
        return std::nullopt;
    case 401:
        LOG_ERROR(Network, "Missing token in headers");
        return std::nullopt;
    case 403:
        LOG_ERROR(Network, "User not allowed online");
        return std::nullopt;
    default:
        LOG_ERROR(Network, "Network error={}", response->status);
        return std::nullopt;
    }

    IdToken result;
    std::string pid;
    try {
        nlohmann::json json = nlohmann::json::parse(response->body);
        json.at("token").get_to(result.token);
        json.at("pid").get_to(pid);
    } catch (const std::exception& e) {
        LOG_ERROR(Network, "Error parsing json: {}", e.what());
        return std::nullopt;
    }

    if (std::from_chars(pid.data(), pid.data() + pid.size(), result.id, 16).ec != std::errc{}) {
        LOG_ERROR(Network, "Error parsing account id");
        return std::nullopt;
    }

    return std::make_optional(std::move(result));
}

void OnlineInitiator::AskServer() try {
    std::scoped_lock lock{mutex};
    {
        std::scoped_lock ask_lock{ask_mutex};
        ask_condvar.notify_one();
    }

    httplib::SSLClient client(INITIATOR_URL);
    client.set_follow_location(true);
    const auto response = client.Get("/yuzu.json");
    if (!response || response->status != 200) {
        return;
    }

    const nlohmann::json json = nlohmann::json::parse(response->body);

    const auto data = json.at("data");
    data.at("profile_url").get_to(profile_api_url);
    data.at("friends_url").get_to(friends_api_url);
    data.at("status_url").get_to(troubleshooter_url);
    data.at("yuzu_accounts_url").get_to(yuzu_accounts_url);
    data.at("notification_url").get_to(notification_url);

    for (const auto url_rewrite : json.at("url_rewrites")) {
        const auto destination = url_rewrite.at("destination").get<std::string>();
        const auto source = url_rewrite.at("source").get<std::string>();
        url_rewrites.emplace(source, destination);
    }
    is_connected = true;

} catch (const std::exception& e) {
    url_rewrites.clear();
    LOG_ERROR(Core, "{}", e.what());
}

std::optional<OnlineInitiator::IdToken> OnlineInitiator::LoadIdTokenApp(
    std::string app_name) const {
    return LoadIdTokenInternal({{"R-Target", std::move(app_name)}});
}

std::optional<OnlineInitiator::IdToken> OnlineInitiator::LoadIdToken(u64 title_id) const {
    return LoadIdTokenInternal({{"R-TitleId", fmt::format("{:X}", title_id)}});
}

} // namespace Core
