# Physics Subsystem (Bullet)

Overview
- The engine exposes a Physics subsystem via `IPhysics` (a subclass of `ISubsystem`).
- Backends:
  - `null` (no-op)
  - `bullet` (real simulation using Bullet Physics library)
  - `box2d` (2D physics using Box2D; optional, enabled when `HAVE_BOX2D` is detected)

How to use
- At engine init (`Genesis::Engine::Init()`), built-in factories are registered (including `null` and, if available, `bullet`).
- Create a global physics subsystem with:

    Genesis::Engine::CreatePhysicsSubsystem("bullet"); // or "null"
    auto physics = Genesis::Engine::GetPhysicsSubsystem();

- Step the simulation (if you manage stepping manually):

    physics->StepSimulation(0.016f);

Notes
- Bullet is detected via CMake by searching for `btBulletDynamicsCommon.h` and common libraries (`BulletDynamics`, `BulletCollision`, `LinearMath`) in the configured vcpkg install dir.
- When Bullet is not found, the Bullet backend will be disabled at build time and the `null` backend will be used by default.

Testing
- Unit tests check `null` backend availability and, when `HAVE_BULLET` is defined, attempt to initialize the Bullet backend and perform a short step.

Rigid body API
- The `IPhysics` interface now exposes a minimal rigid-body API:
  - `BodyHandle CreateBoxRigidBody(float mass, float posX, float posY, float posZ, float sizeX, float sizeY, float sizeZ);`
  - `void DestroyRigidBody(BodyHandle h);`
  - `bool GetRigidBodyPosition(BodyHandle h, float& x, float& y, float& z);`
  - `void ApplyCentralImpulse(BodyHandle h, float ix, float iy, float iz);`
  - **Joint API (optional):**
    - `JointHandle CreateDistanceJoint(BodyHandle a, BodyHandle b, float anchorAx, float anchorAy, float anchorBx, float anchorBy);`
    - `JointHandle CreateRevoluteJoint(BodyHandle a, BodyHandle b, float anchorX, float anchorY);`
    - `void DestroyJoint(JointHandle j);`
  - **Contact callbacks:**
    - `void SetContactCallbacks(ContactCallback onBegin, ContactCallback onEnd);`
- The `NullPhysics` backend implements these as no-ops. `Box2D` implements joints and contact callbacks; `Bullet` stores callbacks and currently stubs joint creation (future work: map to Bullet constraints).

SampleGame demo
- You can now run `SampleGame` with `--physics-demo` to spawn a simple box and observe periodic position prints to the console (falls under gravity when using Bullet).

