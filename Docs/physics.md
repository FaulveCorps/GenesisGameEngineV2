# Physics Subsystem (Bullet)

Overview
- The engine exposes a Physics subsystem via `IPhysics` (a subclass of `ISubsystem`).
- Two backends are provided: `null` (no-op) and `bullet` (real simulation using Bullet Physics library).

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
- The `NullPhysics` backend implements these as no-ops, and the `Bullet` backend implements them using Bullet types when available.

SampleGame demo
- You can now run `SampleGame` with `--physics-demo` to spawn a simple box and observe periodic position prints to the console (falls under gravity when using Bullet).

