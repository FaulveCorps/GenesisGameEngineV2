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
        return true;
#else
        std::cerr << "BulletPhysics: Bullet not available at compile time" << std::endl;
        return false;
#endif
    }

    void Shutdown() override {
#ifdef HAVE_BULLET
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

private:
#ifdef HAVE_BULLET
    btDefaultCollisionConfiguration* m_collisionConfig = nullptr;
    btCollisionDispatcher* m_dispatcher = nullptr;
    btBroadphaseInterface* m_broadphase = nullptr;
    btSequentialImpulseConstraintSolver* m_solver = nullptr;
    btDiscreteDynamicsWorld* m_world = nullptr;
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
