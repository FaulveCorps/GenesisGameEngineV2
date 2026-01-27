#include "catch_amalgamated.hpp"
#include "engine/Profiler.h"
#include "engine/JobSystem.h"

using namespace Genesis::Engine;

TEST_CASE("Profiler captures job system stats", "[profiler]") {
    JobSystem::Shutdown();
    JobSystem::Start(2);

    Profiler profiler;
    profiler.BeginFrame();

    REQUIRE(profiler.GetJobWorkerCount() == 2);
    REQUIRE(profiler.GetJobQueuedCount() >= 0);
    REQUIRE(profiler.GetJobActiveCount() >= 0);

    JobSystem::Shutdown();
}

TEST_CASE("Profiler records frame allocator usage", "[profiler]") {
    Profiler profiler;
    profiler.SetFrameAllocatorUsage(128, 1024);

    REQUIRE(profiler.GetFrameAllocatorUsed() == 128);
    REQUIRE(profiler.GetFrameAllocatorCapacity() == 1024);
}
