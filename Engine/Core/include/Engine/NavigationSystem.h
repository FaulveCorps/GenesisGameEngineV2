#pragma once

#include <vector>
#include "engine/Pathfinding.h"

namespace Genesis::Engine {

class Scene;
struct NavGridComponent;

struct NavPathResult {
    bool success = false;
    float cost = 0.0f;
    std::vector<GridCoord> gridPath;
    std::vector<float> worldPath; // x0, y0, z0, x1, y1, z1...
};

class NavigationSystem {
public:
    static bool WorldToGrid(const NavGridComponent& grid, float worldX, float worldZ, GridCoord& outCoord);
    static void GridToWorld(const NavGridComponent& grid, const GridCoord& coord, float& outX, float& outZ);

    static GridGraph BuildGrid(const Scene& scene, const NavGridComponent& grid, std::vector<uint8_t>* blockedOut = nullptr);
    static NavPathResult FindPath(const Scene& scene, const NavGridComponent& grid,
                                  float startX, float startZ, float goalX, float goalZ);
};

} // namespace Genesis::Engine
