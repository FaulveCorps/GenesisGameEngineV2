#pragma once

#include "engine/ISubsystem.h"
#include <vector>
#include <cstdint>
#include <string>

namespace Genesis::Engine {

class INetwork : public ISubsystem {
public:
    using Packet = std::vector<uint8_t>;
    virtual ~INetwork() = default;

    // Host a server on the given port. Returns true on success.
    virtual bool Host(uint16_t port) = 0;

    // Connect to a remote host:port. Returns true if connection started/established.
    virtual bool Connect(const std::string& host, uint16_t port) = 0;

    // Poll network events (should be called regularly)
    virtual void Poll(double dt) = 0;

    // Send data to all peers. Returns true on success.
    virtual bool SendToAll(const Packet& data) = 0;

    // Receive a single packet if available; returns true and sets out/peerId
    virtual bool Receive(Packet& out, int& peerId) = 0;

    // Query whether this instance is hosting
    virtual bool IsHost() const = 0;
};

} // namespace Genesis::Engine
