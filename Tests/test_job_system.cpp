#include "catch_amalgamated.hpp"
#include "engine/JobSystem.h"

#include <atomic>
#include <cstdint>

using namespace Genesis::Engine;

TEST_CASE("JobSystem executes enqueued jobs", "[jobs]") {
    JobSystem::Start(2);

    std::atomic<int> sum{0};
    std::vector<JobSystem::JobHandle> handles;
    handles.reserve(16);

    for (int i = 0; i < 16; ++i) {
        handles.push_back(JobSystem::Enqueue([&sum]() {
            sum.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    for (const auto& h : handles) {
        h.Wait();
    }

    REQUIRE(sum.load() == 16);
    JobSystem::Shutdown();
}

TEST_CASE("JobSystem ParallelFor covers range", "[jobs]") {
    JobSystem::Start(2);

    const uint32_t count = 100;
    std::atomic<uint64_t> total{0};

    auto handle = JobSystem::ParallelFor(count, 8, [&total](uint32_t i) {
        total.fetch_add(i, std::memory_order_relaxed);
    });

    handle.Wait();

    const uint64_t expected = (static_cast<uint64_t>(count) - 1u) * static_cast<uint64_t>(count) / 2u;
    REQUIRE(total.load() == expected);

    JobSystem::Shutdown();
}
