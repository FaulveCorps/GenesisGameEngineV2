#include "engine/DebugRenderer.h"
#include "engine/Components.h"
#include "engine/NavigationSystem.h"
#include "engine/MathUtils.h"
#include <vector>
#include <cmath>

namespace Genesis::Engine {

struct Vec3 { float x, y, z; };

static Vec3 TransformPoint(const Matrix4& m, const Vec3& v) {
    float x = m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12];
    float y = m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13];
    float z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14];
    return {x, y, z};
}

void DebugRenderer::Render(Scene& scene, IGraphicsAPI* renderer) {
    if (!renderer) return;

    std::vector<float> vertices;
    std::vector<float> colors;

    auto& registry = scene.Registry();

    // BoxCollider
    auto boxView = registry.view<BoxColliderComponent, Transform>();
    for (auto entity : boxView) {
        auto& collider = boxView.get<BoxColliderComponent>(entity);
        Matrix4 worldParams = scene.GetWorldMatrix(entity);

        float hx = collider.size[0] * 0.5f;
        float hy = collider.size[1] * 0.5f;
        float hz = collider.size[2] * 0.5f;

        float ox = collider.offset[0];
        float oy = collider.offset[1];
        float oz = collider.offset[2];

        Vec3 corners[8] = {
            {ox - hx, oy - hy, oz - hz},
            {ox + hx, oy - hy, oz - hz},
            {ox + hx, oy + hy, oz - hz},
            {ox - hx, oy + hy, oz - hz},
            {ox - hx, oy - hy, oz + hz},
            {ox + hx, oy - hy, oz + hz},
            {ox + hx, oy + hy, oz + hz},
            {ox - hx, oy + hy, oz + hz}
        };

        Vec3 tCorners[8];
        for (int i=0; i<8; ++i) tCorners[i] = TransformPoint(worldParams, corners[i]);

        auto addLine = [&](int i, int j) {
            vertices.push_back(tCorners[i].x); vertices.push_back(tCorners[i].y); vertices.push_back(tCorners[i].z);
            vertices.push_back(tCorners[j].x); vertices.push_back(tCorners[j].y); vertices.push_back(tCorners[j].z);
            colors.push_back(0.0f); colors.push_back(1.0f); colors.push_back(0.0f);
            colors.push_back(0.0f); colors.push_back(1.0f); colors.push_back(0.0f);
        };

        // Bottom
        addLine(0, 1); addLine(1, 2); addLine(2, 3); addLine(3, 0);
        // Top
        addLine(4, 5); addLine(5, 6); addLine(6, 7); addLine(7, 4);
        // Sides
        addLine(0, 4); addLine(1, 5); addLine(2, 6); addLine(3, 7);
    }

    // SphereCollider
    auto sphereView = registry.view<SphereColliderComponent, Transform>();
    const int segments = 24;
    const float step = 6.28318530718f / segments;

    for (auto entity : sphereView) {
        auto& collider = sphereView.get<SphereColliderComponent>(entity);
        Matrix4 worldParams = scene.GetWorldMatrix(entity);

        float r = collider.radius;
        float ox = collider.offset[0];
        float oy = collider.offset[1];
        float oz = collider.offset[2];
        Vec3 center = {ox, oy, oz};

        auto addSegment = [&](Vec3 p1, Vec3 p2) {
            Vec3 tp1 = TransformPoint(worldParams, p1);
            Vec3 tp2 = TransformPoint(worldParams, p2);
            vertices.push_back(tp1.x); vertices.push_back(tp1.y); vertices.push_back(tp1.z);
            vertices.push_back(tp2.x); vertices.push_back(tp2.y); vertices.push_back(tp2.z);
            colors.push_back(0.0f); colors.push_back(1.0f); colors.push_back(0.0f);
            colors.push_back(0.0f); colors.push_back(1.0f); colors.push_back(0.0f);
        };

        // XY Circle
        for (int i = 0; i < segments; ++i) {
            float angle = i * step;
            float nextAngle = (i + 1) * step;
            Vec3 p1 = { center.x + std::cos(angle) * r, center.y + std::sin(angle) * r, center.z };
            Vec3 p2 = { center.x + std::cos(nextAngle) * r, center.y + std::sin(nextAngle) * r, center.z };
            addSegment(p1, p2);
        }
        // XZ Circle
        for (int i = 0; i < segments; ++i) {
            float angle = i * step;
            float nextAngle = (i + 1) * step;
            Vec3 p1 = { center.x + std::cos(angle) * r, center.y, center.z + std::sin(angle) * r };
            Vec3 p2 = { center.x + std::cos(nextAngle) * r, center.y, center.z + std::sin(nextAngle) * r };
            addSegment(p1, p2);
        }
        // YZ Circle
        for (int i = 0; i < segments; ++i) {
            float angle = i * step;
            float nextAngle = (i + 1) * step;
            Vec3 p1 = { center.x, center.y + std::cos(angle) * r, center.z + std::sin(angle) * r };
            Vec3 p2 = { center.x, center.y + std::cos(nextAngle) * r, center.z + std::sin(nextAngle) * r };
            addSegment(p1, p2);
        }
    }

    // Navigation grid debug
    const NavGridComponent* navGrid = nullptr;
    auto navView = registry.view<NavGridComponent>();
    for (auto entity : navView) {
        const auto& nav = navView.get<NavGridComponent>(entity);
        navGrid = &nav;
        if (!nav.drawDebug || nav.width <= 0 || nav.height <= 0 || nav.cellSize <= 0.0f) continue;
        auto* navState = registry.try_get<NavGridState>(entity);
        if (!navState) {
            navState = &registry.emplace<NavGridState>(entity);
        }
        NavigationSystem::EnsureNavGridCache(scene, nav, *navState);
        const auto& blocked = navState->blocked;

        auto addLineColor = [&](const Vec3& a, const Vec3& b, float r, float g, float bcol) {
            vertices.push_back(a.x); vertices.push_back(a.y); vertices.push_back(a.z);
            vertices.push_back(b.x); vertices.push_back(b.y); vertices.push_back(b.z);
            colors.push_back(r); colors.push_back(g); colors.push_back(bcol);
            colors.push_back(r); colors.push_back(g); colors.push_back(bcol);
        };

        float x0 = nav.originX;
        float z0 = nav.originZ;
        float y = nav.y;
        float w = nav.width * nav.cellSize;
        float h = nav.height * nav.cellSize;

        // Grid lines (blue)
        for (int x = 0; x <= nav.width; ++x) {
            float xx = x0 + x * nav.cellSize;
            addLineColor({xx, y, z0}, {xx, y, z0 + h}, 0.2f, 0.6f, 1.0f);
        }
        for (int z = 0; z <= nav.height; ++z) {
            float zz = z0 + z * nav.cellSize;
            addLineColor({x0, y, zz}, {x0 + w, y, zz}, 0.2f, 0.6f, 1.0f);
        }

        // Blocked cells (red)
        if (!blocked.empty()) {
            for (int gy = 0; gy < nav.height; ++gy) {
                for (int gx = 0; gx < nav.width; ++gx) {
                    if (!blocked[static_cast<size_t>(gy * nav.width + gx)]) continue;
                    float cx = x0 + gx * nav.cellSize;
                    float cz = z0 + gy * nav.cellSize;
                    Vec3 p0{cx, y, cz};
                    Vec3 p1{cx + nav.cellSize, y, cz};
                    Vec3 p2{cx + nav.cellSize, y, cz + nav.cellSize};
                    Vec3 p3{cx, y, cz + nav.cellSize};
                    addLineColor(p0, p1, 1.0f, 0.2f, 0.2f);
                    addLineColor(p1, p2, 1.0f, 0.2f, 0.2f);
                    addLineColor(p2, p3, 1.0f, 0.2f, 0.2f);
                    addLineColor(p3, p0, 1.0f, 0.2f, 0.2f);
                }
            }
        }

        if (nav.debugPath && navState->grid.Width() > 0 && navState->grid.Height() > 0) {
            GridCoord start;
            GridCoord goal;
            if (NavigationSystem::WorldToGrid(nav, nav.debugStartX, nav.debugStartZ, start)
                && NavigationSystem::WorldToGrid(nav, nav.debugEndX, nav.debugEndZ, goal)) {
                auto pathResult = ::Genesis::Engine::FindPath(navState->grid, start, goal);
                if (pathResult.success && pathResult.path.size() > 1) {
                    float prevX = 0.0f;
                    float prevZ = 0.0f;
                    NavigationSystem::GridToWorld(nav, {pathResult.path[0].x, pathResult.path[0].y}, prevX, prevZ);
                    for (size_t i = 1; i < pathResult.path.size(); ++i) {
                        float nextX = 0.0f;
                        float nextZ = 0.0f;
                        NavigationSystem::GridToWorld(nav, {pathResult.path[i].x, pathResult.path[i].y}, nextX, nextZ);
                        vertices.push_back(prevX); vertices.push_back(y + 0.05f); vertices.push_back(prevZ);
                        vertices.push_back(nextX); vertices.push_back(y + 0.05f); vertices.push_back(nextZ);
                        colors.push_back(1.0f); colors.push_back(0.2f); colors.push_back(1.0f);
                        colors.push_back(1.0f); colors.push_back(0.2f); colors.push_back(1.0f);
                        prevX = nextX;
                        prevZ = nextZ;
                    }
                }
            }
        }
    }

    if (navGrid) {
        auto agentView = registry.view<NavAgentComponent, NavAgentState, Transform>();
        for (auto entity : agentView) {
            const auto& agent = agentView.get<NavAgentComponent>(entity);
            const auto& state = agentView.get<NavAgentState>(entity);
            if (!agent.drawPath || state.path.size() < 2) continue;

            float prevX = 0.0f;
            float prevZ = 0.0f;
            NavigationSystem::GridToWorld(*navGrid, {state.path[0].x, state.path[0].y}, prevX, prevZ);

            for (size_t i = 1; i < state.path.size(); ++i) {
                float nextX = 0.0f;
                float nextZ = 0.0f;
                NavigationSystem::GridToWorld(*navGrid, {state.path[i].x, state.path[i].y}, nextX, nextZ);
                vertices.push_back(prevX); vertices.push_back(navGrid->y + 0.02f); vertices.push_back(prevZ);
                vertices.push_back(nextX); vertices.push_back(navGrid->y + 0.02f); vertices.push_back(nextZ);
                colors.push_back(1.0f); colors.push_back(0.9f); colors.push_back(0.2f);
                colors.push_back(1.0f); colors.push_back(0.9f); colors.push_back(0.2f);
                prevX = nextX;
                prevZ = nextZ;
            }
        }
    }

    if (!vertices.empty()) {
        renderer->DrawLines(vertices, colors);
    }
}

} // namespace Genesis::Engine
