#include "engine/INetwork.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>
#include <mutex>
#include <deque>

#ifdef HAVE_ENET
#include <enet/enet.h>
#endif

namespace Genesis::Engine {

#ifdef HAVE_ENET

class EnetNetwork : public INetwork {
public:
    EnetNetwork() : m_host(nullptr), m_peer(nullptr), m_isHost(false) {}
    bool Init() override {
        if (enet_initialize() != 0) {
            std::cerr << "EnetNetwork: enet_initialize failed" << std::endl;
            return false;
        }
        std::cout << "EnetNetwork: Init" << std::endl;
        return true;
    }
    void Shutdown() override {
        if (m_host) { enet_host_destroy(m_host); m_host = nullptr; }
        enet_deinitialize();
        std::cout << "EnetNetwork: Shutdown" << std::endl;
    }
    void Update(double dt) override { Poll(dt); }
    std::string Name() const override { return "enet"; }

    bool Host(uint16_t port) override {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;
        m_host = enet_host_create(&address, 32, 2, 0, 0);
        if (!m_host) {
            std::cerr << "EnetNetwork: host create failed" << std::endl;
            return false;
        }
        m_isHost = true;
        return true;
    }

    bool Connect(const std::string& host, uint16_t port) override {
        ENetAddress address;
        if (enet_address_set_host(&address, host.c_str()) != 0) {
            std::cerr << "EnetNetwork: address resolution failed" << std::endl;
            return false;
        }
        address.port = port;
        m_host = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!m_host) return false;
        m_peer = enet_host_connect(m_host, &address, 2, 0);
        if (!m_peer) return false;
        m_isHost = false;
        return true;
    }

    void Poll(double dt) override {
        if (!m_host) return;
        ENetEvent event;
        while (enet_host_service(m_host, &event, static_cast<enet_uint32>(dt * 1000)) > 0) {
            switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                // peer connected
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                Packet data(event.packet->data, event.packet->data + event.packet->dataLength);
                std::lock_guard<std::mutex> lock(m_lock);
                int peerId = event.peer ? 1 : 0; // simplistic
                m_queue.emplace_back(peerId, std::move(data));
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                // peer disconnected
                break;
            default:
                break;
            }
        }
    }

    bool SendToAll(const Packet& data) override {
        if (!m_host) return false;
        ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
        if (!packet) return false;
        if (m_isHost) enet_host_broadcast(m_host, 0, packet);
        else if (m_peer) enet_peer_send(m_peer, 0, packet);
        return true;
    }

    bool Receive(Packet& out, int& peerId) override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_queue.empty()) return false;
        auto p = m_queue.front();
        m_queue.pop_front();
        peerId = p.first;
        out = std::move(p.second);
        return true;
    }

    bool IsHost() const override { return m_isHost; }

private:
    ENetHost* m_host;
    ENetPeer* m_peer;
    bool m_isHost;
    std::mutex m_lock;
    std::deque<std::pair<int, Packet>> m_queue;
};

#endif // HAVE_ENET

static bool register_enet_network = []() {
#ifdef HAVE_ENET
    SubsystemRegistry::Instance().RegisterFactory("Network", "enet", []() {
        return std::make_unique<EnetNetwork>();
    });
#endif
    return true;
}();

void RegisterEnetFactory() {
#ifdef HAVE_ENET
    SubsystemRegistry::Instance().RegisterFactory("Network", "enet", []() {
        return std::make_unique<EnetNetwork>();
    });
#endif
}

} // namespace Genesis::Engine
