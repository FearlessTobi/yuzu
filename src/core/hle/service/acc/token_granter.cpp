// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <string_view>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <httplib.h>

#include "common/common_types.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/service/acc/token_granter.h"
#include "core/online_initiator.h"
#include "core/settings.h"

namespace Service::Account {

constexpr ResultCode RESULT_CANCELLED{ErrorModule::Account, 0};
constexpr ResultCode RESULT_NETWORK_ERROR{ErrorModule::Account, 3000};

TokenGranter::TokenGranter(Core::System& system, std::string& output_token, u64& output_id) {
    events = Kernel::WritableEvent::CreateEventPair(system.Kernel(), "IAsyncContext:TokenGranter");
    thread = std::thread([&] { WorkerThread(system, output_token, output_id); });
}

TokenGranter::~TokenGranter() {
    thread.join();
}

void TokenGranter::Cancel() {
    std::scoped_lock lock{mutex};
    if (has_done) {
        LOG_WARNING(Service_ACC, "Cancelling a finished operation");
    }
    output_result = RESULT_CANCELLED;
}

std::shared_ptr<Kernel::ReadableEvent> TokenGranter::GetSystemEvent() {
    std::scoped_lock lock{mutex};
    return events.readable;
}

bool TokenGranter::HasDone() {
    std::scoped_lock lock{mutex};
    return has_done;
}

ResultCode TokenGranter::GetResult() {
    std::scoped_lock lock{mutex};
    if (!has_done) {
        LOG_ERROR(Service_ACC, "Asynchronous result read before it was written");
    }
    return output_result;
}

void TokenGranter::WorkerThread(Core::System& system, std::string& output_token, u64& output_id) {
    Common::SetCurrentThreadName("TokenGranter");
    system.Kernel().RegisterHostThread();

    const u64 title_id = system.CurrentProcess()->GetTitleID();
    auto& online_initiator = system.OnlineInitiator();
    online_initiator.StartOnlineSession(title_id);

    std::optional id_token = online_initiator.LoadIdToken(title_id);

    std::scoped_lock lock{mutex};
    if (output_result == RESULT_CANCELLED) {
        return;
    }
    if (id_token) {
        output_token = std::move(id_token->token);
        output_id = id_token->id;
    }
    has_done = true;
    output_result = id_token ? RESULT_SUCCESS : RESULT_NETWORK_ERROR;
    events.writable->Signal();

    LOG_INFO(Service_ACC, "Asynchronous operation has completed");
}

} // namespace Service::Account
