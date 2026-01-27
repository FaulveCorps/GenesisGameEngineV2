#include "engine/NavigationSystem.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Genesis::Engine {

struct NavVec3 {
    float x, y, z;
};

static NavVec3 TransformPoint(const Matrix4& m, const NavVec3& v) {
    return {
        m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12],
        m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13],
        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]
    };
}

bool NavigationSystem::WorldToGrid(const NavGridComponent& grid, float worldX, float worldZ, GridCoord& outCoord) {
    if (grid.cellSize <= 0.0f) return false;
    int gx = static_cast<int>(std::floor((worldX - grid.originX) / grid.cellSize));
    int gy = static_cast<int>(std::floor((worldZ - grid.originZ) / grid.cellSize));
    if (gx < 0 || gy < 0 || gx >= grid.width || gy >= grid.height) return false;
    outCoord = { gx, gy };
    return true;
}

void NavigationSystem::GridToWorld(const NavGridComponent& grid, const GridCoord& coord, float& outX, float& outZ) {
    outX = grid.originX + (static_cast<float>(coord.x) + 0.5f) * grid.cellSize;
    outZ = grid.originZ + (static_cast<float>(coord.y) + 0.5f) * grid.cellSize;
}

GridGraph NavigationSystem::BuildGrid(const Scene& scene, const NavGridComponent& grid, std::vector<uint8_t>* blockedOut) {
    GridGraph graph(grid.width, grid.height);
    if (blockedOut) {
        blockedOut->assign(static_cast<size_t>(grid.width * grid.height), 0);
    }

    if (!grid.autoBakeColliders || grid.width <= 0 || grid.height <= 0 || grid.cellSize <= 0.0f) {
        return graph;
    }

    auto& reg = scene.Registry();
    auto view = reg.view<BoxColliderComponent, Transform>();

    for (auto entity : view) {
        const auto& collider = view.get<BoxColliderComponent>(entity);
        Matrix4 world = scene.GetWorldMatrix(entity);

        float hx = collider.size[0] * 0.5f;
        float hy = collider.size[1] * 0.5f;
        float hz = collider.size[2] * 0.5f;
        float ox = collider.offset[0];
        float oy = collider.offset[1];
        float oz = collider.offset[2];

        NavVec3 corners[8] = {
            {ox - hx, oy - hy, oz - hz},
            {ox + hx, oy - hy, oz - hz},
            {ox + hx, oy + hy, oz - hz},
            {ox - hx, oy + hy, oz - hz},
            {ox - hx, oy - hy, oz + hz},
            {ox + hx, oy - hy, oz + hz},
            {ox + hx, oy + hy, oz + hz},
            {ox - hx, oy + hy, oz + hz}
        };

        float minX = std::numeric_limits<float>::infinity();
        float maxX = -std::numeric_limits<float>::infinity();
        float minZ = std::numeric_limits<float>::infinity();
        float maxZ = -std::numeric_limits<float>::infinity();

        for (const auto& c : corners) {
            NavVec3 wc = TransformPoint(world, c);
            minX = std::min(minX, wc.x);
            maxX = std::max(maxX, wc.x);
            minZ = std::min(minZ, wc.z);
            maxZ = std::max(maxZ, wc.z);
        }

        int minCellX = static_cast<int>(std::floor((minX - grid.originX) / grid.cellSize));
        int maxCellX = static_cast<int>(std::floor((maxX - grid.originX) / grid.cellSize));
        int minCellY = static_cast<int>(std::floor((minZ - grid.originZ) / grid.cellSize));
        int maxCellY = static_cast<int>(std::floor((maxZ - grid.originZ) / grid.cellSize));

        minCellX = std::max(0, std::min(minCellX, grid.width - 1));
        maxCellX = std::max(0, std::min(maxCellX, grid.width - 1));
        minCellY = std::max(0, std::min(minCellY, grid.height - 1));
        maxCellY = std::max(0, std::min(maxCellY, grid.height - 1));

        for (int y = minCellY; y <= maxCellY; ++y) {
            for (int x = minCellX; x <= maxCellX; ++x) {
                GridCoord coord{ x, y };
                graph.SetWalkable(coord, false);
                if (blockedOut) {
                    (*blockedOut)[static_cast<size_t>(y * grid.width + x)] = 1;
                }
            }
        }
    }

    return graph;
}

NavPathResult NavigationSystem::FindPath(const Scene& scene, const NavGridComponent& grid,
                                         float startX, float startZ, float goalX, float goalZ) {
    NavPathResult result;

    GridCoord start;
    GridCoord goal;
    if (!WorldToGrid(grid, startX, startZ, start)) return result;
    if (!WorldToGrid(grid, goalX, goalZ, goal)) return result;

    GridGraph graph = BuildGrid(scene, grid);
    auto pathResult = FindPath(graph, start, goal);
    result.success = pathResult.success;
    result.cost = pathResult.cost;
    result.gridPath = pathResult.path;

    if (result.success) {
        result.worldPath.reserve(result.gridPath.size() * 3);
        for (const auto& coord : result.gridPath) {
            float wx = 0.0f, wz = 0.0f;
            GridToWorld(grid, coord, wx, wz);
            result.worldPath.push_back(wx);
            result.worldPath.push_back(grid.y);
            result.worldPath.push_back(wz);
        }
    }

    return result;
}

void NavigationSystem::UpdateAgents(Scene& scene, double dt) {
    auto& reg = scene.Registry();

    NavGridComponent* navGrid = nullptr;
    auto navView = reg.view<NavGridComponent>();
    for (auto entity : navView) {
        navGrid = &navView.get<NavGridComponent>(entity);
        break;
    }
    if (!navGrid || navGrid->width <= 0 || navGrid->height <= 0 || navGrid->cellSize <= 0.0f) return;

    GridGraph grid = BuildGrid(scene, *navGrid);

    auto view = reg.view<NavAgentComponent, Transform>();
    for (auto entity : view) {
        auto& agent = view.get<NavAgentComponent>(entity);
        auto& transform = view.get<Transform>(entity);

        if (!agent.hasTarget) continue;

        auto* state = reg.try_get<NavAgentState>(entity);
        if (!state) {
            state = &reg.emplace<NavAgentState>(entity);
        }

        state->repathTimer -= static_cast<float>(dt);
        bool needsRepath = state->path.empty() || state->pathIndex >= state->path.size() || state->repathTimer <= 0.0f;

        if (needsRepath) {
            GridCoord start;
            GridCoord goal;
            if (!WorldToGrid(*navGrid, transform.x, transform.z, start)
                || !WorldToGrid(*navGrid, agent.targetX, agent.targetZ, goal)) {
                state->path.clear();
                state->pathIndex = 0;
                state->repathTimer = std::max(0.1f, agent.repathInterval);
                continue;
            }

            auto pathResult = FindPath(grid, start, goal);
            state->path.clear();
            state->pathIndex = 0;
            state->repathTimer = std::max(0.1f, agent.repathInterval);

            if (!pathResult.success) {
                continue;
            }

            state->path.reserve(pathResult.path.size());
            for (const auto& c : pathResult.path) {
                state->path.push_back({c.x, c.y});
            }
        }

        if (state->path.empty() || state->pathIndex >= state->path.size()) {
            float dx = agent.targetX - transform.x;
            float dz = agent.targetZ - transform.z;
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist <= std::max(0.0f, agent.stopDistance)) {
                agent.hasTarget = false;
            }
            continue;
        }

        const auto& next = state->path[state->pathIndex];
        GridCoord nextCoord{next.x, next.y};
        float targetX = 0.0f;
        float targetZ = 0.0f;
        GridToWorld(*navGrid, nextCoord, targetX, targetZ);

        float dx = targetX - transform.x;
        float dz = targetZ - transform.z;
        float dist = std::sqrt(dx * dx + dz * dz);
        float stop = std::max(0.0f, agent.stopDistance);

        if (dist <= stop) {
            state->pathIndex++;
            continue;
        }

        float maxStep = agent.speed * static_cast<float>(dt);
        if (maxStep <= 0.0f || dist <= 1e-5f) continue;
        float step = std::min(dist, maxStep);

        transform.x += (dx / dist) * step;
        transform.z += (dz / dist) * step;
    }
}

} // namespace Genesis::Engine
