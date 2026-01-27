#include "catch_amalgamated.hpp"
#include "engine/Pathfinding.h"

using namespace Genesis::Engine;

TEST_CASE("Pathfinding: simple straight line", "[pathfinding]") {
    GridGraph grid(5, 1);
    GridCoord start{0, 0};
    GridCoord goal{4, 0};

    auto result = FindPath(grid, start, goal);
    REQUIRE(result.success == true);
    REQUIRE(result.path.front() == start);
    REQUIRE(result.path.back() == goal);
    REQUIRE(result.path.size() == 5);
    REQUIRE(result.cost == Catch::Approx(4.0f));
}

TEST_CASE("Pathfinding: blocked path returns failure", "[pathfinding]") {
    GridGraph grid(3, 1);
    grid.SetWalkable({1, 0}, false);

    auto result = FindPath(grid, {0, 0}, {2, 0});
    REQUIRE(result.success == false);
    REQUIRE(result.path.empty());
}

TEST_CASE("Pathfinding: detour around obstacle", "[pathfinding]") {
    GridGraph grid(4, 3);
    grid.SetWalkable({1, 1}, false);
    grid.SetWalkable({2, 1}, false);

    auto result = FindPath(grid, {0, 1}, {3, 1});
    REQUIRE(result.success == true);
    REQUIRE(result.path.front() == GridCoord{0, 1});
    REQUIRE(result.path.back() == GridCoord{3, 1});
    REQUIRE(result.path.size() > 4);
}

TEST_CASE("Pathfinding: invalid start or goal", "[pathfinding]") {
    GridGraph grid(2, 2);
    auto result = FindPath(grid, {-1, 0}, {1, 1});
    REQUIRE(result.success == false);

    grid.SetWalkable({1, 1}, false);
    result = FindPath(grid, {0, 0}, {1, 1});
    REQUIRE(result.success == false);
}
