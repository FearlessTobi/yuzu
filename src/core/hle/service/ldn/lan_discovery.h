#pragma once
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <stdint.h>
#include "core/hle/kernel/errors.h"
#include "core/hle/service/ldn/lan_protocol.h"
#include "core/hle/service/ldn/ldn_types.h"

enum class NodeStatus : u8 {
    Disconnected,
    Connect,
    Connected,
};

class LANDiscovery;

class LanStation : public Pollable {
protected:
    friend class LANDiscovery;
    NodeInfo* nodeInfo;
    NodeStatus status;
    std::unique_ptr<TcpLanSocketBase> socket;
    int nodeId;
    LANDiscovery* discovery;

public:
    LanStation(int nodeId, LANDiscovery* discovery)
        : nodeInfo(nullptr), status(NodeStatus::Disconnected), nodeId(nodeId),
          discovery(discovery) {}
    NodeStatus getStatus() const {
        return this->status;
    }
    void reset() {
        this->socket.reset();
        this->status = NodeStatus::Disconnected;
    };
    void link(int fd) {
        this->socket = std::make_unique<TcpLanSocketBase>(fd);
        this->status = NodeStatus::Connect;
    };
    int getFd() override {
        if (!this->socket) {
            return -1;
        }
        return this->socket->getFd();
    };
    int onRead() override;
    void onClose() override;
    int sendPacket(LANPacketType type, const void* data, size_t size) {
        if (!this->socket) {
            return -1;
        }
        return this->socket->sendPacket(type, data, size);
    };
    void overrideInfo() {
        bool connected = this->getStatus() == NodeStatus::Connected;
        this->nodeInfo->nodeId = this->nodeId;
        if (connected) {
            this->nodeInfo->isConnected = 1;
        } else {
            this->nodeInfo->isConnected = 0;
        }
    }
};

class LDUdpSocket : public UdpLanSocketBase, public Pollable {
protected:
    struct MacHash {
        std::size_t operator()(const MacAddress& t) const {
            return *reinterpret_cast<const u32*>(t.raw + 2);
        }
    };
    virtual u32 getBroadcast() override;
    LANDiscovery* discovery;

public:
    std::unordered_map<MacAddress, NetworkInfo, MacHash> scanResults;

public:
    LDUdpSocket(int fd, LANDiscovery* discovery);
    int getFd() override {
        return UdpLanSocketBase::getFd();
    }
    int onRead() override;
    void onClose() override {
        LOG_CRITICAL(Service_LDN, "LDUdpSocket::onClose");
    };
};

class LDTcpSocket : public TcpLanSocketBase, public Pollable {
protected:
    LANDiscovery* discovery;

public:
    LDTcpSocket(int fd, LANDiscovery* discovery) : TcpLanSocketBase(fd), discovery(discovery){};
    int getFd() override {
        return TcpLanSocketBase::getFd();
    }
    int onRead() override;
    void onClose() override;
};

class LANDiscovery {
public:
    static const int DefaultPort = 11452;
    static const char* FakeSsid;
    typedef std::function<int(LANPacketType, const void*, size_t)> ReplyFunc;
    typedef std::function<void()> LanEventFunc;
    static const LanEventFunc EmptyFunc;

protected:
    friend class LDUdpSocket;
    friend class LDTcpSocket;
    friend class LanStation;
    // 0: udp 1: tcp 2: client
    std::mutex pollMutex;
    std::unique_ptr<LDUdpSocket> udp;
    std::unique_ptr<LDTcpSocket> tcp;
    std::array<LanStation, StationCountMax> stations;
    std::array<NodeLatestUpdate, NodeCountMax> nodeChanges;
    std::array<u8, NodeCountMax> nodeLastStates;
    static void Worker(void* args);
    bool stop;
    bool inited;
    NetworkInfo networkInfo;
    u16 listenPort;
    // HosThread workerThread;
    CommState state;
    void worker();
    int loopPoll();
    void onSyncNetwork(NetworkInfo* info);
    void onConnect(int new_fd);
    void onDisconnectFromHost();
    void onNetworkInfoChanged();

    void updateNodes();
    void resetStations();
    ResultCode getFakeMac(MacAddress* mac);
    ResultCode getNodeInfo(NodeInfo* node, const UserConfig* userConfig,
                           u16 localCommunicationVersion);
    LanEventFunc lanEvent;

public:
    ResultCode initialize(LanEventFunc lanEvent = EmptyFunc, bool listening = true);
    int finalize();
    ResultCode initNetworkInfo();
    ResultCode scan(NetworkInfo* networkInfo, u16* count, ScanFilter filter);
    ResultCode setAdvertiseData(const u8* data, uint16_t size);
    ResultCode createNetwork(const SecurityConfig* securityConfig, const UserConfig* userConfig,
                             const NetworkConfig* networkConfig);
    int destroyNetwork();
    ResultCode connect(NetworkInfo* networkInfo, UserConfig* userConfig,
                       u16 localCommunicationVersion);
    int disconnect();
    ResultCode getNetworkInfo(NetworkInfo* pOutNetwork);
    ResultCode getNetworkInfo(NetworkInfo* pOutNetwork, NodeLatestUpdate* pOutUpdates,
                              int bufferCount);
    ResultCode openAccessPoint();
    ResultCode closeAccessPoint();
    ResultCode openStation();
    ResultCode closeStation();

public:
    LANDiscovery(u16 port = DefaultPort)
        : stations({{{1, this}, {2, this}, {3, this}, {4, this}, {5, this}, {6, this}, {7, this}}}),
          stop(false), inited(false), networkInfo({0}), listenPort(port), state(CommState::None) {
        LOG_CRITICAL(Service_LDN, "LANDiscovery");
    };
    ~LANDiscovery();
    u16 getListenPort() const {
        return this->listenPort;
    }
    CommState getState() const {
        return this->state;
    };
    void setState(CommState v) {
        this->state = v;
        this->lanEvent();
    };
    int stationCount();

protected:
    ResultCode setSocketOpts(int fd);
    ResultCode initTcp(bool listening);
    ResultCode initUdp(bool listening);
    void initNodeStateChange();
    bool isNodeStateChanged();
};
