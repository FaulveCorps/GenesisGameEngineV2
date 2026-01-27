#include "catch_amalgamated.hpp"
#include "engine/FrameAllocator.h"

#include <cstdint>

using namespace Genesis::Engine;

TEST_CASE("FrameAllocator basic allocation and alignment", "[framealloc]") {
    FrameAllocator alloc(64);

    void* p1 = alloc.Allocate(8, 8);
    REQUIRE(p1 != nullptr);
    REQUIRE(reinterpret_cast<uintptr_t>(p1) % 8 == 0);

    void* p2 = alloc.Allocate(16, 16);
    REQUIRE(p2 != nullptr);
    REQUIRE(reinterpret_cast<uintptr_t>(p2) % 16 == 0);

    REQUIRE(alloc.GetUsed() <= alloc.GetCapacity());
    REQUIRE(alloc.Owns(p1));
    REQUIRE(alloc.Owns(p2));
}

TEST_CASE("FrameAllocator reset reuses memory", "[framealloc]") {
    FrameAllocator alloc(64);
    void* first = alloc.Allocate(8, 8);
    REQUIRE(first != nullptr);
    alloc.Reset();
    REQUIRE(alloc.GetUsed() == 0);

    void* second = alloc.Allocate(8, 8);
    REQUIRE(second == first);
}

TEST_CASE("FrameAllocator out-of-memory returns null", "[framealloc]") {
    FrameAllocator alloc(32);
    void* first = alloc.Allocate(24, 1);
    REQUIRE(first != nullptr);

    size_t usedBefore = alloc.GetUsed();
    void* second = alloc.Allocate(16, 1);
    REQUIRE(second == nullptr);
    REQUIRE(alloc.GetUsed() == usedBefore);
}
