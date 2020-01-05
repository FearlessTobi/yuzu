#include "common/logging/log.h"
#include "core/hle/service/ldn/lan_discovery.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

//#include "ipinfo.hpp"

static const int ModuleID = 0xFD;
static const int LdnModuleId = 0xCB;

const char* LANDiscovery::FakeSsid = "12345678123456781234567812345678";
const LANDiscovery::LanEventFunc LANDiscovery::EmptyFunc = []() {};

constexpr ResultCode COMMON_LDN_ERR{ErrorModule::LDN, 32};
constexpr ResultCode LDN_ERR_20{ErrorModule::LDN, 20};
constexpr ResultCode LDN_ERR_10{ErrorModule::LDN, 10};
constexpr ResultCode LDN_ERR_30{ErrorModule::LDN, 30};
constexpr ResultCode LDN_ERR_31{ErrorModule::LDN, 31};
constexpr ResultCode LDN_ERR_50{ErrorModule::LDN, 50};
constexpr ResultCode LDN_ERR_1{ErrorModule::LDN, 1};
constexpr ResultCode LDN_ERR_2{ErrorModule::LDN, 2};
constexpr ResultCode LDN_ERR_4{ErrorModule::LDN, 4};
constexpr ResultCode LDN_ERR_5{ErrorModule::LDN, 5};
constexpr ResultCode LDN_ERR_6{ErrorModule::LDN, 6};
constexpr ResultCode LDN_ERR_7{ErrorModule::LDN, 7};
constexpr ResultCode LDN_ERR_8{ErrorModule::LDN, 8};

// this gets us a different error
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

int LanStation::onRead() {
    if (!this->socket) {
        LOG_CRITICAL(Service_LDN, "Nullptr {}", this->nodeId);
        return -1;
    }
    return this->socket->recvPacket(
        [&](LANPacketType type, const void* data, size_t size, ReplyFunc reply) -> int {
            if (type == LANPacketType::Connect) {
                LOG_CRITICAL(Service_LDN, "on connect");
                NodeInfo* info = (decltype(info))data;
                if (size != sizeof(*info)) {
                    LOG_CRITICAL(Service_LDN, "NodeInfo size is wrong");
                    return -1;
                }
                *this->nodeInfo = *info;
                this->status = NodeStatus::Connected;

                this->discovery->updateNodes();
            } else {
                LOG_CRITICAL(Service_LDN, "unexpecting type {}", static_cast<int>(type));
            }
            return 0;
        });
}

void LanStation::onClose() {
    LOG_CRITICAL(Service_LDN, "LanStation::onClose {}", this->nodeId);
    this->reset();
    this->discovery->updateNodes();
}

LDUdpSocket::LDUdpSocket(int fd, LANDiscovery* discovery)
    : UdpLanSocketBase(fd, discovery->getListenPort()), discovery(discovery) {
    /* ... */
}

int LDUdpSocket::onRead() {
    LOG_CRITICAL(Service_LDN, "LDUdpSocket::onRead");
    return this->recvPacket([&](LANPacketType type, const void* data, size_t size,
                                ReplyFunc reply) -> int {
        switch (type) {
        case LANPacketType::Scan: {
            if (this->discovery->getState() == CommState::AccessPointCreated) {
                reply(LANPacketType::ScanResp, &this->discovery->networkInfo, sizeof(NetworkInfo));
            }
            break;
        }
        case LANPacketType::ScanResp: {
            LOG_CRITICAL(Service_LDN, "ScanResp");
            NetworkInfo* info = (decltype(info))data;
            if (size != sizeof(*info)) {
                break;
            }
            this->scanResults.insert({info->common.bssid, *info});
            break;
        }
        default: {
            LOG_CRITICAL(Service_LDN, "LDUdpSocket::onRead unhandle type {}",
                         static_cast<int>(type));
            break;
        }
        }
        return 0;
    });
}

int LDTcpSocket::onRead() {
    LOG_CRITICAL(Service_LDN, "LDTcpSocket::onRead");
    const auto state = this->discovery->getState();
    if (state == CommState::Station || state == CommState::StationConnected) {
        return this->recvPacket(
            [&](LANPacketType type, const void* data, size_t size, ReplyFunc reply) -> int {
                if (type == LANPacketType::SyncNetwork) {
                    LOG_CRITICAL(Service_LDN, "SyncNetwork");
                    NetworkInfo* info = (decltype(info))data;
                    if (size != sizeof(*info)) {
                        return -1;
                    }

                    this->discovery->onSyncNetwork(info);
                } else {
                    LOG_CRITICAL(Service_LDN, "LDTcpSocket::onRead unhandle type {}",
                                 static_cast<int>(type));
                    return -1;
                }

                return 0;
            });
    } else if (state == CommState::AccessPointCreated) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int new_fd = accept(this->getFd(), (struct sockaddr*)&addr, &addrlen);
        if (new_fd < 0) {
            LOG_CRITICAL(Service_LDN, "accept failed");
            return -1;
        }
        this->discovery->onConnect(new_fd);
        return 0;
    } else {
        LOG_CRITICAL(Service_LDN, "LDTcpSocket::onRead wrong state {}", static_cast<int>(state));
        return -1;
    }
}

void LDTcpSocket::onClose() {
    LOG_CRITICAL(Service_LDN, "LDTcpSocket::onClose");
    this->discovery->onDisconnectFromHost();
}

u32 LDUdpSocket::getBroadcast() {
    u32 address;
    u32 netmask;
    ResultCode rc = ipinfoGetIpConfig(&address, &netmask);
    if (rc != RESULT_SUCCESS) {
        LOG_CRITICAL(Service_LDN, "Broadcast failed to get ip");
        return 0xFFFFFFFF;
    }
    u32 ret = address | ~netmask;
    return ret;
}

void LANDiscovery::onSyncNetwork(NetworkInfo* info) {
    this->networkInfo = *info;
    if (this->state == CommState::Station) {
        this->setState(CommState::StationConnected);
    }
    this->onNetworkInfoChanged();
}

void LANDiscovery::onConnect(int new_fd) {
    LOG_CRITICAL(Service_LDN, "Accepted{}", new_fd);
    if (this->stationCount() >= StationCountMax) {
        LOG_CRITICAL(Service_LDN, "Close new_fd. stations are full");
        closesocket(new_fd);
        return;
    }

    bool found = false;
    for (auto& i : this->stations) {
        if (i.getStatus() == NodeStatus::Disconnected) {
            i.link(new_fd);
            found = true;
            break;
        }
    }

    if (!found) {
        LOG_CRITICAL(Service_LDN, "Close new_fd. no free station found");
        closesocket(new_fd);
    }
}

void LANDiscovery::onDisconnectFromHost() {
    LOG_CRITICAL(Service_LDN, "onDisconnectFromHost state:{}", static_cast<int>(this->state));
    if (this->state == CommState::StationConnected) {
        this->setState(CommState::Station);
    }
}

void LANDiscovery::onNetworkInfoChanged() {
    if (this->isNodeStateChanged()) {
        this->lanEvent();
    }
    return;
}

ResultCode LANDiscovery::setAdvertiseData(const u8* data, uint16_t size) {
    if (size > AdvertiseDataSizeMax) {
        return LDN_ERR_10;
    }

    if (size > 0 && data != nullptr) {
        std::memcpy(this->networkInfo.ldn.advertiseData, data, size);
    } else {
        LOG_CRITICAL(Service_LDN, "LANDiscovery::setAdvertiseData size {}", size);
    }
    this->networkInfo.ldn.advertiseDataSize = size;

    this->updateNodes();

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::initNetworkInfo() {
    ResultCode rc = getFakeMac(&this->networkInfo.common.bssid);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }
    this->networkInfo.common.channel = 6;
    this->networkInfo.common.linkLevel = 3;
    this->networkInfo.common.networkType = 2;
    this->networkInfo.common.ssid = FakeSsid;

    auto nodes = this->networkInfo.ldn.nodes;
    for (int i = 0; i < NodeCountMax; i++) {
        nodes[i].nodeId = i;
        nodes[i].isConnected = 0;
    }

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::getFakeMac(MacAddress* mac) {
    mac->raw[0] = 0x02;
    mac->raw[1] = 0x00;

    u32 ip;
    ResultCode rc = ipinfoGetIpConfig(&ip);
    if (rc == RESULT_SUCCESS) {
        memcpy(mac->raw + 2, &ip, sizeof(ip));
    }

    return rc;
}

ResultCode LANDiscovery::setSocketOpts(int fd) {
    int rc;

    LOG_WARNING(Frontend, "Receiving packet");

    {
        int yess = 1;

        rc = setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const char*)&yess, sizeof(yess));
        if (rc == SOCKET_ERROR) {
            LOG_CRITICAL(Frontend, "ERROR! {}", WSAGetLastError()); // 10042
            // return LDN_ERR_4;
        }
    }
    {
        int yes = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
        if (rc == SOCKET_ERROR) {
            LOG_CRITICAL(Service_LDN, "SO_REUSEADDR failed");
            return LDN_ERR_5;
        }
    }

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::initTcp(bool listening) {
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return LDN_ERR_6;
    }
    auto tcpSocket = std::make_unique<LDTcpSocket>(fd, this);

    if (listening) {
        ResultCode rc = setSocketOpts(fd);
        if (rc != RESULT_SUCCESS) {
            LOG_CRITICAL(Frontend, "setSocketOpts failed!");
            return rc;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htons(INADDR_ANY);
        addr.sin_port = htons(listenPort);
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            LOG_CRITICAL(Frontend, "Bind failed!");
            return LDN_ERR_7;
        }
        if (listen(fd, 10) != 0) {
            LOG_CRITICAL(Frontend, "Bind failed! Other error");
            return LDN_ERR_8;
        }
    }

    {
        std::scoped_lock<std::mutex> lock(this->pollMutex);
        this->tcp = std::move(tcpSocket);
    }

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::initUdp(bool listening) {
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        LOG_CRITICAL(Frontend, "Socket init failed!");
        return LDN_ERR_1;
    }
    auto udpSocket = std::make_unique<LDUdpSocket>(fd, this);

    if (listening) {
        ResultCode rc = setSocketOpts(fd);
        if (rc != RESULT_SUCCESS) {
            LOG_CRITICAL(Frontend, "setSocketOpts failed!");
            return rc;
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htons(INADDR_ANY);
        addr.sin_port = htons(listenPort);
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            LOG_CRITICAL(Frontend, "Bind failed!");
            return LDN_ERR_2;
        }
    }

    {
        std::scoped_lock<std::mutex> lock(this->pollMutex);
        this->udp = std::move(udpSocket);
    }

    return RESULT_SUCCESS;
}

void LANDiscovery::initNodeStateChange() {
    for (auto& i : this->nodeChanges) {
        i.stateChange = NodeStateChange_None;
    }
    for (auto& i : this->nodeLastStates) {
        i = 0;
    }
}

bool LANDiscovery::isNodeStateChanged() {
    bool changed = false;
    const auto& nodes = this->networkInfo.ldn.nodes;
    for (int i = 0; i < NodeCountMax; i++) {
        if (nodes[i].isConnected != this->nodeLastStates[i]) {
            if (nodes[i].isConnected) {
                this->nodeChanges[i].stateChange |= NodeStateChange_Connect;
            } else {
                this->nodeChanges[i].stateChange |= NodeStateChange_Disconnect;
            }
            this->nodeLastStates[i] = nodes[i].isConnected;
            changed = true;
        }
    }
    return changed;
}

void LANDiscovery::Worker(void* args) {
    LANDiscovery* self = (LANDiscovery*)args;

    self->worker();
}

ResultCode LANDiscovery::scan(NetworkInfo* pOutNetwork, u16* count, ScanFilter filter) {
    this->udp->scanResults.clear();

    // TODO: Probably this is the problem
    int len = this->udp->sendBroadcast(LANPacketType::Scan);
    if (len < 0) {
        int error = this->udp->GetLastError();
        LOG_CRITICAL(Frontend, "Socket error! code : {}", error);
        return LDN_ERR_20;
    }

    // svcSleepThread(1000000000L); // 1sec
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    int i = 0;
    auto results = this->udp->scanResults;
    LOG_CRITICAL(Frontend, "Results Size: {}", results.size());
    for (auto& item : results) {
        LOG_WARNING(Frontend, "MAC: STUB");
    }

    for (auto& item : results) {
        if (i >= *count) {
            // Here
            break;
        }
        auto& info = item.second;

        bool copy = true;
        // filter
        if (filter.flag & ScanFilterFlag_LocalCommunicationId) {
            copy &= filter.networkId.intentId.localCommunicationId ==
                    info.networkId.intentId.localCommunicationId;
        }
        if (filter.flag & ScanFilterFlag_SessionId) {
            copy &= filter.networkId.sessionId == info.networkId.sessionId;
        }
        if (filter.flag & ScanFilterFlag_NetworkType) {
            copy &= filter.networkType == info.common.networkType;
        }
        if (filter.flag & ScanFilterFlag_Ssid) {
            copy &= filter.ssid == info.common.ssid;
        }
        if (filter.flag & ScanFilterFlag_SceneId) {
            copy &= filter.networkId.intentId.sceneId == info.networkId.intentId.sceneId;
        }

        if (copy) {
            pOutNetwork[i++] = info;
        }
    }
    *count = i;

    return RESULT_SUCCESS;
}

void LANDiscovery::resetStations() {
    for (auto& i : this->stations) {
        i.reset();
    }
}

int LANDiscovery::stationCount() {
    int count = 0;

    for (auto const& i : this->stations) {
        if (i.getStatus() != NodeStatus::Disconnected) {
            count++;
        }
    }

    return count;
}

void LANDiscovery::updateNodes() {
    int count = 0;
    for (auto& i : this->stations) {
        bool connected = i.getStatus() == NodeStatus::Connected;
        if (connected) {
            count++;
        }
        i.overrideInfo();
    }
    this->networkInfo.ldn.nodeCount = count + 1;

    for (auto& i : stations) {
        if (i.getStatus() == NodeStatus::Connected) {
            int ret = i.sendPacket(LANPacketType::SyncNetwork, &this->networkInfo,
                                   sizeof(this->networkInfo));
            if (ret < 0) {
                LOG_CRITICAL(Service_LDN, "Failed to sendTcp");
            }
        }
    }

    this->onNetworkInfoChanged();
}

int LANDiscovery::loopPoll() {
    int rc;
    if (!inited) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(this->pollMutex);
    const int nfds = 2 + StationCountMax;
    Pollable* fds[nfds];
    fds[0] = this->udp.get();
    fds[1] = this->tcp.get();
    for (int i = 0; i < StationCountMax; i++) {
        fds[2 + i] = this->stations.data() + i;
    }
    rc = Pollable::Poll(fds, nfds);

    return rc;
}

LANDiscovery::~LANDiscovery() {
    LOG_CRITICAL(Service_LDN, "~LANDiscovery");
}

void LANDiscovery::worker() {
    this->stop = false;
    while (!this->stop) {
        int rc = loopPoll();
        if (rc < 0) {
            break;
        }
        // svcSleepThread(0);
    }
    LOG_CRITICAL(Service_LDN, "Worker exit");
    // svcExitThread();
}

ResultCode LANDiscovery::getNetworkInfo(NetworkInfo* pOutNetwork) {
    ResultCode rc = RESULT_SUCCESS;

    if (this->state == CommState::AccessPointCreated ||
        this->state == CommState::StationConnected) {
        std::memcpy(pOutNetwork, &networkInfo, sizeof(networkInfo));
    } else {
        rc = COMMON_LDN_ERR;
    }

    return rc;
}

ResultCode LANDiscovery::getNetworkInfo(NetworkInfo* pOutNetwork, NodeLatestUpdate* pOutUpdates,
                                        int bufferCount) {
    ResultCode rc = RESULT_SUCCESS;

    if (bufferCount < 0 || bufferCount > NodeCountMax) {
        return LDN_ERR_50;
    }

    if (this->state == CommState::AccessPointCreated ||
        this->state == CommState::StationConnected) {
        std::memcpy(pOutNetwork, &networkInfo, sizeof(networkInfo));

        char str[10] = {0};
        for (int i = 0; i < bufferCount; i++) {
            pOutUpdates[i].stateChange = nodeChanges[i].stateChange;
            nodeChanges[i].stateChange = NodeStateChange_None;
            str[i] = '0' + pOutUpdates[i].stateChange;
        }
        LOG_CRITICAL(Service_LDN, "getNetworkInfo updates {}", str);
    } else {
        rc = COMMON_LDN_ERR;
    }

    return rc;
}

ResultCode LANDiscovery::getNodeInfo(NodeInfo* node, const UserConfig* userConfig,
                                     u16 localCommunicationVersion) {
    u32 ipAddress = 0;
    ResultCode rc = ipinfoGetIpConfig(&ipAddress);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }
    rc = getFakeMac(&node->macAddress);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }

    node->isConnected = 1;
    strcpy(node->userName, userConfig->userName);
    node->localCommunicationVersion = localCommunicationVersion;
    node->ipv4Address = ipAddress;

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::createNetwork(const SecurityConfig* securityConfig,
                                       const UserConfig* userConfig,
                                       const NetworkConfig* networkConfig) {
    ResultCode rc = RESULT_SUCCESS;

    if (this->state != CommState::AccessPoint) {
        return COMMON_LDN_ERR;
    }

    rc = this->initTcp(true);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }
    rc = this->initNetworkInfo();
    if (rc != RESULT_SUCCESS) {
        return rc;
    }
    this->networkInfo.ldn.nodeCountMax = networkConfig->nodeCountMax;
    this->networkInfo.ldn.securityMode = securityConfig->securityMode;

    if (networkConfig->channel == 0) {
        this->networkInfo.common.channel = 6;
    } else {
        this->networkInfo.common.channel = networkConfig->channel;
    }
    this->networkInfo.networkId.intentId = networkConfig->intentId;

    NodeInfo* node0 = &this->networkInfo.ldn.nodes[0];
    rc = this->getNodeInfo(node0, userConfig, networkConfig->localCommunicationVersion);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }

    this->setState(CommState::AccessPointCreated);

    this->initNodeStateChange();
    node0->isConnected = 1;
    this->updateNodes();

    return rc;
}

int LANDiscovery::destroyNetwork() {
    if (this->tcp) {
        this->tcp->close();
    }
    this->resetStations();

    this->setState(CommState::AccessPoint);

    return 0;
}

int LANDiscovery::disconnect() {
    if (this->tcp) {
        this->tcp->close();
    }
    this->setState(CommState::Station);

    return 0;
}

ResultCode LANDiscovery::openAccessPoint() {
    if (this->state == CommState::None) {
        return COMMON_LDN_ERR;
    }

    if (this->tcp) {
        this->tcp->close();
    }
    this->resetStations();

    this->setState(CommState::AccessPoint);

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::closeAccessPoint() {
    if (this->state == CommState::None) {
        return COMMON_LDN_ERR;
    }

    if (this->tcp) {
        this->tcp->close();
    }
    this->resetStations();

    this->setState(CommState::Initialized);

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::openStation() {
    if (this->state == CommState::None) {
        return COMMON_LDN_ERR;
    }

    if (this->tcp) {
        this->tcp->close();
    }
    this->resetStations();

    this->setState(CommState::Station);

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::closeStation() {
    if (this->state == CommState::None) {
        return COMMON_LDN_ERR;
    }

    if (this->tcp) {
        this->tcp->close();
    }
    this->resetStations();

    this->setState(CommState::Initialized);

    return RESULT_SUCCESS;
}

ResultCode LANDiscovery::connect(NetworkInfo* networkInfo, UserConfig* userConfig,
                                 u16 localCommunicationVersion) {
    if (networkInfo->ldn.nodeCount == 0) {
        return LDN_ERR_30;
    }

    u32 hostIp = networkInfo->ldn.nodes[0].ipv4Address;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(hostIp);
    addr.sin_port = htons(listenPort);
    LOG_CRITICAL(Service_LDN, "connect hostIp {}", hostIp);

    ResultCode rc = this->initTcp(false);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }

    int ret = ::connect(this->tcp->getFd(), (struct sockaddr*)&addr, sizeof(addr));
    if (ret != 0) {
        LOG_CRITICAL(Service_LDN, "connect failed");
        return LDN_ERR_31;
    }

    NodeInfo myNode = {0};
    rc = this->getNodeInfo(&myNode, userConfig, localCommunicationVersion);
    if (rc != RESULT_SUCCESS) {
        return rc;
    }
    ret = this->tcp->sendPacket(LANPacketType::Connect, &myNode, sizeof(myNode));
    if (ret < 0) {
        LOG_CRITICAL(Service_LDN, "sendPacket failed");
        return COMMON_LDN_ERR;
    }
    this->initNodeStateChange();

    // svcSleepThread(1000000000L); // 1sec*/
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return RESULT_SUCCESS;
}

int LANDiscovery::finalize() {
    if (this->inited) {
        this->stop = true;
        // this->workerThread.Join();
        this->udp.reset();
        this->tcp.reset();
        this->resetStations();
        this->inited = false;
    }

    this->setState(CommState::None);

    return 0;
}

ResultCode LANDiscovery::initialize(LanEventFunc lanEvent, bool listening) {
    if (this->inited) {
        return RESULT_SUCCESS;
    }

    for (auto& i : stations) {
        i.discovery = this;
        i.nodeInfo = &this->networkInfo.ldn.nodes[i.nodeId];
        i.reset();
    }

    this->lanEvent = lanEvent;
    ResultCode rc = initUdp(listening);
    if (rc != RESULT_SUCCESS) {
        LOG_CRITICAL(Service_LDN, "initUdp {}", rc == RESULT_SUCCESS);
        return rc;
    }

    /*if (R_FAILED(this->workerThread.Initialize(&Worker, this, 0x4000, 0x15, 2))) {
         LOG_CRITICAL(Service_LDN, "LANDiscovery Failed to threadCreate");
         return ResultCode(0xF601);
     }
     if (R_FAILED(this->workerThread.Start())) {
         LOG_CRITICAL(Service_LDN, "LANDiscovery Failed to threadStart");
         return ResultCode(0xF601);
     }*/

    this->setState(CommState::Initialized);

    this->inited = true;
    return RESULT_SUCCESS;
}