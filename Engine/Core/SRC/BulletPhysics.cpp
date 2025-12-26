#include "engine/IPhysics.h"
#include "engine/SubsystemRegistry.h"

#include <iostream>

#ifdef HAVE_BULLET
#include <btBulletDynamicsCommon.h>
#endif

namespace Genesis::Engine {

class BulletPhysics : public IPhysics {
public:
    BulletPhysics() {}
    bool Init() override {
#ifdef HAVE_BULLET
        m_collisionConfig = new btDefaultCollisionConfiguration();
        m_dispatcher = new btCollisionDispatcher(m_collisionConfig);
        m_broadphase = new btDbvtBroadphase();
        m_solver = new btSequentialImpulseConstraintSolver();
        m_world = new btDiscreteDynamicsWorld(m_dispatcher, m_broadphase, m_solver, m_collisionConfig);
        if (!m_world) {
            std::cerr << "BulletPhysics: failed to create dynamics world" << std::endl;
            return false;
        }
        // default gravity
        m_world->setGravity(btVector3(0, -9.81f, 0));
        return true;
#else
        std::cerr << "BulletPhysics: Bullet not available at compile time" << std::endl;
        return false;
#endif
    }

    void Shutdown() override {
#ifdef HAVE_BULLET
        // remove & delete all bodies we created
        for (auto& kv : m_bodies) {
            btRigidBody* body = kv.second;
            if (m_world && body) m_world->removeRigidBody(body);
            delete body;
        }
        for (auto& kv : m_shapes) delete kv.second;
        for (auto& kv : m_motionStates) delete kv.second;
        m_bodies.clear(); m_shapes.clear(); m_motionStates.clear();

        delete m_world; m_world = nullptr;
        delete m_solver; m_solver = nullptr;
        delete m_broadphase; m_broadphase = nullptr;
        delete m_dispatcher; m_dispatcher = nullptr;
        delete m_collisionConfig; m_collisionConfig = nullptr;
#endif
    }

    void Update(double dt) override {
#ifdef HAVE_BULLET
        if (m_world) m_world->stepSimulation(static_cast<float>(dt), 1);
#endif
    }

    std::string Name() const override { return "bullet"; }

    void StepSimulation(float dt, int maxSubSteps = 1) override {
#ifdef HAVE_BULLET
        if (m_world) m_world->stepSimulation(dt, maxSubSteps);
#endif
    }

    BodyHandle CreateBoxRigidBody(float mass, float posX, float posY, float posZ, float sizeX, float sizeY, float sizeZ) override {
#ifdef HAVE_BULLET
        if (!m_world) return 0;
        btCollisionShape* shape = new btBoxShape(btVector3(sizeX * 0.5f, sizeY * 0.5f, sizeZ * 0.5f));
        btTransform transform; transform.setIdentity(); transform.setOrigin(btVector3(posX, posY, posZ));

        btVector3 localInertia(0,0,0);
        if (mass != 0.0f) shape->calculateLocalInertia(mass, localInertia);

        btDefaultMotionState* motion = new btDefaultMotionState(transform);
        btRigidBody::btRigidBodyConstructionInfo info(mass, motion, shape, localInertia);
        btRigidBody* body = new btRigidBody(info);
        m_world->addRigidBody(body);

        BodyHandle h = m_nextHandle++;
        m_bodies[h] = body;
        m_shapes[h] = shape;
        m_motionStates[h] = motion;
        return h;
#else
        (void)mass; (void)posX; (void)posY; (void)posZ; (void)sizeX; (void)sizeY; (void)sizeZ;
        return 0;
#endif
    }

    void DestroyRigidBody(BodyHandle h) override {
#ifdef HAVE_BULLET
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return;
        btRigidBody* body = it->second;
        if (m_world && body) m_world->removeRigidBody(body);
        delete body;
        m_bodies.erase(it);
        auto mit = m_motionStates.find(h);
        if (mit != m_motionStates.end()) { delete mit->second; m_motionStates.erase(mit); }
        auto sit = m_shapes.find(h);
        if (sit != m_shapes.end()) { delete sit->second; m_shapes.erase(sit); }
#endif
    }

    bool GetRigidBodyPosition(BodyHandle h, float& x, float& y, float& z) override {
#ifdef HAVE_BULLET
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return false;
        btRigidBody* body = it->second;
        if (!body) return false;
        btTransform t;
        if (body->getMotionState()) body->getMotionState()->getWorldTransform(t);
        else t = body->getWorldTransform();
        btVector3 o = t.getOrigin();
        x = o.getX(); y = o.getY(); z = o.getZ();
        return true;
#else
        (void)h; (void)x; (void)y; (void)z;
        return false;
#endif
    }

    void ApplyCentralImpulse(BodyHandle h, float ix, float iy, float iz) override {
#ifdef HAVE_BULLET
        auto it = m_bodies.find(h);
        if (it == m_bodies.end()) return;
        btRigidBody* body = it->second;
        if (!body) return;
        body->applyCentralImpulse(btVector3(ix, iy, iz));
#endif
    }

private:
#ifdef HAVE_BULLET
    btDefaultCollisionConfiguration* m_collisionConfig = nullptr;
    btCollisionDispatcher* m_dispatcher = nullptr;
    btBroadphaseInterface* m_broadphase = nullptr;
    btSequentialImpulseConstraintSolver* m_solver = nullptr;
    btDiscreteDynamicsWorld* m_world = nullptr;

    std::unordered_map<BodyHandle, btRigidBody*> m_bodies;
    std::unordered_map<BodyHandle, btCollisionShape*> m_shapes;
    std::unordered_map<BodyHandle, btDefaultMotionState*> m_motionStates;
    BodyHandle m_nextHandle = 1;
#endif
};

static bool register_bullet_physics = []() {
#ifdef HAVE_BULLET
    SubsystemRegistry::Instance().RegisterFactory("Physics", "bullet", []() {
        return std::make_unique<BulletPhysics>();
    });
#endif
    return true;
}();

void RegisterBulletFactory() {
#ifdef HAVE_BULLET
    SubsystemRegistry::Instance().RegisterFactory("Physics", "bullet", []() {
        return std::make_unique<BulletPhysics>();
    });
#endif
}

} // namespace Genesis::Engine
