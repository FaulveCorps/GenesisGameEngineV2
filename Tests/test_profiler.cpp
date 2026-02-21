#include "catch_amalgamated.hpp"
#include "engine/Profiler.h"
#include "engine/JobSystem.h"
#include "engine/Stats.h"

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

TEST_CASE("Profiler stores frame history samples", "[profiler]") {
    Profiler profiler;
    profiler.BeginFrame();
    {
        Profiler::Scope scope(profiler, "TestScope");
    }
    profiler.EndFrame();

    const auto& history = profiler.GetFrameHistory();
    REQUIRE(history.size() == 1);
    REQUIRE(history.back().samples.size() == 1);
    REQUIRE(history.back().samples.back().name == "TestScope");
}

TEST_CASE("Profiler captures draw call stats", "[profiler]") {
    Profiler profiler;
    Stats::Reset();

    profiler.BeginFrame();
    Stats::AddDrawCalls(7);
    profiler.EndFrame();

    REQUIRE(profiler.GetDrawCalls() == 7);
    REQUIRE(profiler.GetFrameHistory().back().drawCalls == 7);
}
