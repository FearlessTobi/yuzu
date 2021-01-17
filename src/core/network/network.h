// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "common/common_types.h"

namespace Network {

class Socket;

/// Error code for network functions
enum class Errno {
    SUCCESS,
    BADF,
    INVAL,
    MFILE,
    NOTCONN,
    AGAIN,
};

/// Address families
enum class Domain {
    UNSPECIFIED, ///< Unspecified family
    INET,        ///< Address family for IPv4
};

/// Socket types
enum class Type {
    UNSPECIFIED,
    STREAM,
    DGRAM,
    RAW,
    SEQPACKET,
};

/// Protocol values for sockets
enum class Protocol {
    UNSPECIFIED,
    ICMP,
    TCP,
    UDP,
};

/// Shutdown mode
enum class ShutdownHow {
    RD,
    WR,
    RDWR,
};

/// Array of IPv4 address
using IPv4Address = std::array<u8, 4>;

/// Cross-platform sockaddr structure
struct SockAddrIn {
    Domain family;
    IPv4Address ip;
    u16 portno;
};

/// Cross-platform poll fd structure
struct PollFD {
    Socket* socket;
    u16 events;
    u16 revents;
};

/// Cross-platform hostent representation
struct HostEnt {
    std::string name;
    std::vector<std::string> aliases;
    std::vector<IPv4Address> addr_list;
    Domain addr_type;
};

/// Cross-platform addrinfo node representation
struct AddrInfo {
    u32 flags;
    Domain family;
    Type socket_type;
    Protocol protocol;
    SockAddrIn addr;
    std::string canonname;
};

constexpr u16 POLL_IN = 1 << 0;
constexpr u16 POLL_PRI = 1 << 1;
constexpr u16 POLL_OUT = 1 << 2;
constexpr u16 POLL_ERR = 1 << 3;
constexpr u16 POLL_HUP = 1 << 4;
constexpr u16 POLL_NVAL = 1 << 5;
constexpr u16 POLL_RDNORM = 1 << 6;
constexpr u16 POLL_RDBAND = 1 << 7;
constexpr u16 POLL_WRBAND = 1 << 8;

class NetworkInstance {
public:
    explicit NetworkInstance();
    ~NetworkInstance();
};

/// @brief Returns host's IPv4 address
/// @return Pair of an array of human ordered IPv4 address (e.g. 192.168.0.1) and an error code
std::pair<IPv4Address, Errno> GetHostIPv4Address();

/// @brief Retrieves host information corresponding to a host name from a host database
/// @return Pair of host information and an error code
std::pair<HostEnt, Errno> GetHostByName(const char* name);

/// @brief Retrieves host information corresponding to a network address
/// @return Pair of host information and an error code
std::pair<HostEnt, Errno> GetHostByAddr(const char* addr, int len, Domain type);

/// @brief Provides protocol independent translation from ANSI host name to an address
/// @return Pair of vector host address information and an error code
std::pair<std::vector<AddrInfo>, Errno> GetAddressInfo(const char* node, const char* service,
                                                       const std::vector<AddrInfo>& hints);

} // namespace Network
