// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "core/hle/kernel/writable_event.h"
#include "core/hle/result.h"

namespace Core {
class System;
}

namespace Service::Account {

class TokenGranter {
public:
    explicit TokenGranter(Core::System& system, std::string& output_token, u64& output_id);
    ~TokenGranter();

    void Cancel();

    [[nodiscard]] std::shared_ptr<Kernel::ReadableEvent> GetSystemEvent();

    [[nodiscard]] bool HasDone();

    [[nodiscard]] ResultCode GetResult();

private:
    void WorkerThread(Core::System& system, std::string& output_token, u64& output_id);

    ResultCode output_result = RESULT_SUCCESS;
    bool has_done = false;
    bool is_cancelled = false;

    Kernel::EventPair events;

    std::thread thread;
    std::mutex mutex;
};

} // namespace Service::Account
