#include "engine/DebugRenderer.h"
#include "engine/Components.h"
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
        auto& t = boxView.get<Transform>(entity);

        Matrix4 transMat = Matrix4::CreateTranslation(t.x, t.y, t.z);
        Matrix4 rotX = Matrix4::CreateRotationX(t.rx);
        Matrix4 rotY = Matrix4::CreateRotationY(t.ry);
        Matrix4 rotZ = Matrix4::CreateRotationZ(t.rz);
        Matrix4 scaleMat = Matrix4::CreateScale(t.sx, t.sy, t.sz);
        // T * R * S
        Matrix4 worldParams = transMat * rotZ * rotY * rotX * scaleMat;

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
        auto& t = sphereView.get<Transform>(entity);

        Matrix4 transMat = Matrix4::CreateTranslation(t.x, t.y, t.z);
        Matrix4 rotX = Matrix4::CreateRotationX(t.rx);
        Matrix4 rotY = Matrix4::CreateRotationY(t.ry);
        Matrix4 rotZ = Matrix4::CreateRotationZ(t.rz);
        Matrix4 scaleMat = Matrix4::CreateScale(t.sx, t.sy, t.sz);
        Matrix4 worldParams = transMat * rotZ * rotY * rotX * scaleMat;

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

    if (!vertices.empty()) {
        renderer->DrawLines(vertices, colors);
    }
}

} // namespace Genesis::Engine
