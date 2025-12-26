#include "engine/IPhysics.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>

#ifndef HAVE_BOX2D
namespace Genesis::Engine { void RegisterBox2DFactory() {} }
#else

#include <box2d/box2d.h>
#include <unordered_map>

namespace Genesis::Engine {

class Box2DPhysics : public IPhysics {
public:
    Box2DPhysics(): m_world(nullptr), m_nextHandle(1) {}

    bool Init() override {
        // gravity downwards (y)
        m_world = new b2World(b2Vec2(0.0f, -9.81f));
        std::cout << "Box2DPhysics: Init" << std::endl;
        return true;
    }

    void Update(double /*dt*/) override { /* no-op */ }

    void Shutdown() override {
        if (m_world) {
            // destroy bodies if any
            for (auto &p : m_bodies) {
                if (p.second) m_world->DestroyBody(p.second);
            }
            m_bodies.clear();
            delete m_world;
            m_world = nullptr;
        }
        std::cout << "Box2DPhysics: Shutdown" << std::endl;
    }

    std::string Name() const override { return "box2d"; }

    void StepSimulation(float dt, int /*maxSubSteps*/ = 1) override {
        if (!m_world) return;
        // Simple single-step; Box2D prefers small fixed steps
        m_world->Step(dt, 8, 3);
    }

    BodyHandle CreateBoxRigidBody(float mass, float posX, float posY, float /*posZ*/, float sizeX, float sizeY, float /*sizeZ*/) override {
        if (!m_world) return 0;
        b2BodyDef bd;
        if (mass > 0.0f) bd.type = b2_dynamicBody; else bd.type = b2_staticBody;
        bd.position.Set(posX, posY);
        b2Body* body = m_world->CreateBody(&bd);
        b2PolygonShape box;
        box.SetAsBox(sizeX * 0.5f, sizeY * 0.5f);
        b2FixtureDef fd;
        fd.shape = &box;
        if (mass > 0.0f) {
            float area = sizeX * sizeY;
            fd.density = (area > 0.0f) ? (mass / area) : 1.0f;
        } else {
            fd.density = 0.0f;
        }
        fd.friction = 0.3f;
        body->CreateFixture(&fd);

        BodyHandle h = m_nextHandle++;
        m_bodies[h] = body;
        return h;
    }

    void DestroyRigidBody(BodyHandle h) override {
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return;
        if (it->second && m_world) m_world->DestroyBody(it->second);
        m_bodies.erase(it);
    }

    bool GetRigidBodyPosition(BodyHandle h, float& x, float& y, float& z) override {
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return false;
        if (!it->second) return false;
        b2Vec2 pos = it->second->GetPosition();
        x = pos.x; y = pos.y; z = 0.0f;
        return true;
    }

    void ApplyCentralImpulse(BodyHandle h, float ix, float iy, float /*iz*/) override {
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return;
        if (!it->second) return;
        it->second->ApplyLinearImpulseToCenter(b2Vec2(ix, iy), true);
    }

private:
    b2World* m_world;
    std::unordered_map<BodyHandle, b2Body*> m_bodies;
    BodyHandle m_nextHandle;
};

static bool register_box2d = []() {
    SubsystemRegistry::Instance().RegisterFactory("Physics", "box2d", []() {
        return std::make_unique<Box2DPhysics>();
    });
    return true;
}();

void RegisterBox2DFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Physics", "box2d", []() {
        return std::make_unique<Box2DPhysics>();
    });
}

} // namespace Genesis::Engine
#endif
