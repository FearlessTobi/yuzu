// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/result.h"
#include "core/hle/service/ldn/lan_discovery.h"
#include "core/hle/service/ldn/ldn.h"
#include "core/hle/service/ldn/ldn_types.h"
#include "core/hle/service/sm/sm.h"

namespace Service::LDN {

class IMonitorService final : public ServiceFramework<IMonitorService> {
public:
    explicit IMonitorService() : ServiceFramework{"IMonitorService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetStateForMonitor"},
            {1, nullptr, "GetNetworkInfoForMonitor"},
            {2, nullptr, "GetIpv4AddressForMonitor"},
            {3, nullptr, "GetDisconnectReasonForMonitor"},
            {4, nullptr, "GetSecurityParameterForMonitor"},
            {5, nullptr, "GetNetworkConfigForMonitor"},
            {100, nullptr, "InitializeMonitor"},
            {101, nullptr, "FinalizeMonitor"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class LDNM final : public ServiceFramework<LDNM> {
public:
    explicit LDNM() : ServiceFramework{"ldn:m"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &LDNM::CreateMonitorService, "CreateMonitorService"}
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    void CreateMonitorService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IMonitorService>();
    }
};

class ISystemLocalCommunicationService final
    : public ServiceFramework<ISystemLocalCommunicationService> {
public:
    explicit ISystemLocalCommunicationService()
        : ServiceFramework{"ISystemLocalCommunicationService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetState"},
            {1, nullptr, "GetNetworkInfo"},
            {2, nullptr, "GetIpv4Address"},
            {3, nullptr, "GetDisconnectReason"},
            {4, nullptr, "GetSecurityParameter"},
            {5, nullptr, "GetNetworkConfig"},
            {100, nullptr, "AttachStateChangeEvent"},
            {101, nullptr, "GetNetworkInfoLatestUpdate"},
            {102, nullptr, "Scan"},
            {103, nullptr, "ScanPrivate"},
            {200, nullptr, "OpenAccessPoint"},
            {201, nullptr, "CloseAccessPoint"},
            {202, nullptr, "CreateNetwork"},
            {203, nullptr, "CreateNetworkPrivate"},
            {204, nullptr, "DestroyNetwork"},
            {205, nullptr, "Reject"},
            {206, nullptr, "SetAdvertiseData"},
            {207, nullptr, "SetStationAcceptPolicy"},
            {208, nullptr, "AddAcceptFilterEntry"},
            {209, nullptr, "ClearAcceptFilter"},
            {300, nullptr, "OpenStation"},
            {301, nullptr, "CloseStation"},
            {302, nullptr, "Connect"},
            {303, nullptr, "ConnectPrivate"},
            {304, nullptr, "Disconnect"},
            {400, nullptr, "InitializeSystem"},
            {401, nullptr, "FinalizeSystem"},
            {402, nullptr, "SetOperationMode"},
            {403, nullptr, "InitializeSystem2"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IUserLocalCommunicationService final
    : public ServiceFramework<IUserLocalCommunicationService> {
public:
    explicit IUserLocalCommunicationService() : ServiceFramework{"IUserLocalCommunicationService"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IUserLocalCommunicationService::GetState, "GetState"},
            {1, &IUserLocalCommunicationService::GetNetworkInfo, "GetNetworkInfo"},
            {2, &IUserLocalCommunicationService::GetIpv4Address, "GetIpv4Address"},
            {3,  &IUserLocalCommunicationService::GetDisconnectReason, "GetDisconnectReason"},
            {4, &IUserLocalCommunicationService::GetSecurityParameter, "GetSecurityParameter"},
            {5, nullptr, "GetNetworkConfig"},
            {100, &IUserLocalCommunicationService::AttachStateChangeEvent, "AttachStateChangeEvent"},
            {101, &IUserLocalCommunicationService::GetNetworkInfoLatestUpdate, "GetNetworkInfoLatestUpdate"},
            {102, &IUserLocalCommunicationService::Scan, "Scan"},
            {103, nullptr, "ScanPrivate"},
            {104, nullptr, "SetWirelessControllerRestriction"},
            {200, &IUserLocalCommunicationService::OpenAccessPoint, "OpenAccessPoint"},
            {201, nullptr, "CloseAccessPoint"},
            {202, &IUserLocalCommunicationService::CreateNetwork, "CreateNetwork"},
            {203, nullptr, "CreateNetworkPrivate"},
            {204, nullptr, "DestroyNetwork"},
            {205, nullptr, "Reject"},
            {206, &IUserLocalCommunicationService::SetAdvertiseData, "SetAdvertiseData"},
            {207, nullptr, "SetStationAcceptPolicy"},
            {208, nullptr, "AddAcceptFilterEntry"},
            {209, nullptr, "ClearAcceptFilter"},
            {300, &IUserLocalCommunicationService::OpenStation, "OpenStation"},
            {301, &IUserLocalCommunicationService::CloseStation, "CloseStation"},
            {302, nullptr, "Connect"},
            {303, nullptr, "ConnectPrivate"},
            {304, nullptr, "Disconnect"},
            {400, &IUserLocalCommunicationService::Initialize, "Initialize"},
            {401, &IUserLocalCommunicationService::Finalize, "Finalize"},
            {402, nullptr, "SetOperationMode"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    void onEventFired() {
        LOG_CRITICAL(Service_LDN, "onEventFired signal_event");
        state_event.writable->Signal();
    }

    void Initialize(Kernel::HLERequestContext& ctx) {
        ResultCode result_code = lanDiscovery.initialize([&]() { this->onEventFired(); });

        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result_code);
    }

    void GetState(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(static_cast<u32>(this->lanDiscovery.getState()));
    }

    void AttachStateChangeEvent(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(state_event.readable);
    }

    void GetDisconnectReason(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u16>(0);
    }

    void OpenStation(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(lanDiscovery.openStation());
    }

    void Scan(Kernel::HLERequestContext& ctx) {
        // TODO: Check IPC shit if something breaks
        LOG_CRITICAL(Service_LDN, "called");

        IPC::RequestParser rp{ctx};
        const auto channel{rp.Pop<u16>()};
        const auto filter{rp.PopRaw<ScanFilter>()};

        ResultCode rc = RESULT_SUCCESS;

        NetworkInfo info{};

        // TODO: Stubbed
        u16 count = 0;
        rc = lanDiscovery.scan(&info, &count, filter);

        std::array<u8, sizeof(info)> out_buf;
        std::memcpy(&out_buf, &info, sizeof(info));

        ctx.WriteBuffer(out_buf);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(rc);
        rb.Push<u16>(count);
    }

    void Finalize(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(lanDiscovery.finalize());
    }

    void CloseStation(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(lanDiscovery.closeStation());
    }

    void OpenAccessPoint(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(lanDiscovery.openAccessPoint());
    }

    void SetAdvertiseData(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};

        std::vector<u8> data = ctx.ReadBuffer();

        // TODO: Correct Number
        rb.Push(lanDiscovery.setAdvertiseData(data.data(), sizeof(data)));
    }

    void CreateNetwork(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::RequestParser rp{ctx};
        const auto data{rp.PopRaw<CreateNetworkConfig>()};

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(lanDiscovery.createNetwork(&data.securityConfig, &data.userConfig,
                                           &data.networkConfig));
    }

    ResultCode ipinfoGetIpConfig(u32* ip) {
        // TODO: hardcoded
        *ip = 1467670916;
        return RESULT_SUCCESS;
    }

    ResultCode ipinfoGetIpConfig(u32* ip, u32* netmask) {
        return ipinfoGetIpConfig(ip);
    }

    void GetIpv4Address(Kernel::HLERequestContext& ctx) {
        u32 address = 0;
        u32 netmask = 0;

        ResultCode rc = ipinfoGetIpConfig(&address, &netmask);

        LOG_CRITICAL(Service_LDN, "STUBBED called address {} netmask {}", address, netmask);

        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(rc);

        rb.Push(address);
        rb.Push(netmask);
    }

    void GetSecurityParameter(Kernel::HLERequestContext& ctx) {
        // TODO: Correct ResultCode checking everywhere
        LOG_CRITICAL(Service_LDN, "called");

        SecurityParameter data;
        NetworkInfo info;
        ResultCode rc = lanDiscovery.getNetworkInfo(&info);
        if (rc == RESULT_SUCCESS) {
            NetworkInfo2SecurityParameter(&info, &data);
        }

        IPC::ResponseBuilder rb{ctx, 2 + 0x20};
        rb.Push(rc);
        rb.PushRaw<SecurityParameter>(data);
    }

    void GetNetworkInfo(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        NetworkInfo info{};
        ResultCode rc = lanDiscovery.getNetworkInfo(&info);

        std::array<u8, sizeof(info)> out_buf;
        std::memcpy(&out_buf, &info, sizeof(info));

        LOG_CRITICAL(Service_LDN, "channel: {}", info.common.channel);
        LOG_CRITICAL(Service_LDN, "linkLevel: {}", info.common.linkLevel);

        ctx.WriteBuffer(out_buf);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void GetNetworkInfoLatestUpdate(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        NetworkInfo info{};
        NodeLatestUpdate updates{};

        // TODO: Unstub size
        ResultCode rc = lanDiscovery.getNetworkInfo(&info, &updates, 0);

        std::array<u8, sizeof(info)> out_buf_0;
        std::memcpy(&out_buf_0, &info, sizeof(info));

        std::array<u8, sizeof(updates)> out_buf_1;
        std::memcpy(&out_buf_1, &updates, sizeof(updates));

        LOG_CRITICAL(Service_LDN, "channel: {}", info.common.channel);
        LOG_CRITICAL(Service_LDN, "linkLevel: {}", info.common.linkLevel);

        ctx.WriteBuffer(out_buf_0);
        ctx.WriteBuffer(out_buf_1);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

private:
    LANDiscovery lanDiscovery;
    Kernel::EventPair state_event = Kernel::WritableEvent::CreateEventPair(
        Core::System::GetInstance().Kernel(), Kernel::ResetType::Automatic,
        "IUserLocalCommunicationService:StateEvent");
};

class LDNS final : public ServiceFramework<LDNS> {
public:
    explicit LDNS() : ServiceFramework{"ldn:s"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &LDNS::CreateSystemLocalCommunicationService, "CreateSystemLocalCommunicationService"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    void CreateSystemLocalCommunicationService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<ISystemLocalCommunicationService>();
    }
};

class LDNU final : public ServiceFramework<LDNU> {
public:
    explicit LDNU() : ServiceFramework{"ldn:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &LDNU::CreateUserLocalCommunicationService, "CreateUserLocalCommunicationService"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    void CreateUserLocalCommunicationService(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IUserLocalCommunicationService>();
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LDNM>()->InstallAsService(sm);
    std::make_shared<LDNS>()->InstallAsService(sm);
    std::make_shared<LDNU>()->InstallAsService(sm);
}

} // namespace Service::LDN
