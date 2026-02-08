#include "engine/NavigationSystem.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/MathUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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

static uint64_t HashCombine(uint64_t h, uint64_t v) {
    constexpr uint64_t kFnvPrime = 1099511628211ULL;
    return (h ^ v) * kFnvPrime;
}

static uint64_t HashFloat(uint64_t h, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    return HashCombine(h, static_cast<uint64_t>(bits));
}

static uint64_t HashMatrix(uint64_t h, const Matrix4& m) {
    for (int i = 0; i < 16; ++i) {
        h = HashFloat(h, m.m[i]);
    }
    return h;
}

static uint64_t HashColliders(const Scene& scene) {
    constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
    uint64_t h = kFnvOffset;
    size_t count = 0;

    auto& reg = scene.Registry();
    auto view = reg.view<BoxColliderComponent, Transform>();
    for (auto entity : view) {
        const auto& collider = view.get<BoxColliderComponent>(entity);
        Matrix4 world = scene.GetWorldMatrix(entity);

        h = HashMatrix(h, world);
        for (int i = 0; i < 3; ++i) {
            h = HashFloat(h, collider.size[i]);
        }
        for (int i = 0; i < 3; ++i) {
            h = HashFloat(h, collider.offset[i]);
        }
        h = HashCombine(h, collider.isTrigger ? 1ULL : 0ULL);
        ++count;
    }

    auto sphereView = reg.view<SphereColliderComponent, Transform>();
    for (auto entity : sphereView) {
        const auto& collider = sphereView.get<SphereColliderComponent>(entity);
        Matrix4 world = scene.GetWorldMatrix(entity);

        h = HashMatrix(h, world);
        h = HashFloat(h, collider.radius);
        for (int i = 0; i < 3; ++i) {
            h = HashFloat(h, collider.offset[i]);
        }
        h = HashCombine(h, collider.isTrigger ? 1ULL : 0ULL);
        ++count;
    }

    h = HashCombine(h, static_cast<uint64_t>(count));
    return h;
}

static bool NavGridConfigChanged(const NavGridComponent& grid, const NavGridState& state) {
    return state.cachedWidth != grid.width
        || state.cachedHeight != grid.height
        || state.cachedCellSize != grid.cellSize
        || state.cachedOriginX != grid.originX
        || state.cachedOriginZ != grid.originZ
        || state.cachedY != grid.y
        || state.cachedAutoBake != grid.autoBakeColliders;
}

static void UpdateNavGridCache(const Scene& scene, const NavGridComponent& grid, NavGridState& state) {
    const bool invalidGrid = grid.width <= 0 || grid.height <= 0 || grid.cellSize <= 0.0f;
    const bool configChanged = NavGridConfigChanged(grid, state);

    uint64_t colliderHash = 0;
    if (!invalidGrid && grid.autoBakeColliders) {
        colliderHash = HashColliders(scene);
    }

    const bool needsRebuild = state.dirty || configChanged
        || (grid.autoBakeColliders && colliderHash != state.collidersHash);

    if (needsRebuild) {
        if (invalidGrid) {
            state.grid = GridGraph(0, 0);
            state.blocked.clear();
        } else {
            state.grid = NavigationSystem::BuildGrid(scene, grid, &state.blocked);
        }

        state.cachedWidth = grid.width;
        state.cachedHeight = grid.height;
        state.cachedCellSize = grid.cellSize;
        state.cachedOriginX = grid.originX;
        state.cachedOriginZ = grid.originZ;
        state.cachedY = grid.y;
        state.cachedAutoBake = grid.autoBakeColliders;
        state.collidersHash = colliderHash;
        state.dirty = false;
    }
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
    if (grid.width <= 0 || grid.height <= 0 || grid.cellSize <= 0.0f) {
        if (blockedOut) blockedOut->clear();
        return GridGraph(0, 0);
    }

    GridGraph graph(grid.width, grid.height);
    if (blockedOut) {
        blockedOut->assign(static_cast<size_t>(grid.width * grid.height), 0);
    }

    if (!grid.autoBakeColliders) {
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

    auto sphereView = reg.view<SphereColliderComponent, Transform>();
    for (auto entity : sphereView) {
        const auto& collider = sphereView.get<SphereColliderComponent>(entity);
        Matrix4 world = scene.GetWorldMatrix(entity);

        NavVec3 centerLocal{collider.offset[0], collider.offset[1], collider.offset[2]};
        NavVec3 centerWorld = TransformPoint(world, centerLocal);

        // Approximate world scale from matrix columns; use max scale for conservative radius.
        float sx = std::sqrt(world.m[0] * world.m[0] + world.m[1] * world.m[1] + world.m[2] * world.m[2]);
        float sy = std::sqrt(world.m[4] * world.m[4] + world.m[5] * world.m[5] + world.m[6] * world.m[6]);
        float sz = std::sqrt(world.m[8] * world.m[8] + world.m[9] * world.m[9] + world.m[10] * world.m[10]);
        float scale = std::max(sx, std::max(sy, sz));
        float radius = collider.radius * scale;

        float minX = centerWorld.x - radius;
        float maxX = centerWorld.x + radius;
        float minZ = centerWorld.z - radius;
        float maxZ = centerWorld.z + radius;

        int minCellX = static_cast<int>(std::floor((minX - grid.originX) / grid.cellSize));
        int maxCellX = static_cast<int>(std::floor((maxX - grid.originX) / grid.cellSize));
        int minCellY = static_cast<int>(std::floor((minZ - grid.originZ) / grid.cellSize));
        int maxCellY = static_cast<int>(std::floor((maxZ - grid.originZ) / grid.cellSize));

        minCellX = std::max(0, std::min(minCellX, grid.width - 1));
        maxCellX = std::max(0, std::min(maxCellX, grid.width - 1));
        minCellY = std::max(0, std::min(minCellY, grid.height - 1));
        maxCellY = std::max(0, std::min(maxCellY, grid.height - 1));

        float radiusSq = radius * radius;
        for (int y = minCellY; y <= maxCellY; ++y) {
            for (int x = minCellX; x <= maxCellX; ++x) {
                float cellMinX = grid.originX + static_cast<float>(x) * grid.cellSize;
                float cellMinZ = grid.originZ + static_cast<float>(y) * grid.cellSize;
                float cellMaxX = cellMinX + grid.cellSize;
                float cellMaxZ = cellMinZ + grid.cellSize;

                float closestX = std::clamp(centerWorld.x, cellMinX, cellMaxX);
                float closestZ = std::clamp(centerWorld.z, cellMinZ, cellMaxZ);
                float dx = centerWorld.x - closestX;
                float dz = centerWorld.z - closestZ;
                if (dx * dx + dz * dz > radiusSq) continue;
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
    auto pathResult = ::Genesis::Engine::FindPath(graph, start, goal);
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

void NavigationSystem::EnsureNavGridCache(const Scene& scene, const NavGridComponent& grid, NavGridState& state) {
    UpdateNavGridCache(scene, grid, state);
}

void NavigationSystem::UpdateAgents(Scene& scene, double dt) {
    auto& reg = scene.Registry();

    NavGridComponent* navGrid = nullptr;
    entt::entity navEntity = entt::null;
    auto navView = reg.view<NavGridComponent>();
    for (auto entity : navView) {
        navGrid = &navView.get<NavGridComponent>(entity);
        navEntity = entity;
        break;
    }
    if (!navGrid || navGrid->width <= 0 || navGrid->height <= 0 || navGrid->cellSize <= 0.0f) return;

    auto* navState = reg.try_get<NavGridState>(navEntity);
    if (!navState) {
        navState = &reg.emplace<NavGridState>(navEntity);
    }
    EnsureNavGridCache(scene, *navGrid, *navState);
    const GridGraph& grid = navState->grid;
    if (grid.Width() <= 0 || grid.Height() <= 0) return;

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

            auto pathResult = ::Genesis::Engine::FindPath(grid, start, goal);
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
