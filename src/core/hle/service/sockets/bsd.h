// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/service.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace Service::Sockets {

class BSD final : public ServiceFramework<BSD> {
public:
    explicit BSD(const char* name);
    ~BSD() override;

private:
    void WriteBsdResult(Kernel::HLERequestContext& ctx, int result, int errorCode = 0);

    void RegisterClient(Kernel::HLERequestContext& ctx);
    void StartMonitoring(Kernel::HLERequestContext& ctx);
    void Socket(Kernel::HLERequestContext& ctx);
    void Connect(Kernel::HLERequestContext& ctx);
    void SendTo(Kernel::HLERequestContext& ctx);
    void Close(Kernel::HLERequestContext& ctx);
    void GetSockOpt(Kernel::HLERequestContext& ctx);
    void SetSockOpt(Kernel::HLERequestContext& ctx);
    void Bind(Kernel::HLERequestContext& ctx);
    void RecvFrom(Kernel::HLERequestContext& ctx);
    void Fcntl(Kernel::HLERequestContext& ctx);

    /// Id to use for the next open file descriptor.
    u32 next_fd = 1;
};

class BSDCFG final : public ServiceFramework<BSDCFG> {
public:
    explicit BSDCFG();
    ~BSDCFG() override;
};

} // namespace Service::Sockets
