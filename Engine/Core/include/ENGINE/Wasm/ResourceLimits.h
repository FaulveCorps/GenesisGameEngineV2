#pragma once

#include <cstdint>

namespace Genesis::Engine {

struct ResourceLimits {
    // Maximum memory per module in bytes (default 64 MB)
    std::size_t memory_limit_bytes = 64ull * 1024ull * 1024ull;
    // Maximum execution time for exported calls in milliseconds (default 2000ms)
    uint32_t execution_time_ms = 2000;
};

} // namespace Genesis::Engine
