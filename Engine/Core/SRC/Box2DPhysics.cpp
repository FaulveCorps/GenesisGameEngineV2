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
    Box2DPhysics(): m_world(nullptr), m_nextHandle(1), m_nextJointHandle(1), m_contactListener(this) {}

    bool Init() override {
        // gravity downwards (y)
        m_world = new b2World(b2Vec2(0.0f, -9.81f));
        m_world->SetContactListener(&m_contactListener);
        std::cout << "Box2DPhysics: Init" << std::endl;
        return true;
    }

    void Update(double /*dt*/) override { /* no-op */ }

    void Shutdown() override {
        if (m_world) {
            // destroy joints
            for (auto &p : m_joints) {
                if (p.second) m_world->DestroyJoint(p.second);
            }
            m_joints.clear();
            // destroy bodies
            for (auto &p : m_bodies) {
                if (p.second) m_world->DestroyBody(p.second);
            }
            m_bodies.clear();
            m_bodyHandles.clear();
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
        m_bodyHandles[body] = h;
        return h;
    }

    void DestroyRigidBody(BodyHandle h) override {
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return;
        if (it->second && m_world) {
            m_bodyHandles.erase(it->second);
            m_world->DestroyBody(it->second);
        }
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

    // Joints (2D)
    JointHandle CreateDistanceJoint(BodyHandle a, BodyHandle b, float anchorAx, float anchorAy, float anchorBx, float anchorBy) override {
        auto ita = m_bodies.find(a);
        auto itb = m_bodies.find(b);
        if (ita == m_bodies.end() || itb == m_bodies.end()) return 0;
        b2DistanceJointDef jd;
        jd.Initialize(ita->second, itb->second, b2Vec2(anchorAx, anchorAy), b2Vec2(anchorBx, anchorBy));
        b2Joint* j = m_world->CreateJoint(&jd);
        JointHandle jh = m_nextJointHandle++;
        m_joints[jh] = j;
        return jh;
    }

    JointHandle CreateRevoluteJoint(BodyHandle a, BodyHandle b, float anchorX, float anchorY) override {
        auto ita = m_bodies.find(a);
        auto itb = m_bodies.find(b);
        if (ita == m_bodies.end() || itb == m_bodies.end()) return 0;
        b2RevoluteJointDef rd;
        rd.Initialize(ita->second, itb->second, b2Vec2(anchorX, anchorY));
        b2Joint* j = m_world->CreateJoint(&rd);
        JointHandle jh = m_nextJointHandle++;
        m_joints[jh] = j;
        return jh;
    }

    void DestroyJoint(JointHandle j) override {
        auto it = m_joints.find(j);
        if (it == m_joints.end()) return;
        if (it->second && m_world) m_world->DestroyJoint(it->second);
        m_joints.erase(it);
    }

    void SetContactCallbacks(ContactCallback onBegin, ContactCallback onEnd) override {
        m_onBegin = onBegin;
        m_onEnd = onEnd;
    }

private:
    // Contact listener forwards begin/end to registered callbacks
    struct ContactListener : public b2ContactListener {
        ContactListener(Box2DPhysics* p) : parent(p) {}
        void BeginContact(b2Contact* contact) override {
            b2Body* a = contact->GetFixtureA()->GetBody();
            b2Body* b = contact->GetFixtureB()->GetBody();
            auto itA = parent->m_bodyHandles.find(a);
            auto itB = parent->m_bodyHandles.find(b);
            if (itA == parent->m_bodyHandles.end() || itB == parent->m_bodyHandles.end()) return;
            if (parent->m_onBegin) parent->m_onBegin(itA->second, itB->second);
        }
        void EndContact(b2Contact* contact) override {
            b2Body* a = contact->GetFixtureA()->GetBody();
            b2Body* b = contact->GetFixtureB()->GetBody();
            auto itA = parent->m_bodyHandles.find(a);
            auto itB = parent->m_bodyHandles.find(b);
            if (itA == parent->m_bodyHandles.end() || itB == parent->m_bodyHandles.end()) return;
            if (parent->m_onEnd) parent->m_onEnd(itA->second, itB->second);
        }
        Box2DPhysics* parent;
    };

    b2World* m_world;
    std::unordered_map<BodyHandle, b2Body*> m_bodies;
    std::unordered_map<b2Body*, BodyHandle> m_bodyHandles;
    std::unordered_map<JointHandle, b2Joint*> m_joints;
    ContactListener m_contactListener;
    std::function<void(BodyHandle,BodyHandle)> m_onBegin;
    std::function<void(BodyHandle,BodyHandle)> m_onEnd;
    BodyHandle m_nextHandle;
    JointHandle m_nextJointHandle;
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
