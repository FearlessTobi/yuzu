// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sockets/bsd.h"

namespace Service::Sockets {

struct BsdSocket {
    int Family;
    int Type;
    int Protocol;

    int fd;
};

std::vector<BsdSocket> sockets;

void BSD::RegisterClient(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // bsd errno
}

void BSD::StartMonitoring(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};

    rb.Push(RESULT_SUCCESS);
}

void BSD::WriteBsdResult(Kernel::HLERequestContext& ctx, int result, int errorCode) {
    if (errorCode != 0) {
        result = -1;
        LOG_CRITICAL(Frontend, "BSD ERROR!");
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(RESULT_SUCCESS);

    rb.Push(result);
    rb.Push(errorCode);
}

// TODO: Socket, SSO, Bind

void BSD::Socket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    u32 domain = rp.Pop<u32>();
    u32 type = rp.Pop<u32>();
    u32 protocol = rp.Pop<u32>();

    LOG_WARNING(Service, "called domain={} type={} protocol={}", domain, type, protocol);

    // SocketInternal(false)

    if (type == 5 || type == 3) {
        if (domain != 2 || type != 3 || protocol != 1) {
            WriteBsdResult(ctx, -1, 2); // 2 = Enoent
            return;
        }
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_CRITICAL(Frontend, "Socket error!");
    }
    BsdSocket newBsdSocket{domain, type, protocol, fd};

    sockets.push_back(newBsdSocket);

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(fd);
    rb.Push<u32>(0); // bsd errno
}

void BSD::Connect(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

void BSD::SendTo(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

void BSD::Close(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

void BSD::GetSockOpt(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

int HandleSetSocketOption(Kernel::HLERequestContext& ctx, BsdSocket socket, int optionName,
                          std::vector<u8> buffer) {
    int optionValue = 0;
    std::memcpy(&optionValue, buffer.data(), sizeof(int));

    switch (optionName) {
    case 0x200: // ReuseAddress
        setsockopt(socket.fd, SOL_SOCKET, 4, (char*)&optionValue, sizeof(optionName));
        break;
    case 128: // Linger
        LOG_CRITICAL(Frontend, "SOCKET LINGER UNSUPPORTED!");
        /*setsockopt(socket.fd, SOL_SOCKET, 128,
                   new LingerOption(ctx.Memory.ReadInt32(optionValuePosition) != 0,
                                    ctx.Memory.ReadInt32(optionValuePosition + 4)));*/
        break;
    default:
        setsockopt(socket.fd, SOL_SOCKET, optionName, (char*)&optionValue, sizeof(optionName));
        break;
    }
    LOG_WARNING(Frontend, "Socket Opt - fd: {}, name: {}, optionValue: {}", socket.fd, optionName,
                optionValue);

    return 0;
}

void BSD::SetSockOpt(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "called");

    IPC::RequestParser rp{ctx};
    int socketFd = rp.Pop<u32>();
    int level = rp.Pop<u32>();
    int optionName = rp.Pop<u32>();

    const auto& input_buffer{ctx.ReadBuffer()};

    int _errno = 9;
    bool found = false;
    BsdSocket socket;
    for (int i = 0; i < sockets.size(); i++) {
        socket = sockets[i];
        if (socket.fd = socketFd) {
            found = true;
            break;
        }
    }

    if (found) {
        _errno = 92;

        if (level == 0xFFFF) {
            _errno = HandleSetSocketOption(ctx, socket, optionName, input_buffer);
        } else {
            LOG_WARNING(Service, "Unsupported SetSockOpt Level: {}", level);
        }
    }

    return WriteBsdResult(ctx, 0, _errno);
}

std::tuple<u64, u16> ParseSockAddr(Kernel::HLERequestContext& ctx, std::vector<u8> buffer) {
    // TODO: Wrong?

    u16 port;
    std::memcpy(&port, buffer.data() + 2, sizeof(u16));
    port = _byteswap_ushort(port);

    u32 rawIpNumber;
    std::memcpy(&rawIpNumber, buffer.data() + 4, 4);

    LOG_WARNING(Frontend, "Bind - rawIpNumber: {}, port: {}", rawIpNumber, port);

    return std::make_tuple(rawIpNumber, port);
}

void BSD::Bind(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "called");

    IPC::RequestParser rp{ctx};
    int socketFd = rp.Pop<u32>();

    IPC::ResponseBuilder rb{ctx, 4};

    const auto& input_buffer{ctx.ReadBuffer()};

    int _errno = 9;
    bool found = false;
    BsdSocket socket;
    for (int i = 0; i < sockets.size(); i++) {
        socket = sockets[i];
        if (socket.fd = socketFd) {
            found = true;
            break;
        }
    }

    if (found) {
        _errno = 0;

        auto [rawIpNumber, port] = ParseSockAddr(ctx, input_buffer);

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = rawIpNumber;
        addr.sin_port = htons(port);

        // Missing htons???

        int result = bind(socket.fd, (SOCKADDR*)&addr, sizeof(addr));
        if (result == SOCKET_ERROR) {
            LOG_CRITICAL(Frontend, "Bind failed with error {}", WSAGetLastError());
            closesocket(socket.fd);
            WSACleanup();
            // return WriteBsdResult;
        }
    }

    return WriteBsdResult(ctx, 0, _errno);
}

void BSD::RecvFrom(Kernel::HLERequestContext& ctx) {
    // TODO
    // LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

void BSD::Fcntl(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

BSD::BSD(const char* name) : ServiceFramework(name) {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &BSD::RegisterClient, "RegisterClient"},
        {1, &BSD::StartMonitoring, "StartMonitoring"},
        {2, &BSD::Socket, "Socket"},
        {3, nullptr, "SocketExempt"},
        {4, nullptr, "Open"},
        {5, nullptr, "Select"},
        {6, nullptr, "Poll"},
        {7, nullptr, "Sysctl"},
        {8, nullptr, "Recv"},
        {9, &BSD::RecvFrom, "RecvFrom"},
        {10, nullptr, "Send"},
        {11, &BSD::SendTo, "SendTo"},
        {12, nullptr, "Accept"},
        {13, &BSD::Bind, "Bind"},
        {14, &BSD::Connect, "Connect"},
        {15, nullptr, "GetPeerName"},
        {16, nullptr, "GetSockName"},
        {17, &BSD::GetSockOpt, "GetSockOpt"},
        {18, nullptr, "Listen"},
        {19, nullptr, "Ioctl"},
        {20, &BSD::Fcntl, "Fcntl"},
        {21, &BSD::SetSockOpt, "SetSockOpt"},
        {22, nullptr, "Shutdown"},
        {23, nullptr, "ShutdownAllSockets"},
        {24, nullptr, "Write"},
        {25, nullptr, "Read"},
        {26, &BSD::Close, "Close"},
        {27, nullptr, "DuplicateSocket"},
        {28, nullptr, "GetResourceStatistics"},
        {29, nullptr, "RecvMMsg"},
        {30, nullptr, "SendMMsg"},
        {31, nullptr, "EventFd"},
        {32, nullptr, "RegisterResourceStatisticsName"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

BSD::~BSD() = default;

BSDCFG::BSDCFG() : ServiceFramework{"bsdcfg"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "SetIfUp"},
        {1, nullptr, "SetIfUpWithEvent"},
        {2, nullptr, "CancelIf"},
        {3, nullptr, "SetIfDown"},
        {4, nullptr, "GetIfState"},
        {5, nullptr, "DhcpRenew"},
        {6, nullptr, "AddStaticArpEntry"},
        {7, nullptr, "RemoveArpEntry"},
        {8, nullptr, "LookupArpEntry"},
        {9, nullptr, "LookupArpEntry2"},
        {10, nullptr, "ClearArpEntries"},
        {11, nullptr, "ClearArpEntries2"},
        {12, nullptr, "PrintArpEntries"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

BSDCFG::~BSDCFG() = default;

} // namespace Service::Sockets
