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

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace Service::LDN {

// TODO: Problems: Scan is broken? IP Getting is Ultra broken? Port might be wrong? CreateNetwork
// returns errors? SockOpt broken? ...? BSD UNIMPLEMENTED (Bind impl wrong)!

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
            {104, nullptr, "SetWirelessControllerRestriction"},
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
            {201, &IUserLocalCommunicationService::CloseAccessPoint, "CloseAccessPoint"},
            {202, &IUserLocalCommunicationService::CreateNetwork, "CreateNetwork"},
            {203, nullptr, "CreateNetworkPrivate"},
            {204, &IUserLocalCommunicationService::DestroyNetwork, "DestroyNetwork"},
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
            {402, &IUserLocalCommunicationService::Initialize2, "Initialize2"}, // 7.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    void Initialize2(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_LDN, "(STUBBED) called");
        // Result success seem make this services start network and continue.
        // If we just pass result error then it will stop and maybe try again and again.
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_UNKNOWN);
	}

    ResultCode ipinfoGetIpConfig(u32* address, u32* netmask) {
        struct {
            u8 _unk;
            u32 address;
            u32 netmask;
            u32 gateway;
        } resp;

        // TODO: Unstub
        resp.address = inet_addr("10.13.0.2");
        // resp.address = 2130706433; // 127.0.0.1
        resp.netmask = inet_addr("255.255.0.0"); // leave it tobi
        resp.gateway = inet_addr("10.13.37.1");  // unused

        *address = ntohl(resp.address);
        *netmask = ntohl(resp.netmask);

        return RESULT_SUCCESS;
    }

    ResultCode ipinfoGetIpConfig(u32* address) {
        u32 netmask;
        return ipinfoGetIpConfig(address, &netmask);
    }

    void onEventFired() {
        LOG_CRITICAL(Service_LDN, "onEventFired signal_event");
        state_event.writable->Signal();
    }

    void Initialize(Kernel::HLERequestContext& ctx) {
        ResultCode result_code = lanDiscovery.initialize([&]() { this->onEventFired(); });

        LOG_CRITICAL(Service_LDN, "called");

        if (result_code != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result_code);
    }

    void GetState(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 3};

        u32 state = static_cast<u32>(lanDiscovery.getState());

        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(state);
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

        ResultCode rc = lanDiscovery.openStation();

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void Scan(Kernel::HLERequestContext& ctx) {
        // TODO: Check IPC shit if something breaks
        LOG_CRITICAL(Service_LDN, "called");

        IPC::RequestParser rp{ctx};
        const auto channel{rp.Pop<u16>()};
        const auto filter{rp.PopRaw<ScanFilter>()};

        NetworkInfo info{};

        // TODO: Stubbed
        u16 count = 5;
        LOG_CRITICAL(Frontend, "count: ", count);
        ResultCode rc = lanDiscovery.scan(&info, &count, filter);

        if (rc != RESULT_SUCCESS) {
            // Error 20 gets returned
            LOG_ERROR(Service_LDN, "Error! {}", rc.description);
        }

        std::array<u8, sizeof(info)> out_buf;
        std::memcpy(&out_buf, &info, sizeof(info));

        ctx.WriteBuffer(out_buf);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(rc);
        rb.Push<u16>(count);
    }

    void Finalize(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        int rc = lanDiscovery.finalize();

        if (rc != 0) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void CloseStation(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        ResultCode rc = lanDiscovery.closeStation();

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void OpenAccessPoint(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        ResultCode rc = lanDiscovery.openAccessPoint();
        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void CloseAccessPoint(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        ResultCode rc = lanDiscovery.closeAccessPoint();
        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void SetAdvertiseData(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2};

        std::vector<u8> data = ctx.ReadBuffer();

        ResultCode rc = lanDiscovery.setAdvertiseData(data.data(), sizeof(data));

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        // TODO: Correct Number
        rb.Push(rc);
    }

    void CreateNetwork(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::RequestParser rp{ctx};
        const auto data{rp.PopRaw<CreateNetworkConfig>()};

        ResultCode rc =
            lanDiscovery.createNetwork(&data.securityConfig, &data.userConfig, &data.networkConfig);

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error! {}", rc.description);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

    void DestroyNetwork(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        int rc = lanDiscovery.destroyNetwork();

        if (rc != 0) {
            LOG_ERROR(Service_LDN, "Error! {}", rc);
        }

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
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

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
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

        // TODO: Correct this?

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

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

        if (rc != RESULT_SUCCESS) {
            LOG_ERROR(Service_LDN, "Error!");
        }

        ctx.WriteBuffer(out_buf_0);
        ctx.WriteBuffer(out_buf_1);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(rc);
    }

private:
    LANDiscovery lanDiscovery;
    Kernel::EventPair state_event = Kernel::WritableEvent::CreateEventPair(
        Core::System::GetInstance().Kernel(), "IUserLocalCommunicationService:StateEvent");
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
            {65000, &LDNU::CreateLdnMitmConfigService, "CreateLdnMitmConfigService"},
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

    struct LdnMitmVersion {
        char raw[32];
    };

    class LdnConfig {
    private:
        enum class CommandId {
            SaveLogToFile = 65000,
            GetVersion = 65001,
            GetLogging = 65002,
            SetLogging = 65003,
            GetEnabled = 65004,
            SetEnabled = 65005,
        };

    public:
        static bool getEnabled() {
            return true;
        }

    protected:
        static std::atomic_bool LdnEnabled;
        /*ResultCode SaveLogToFile();
        ResultCode GetVersion(sf::Out<LdnMitmVersion> version);
        ResultCode GetLogging(sf::Out<u32> enabled);
        ResultCode SetLogging(u32 enabled);
        ResultCode GetEnabled(sf::Out<u32> enabled);
        ResultCode SetEnabled(u32 enabled);
    public:
        DEFINE_SERVICE_DISPATCH_TABLE {
            MAKE_SERVICE_COMMAND_META(SaveLogToFile),
            MAKE_SERVICE_COMMAND_META(GetVersion),
            MAKE_SERVICE_COMMAND_META(GetLogging),
            MAKE_SERVICE_COMMAND_META(SetLogging),
            MAKE_SERVICE_COMMAND_META(GetEnabled),
            MAKE_SERVICE_COMMAND_META(SetEnabled),
        };*/
    };

    void CreateLdnMitmConfigService(Kernel::HLERequestContext& ctx) {
        LOG_CRITICAL(Service_LDN, "called");

        IPC::ResponseBuilder rb{ctx, 2 + 0x30, 0, 1};
        rb.Push(RESULT_SUCCESS);
        LdnConfig ldn_config{};
        rb.PushRaw<LdnConfig>(ldn_config);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LDNM>()->InstallAsService(sm);
    std::make_shared<LDNS>()->InstallAsService(sm);
    std::make_shared<LDNU>()->InstallAsService(sm);
}

} // namespace Service::LDN
