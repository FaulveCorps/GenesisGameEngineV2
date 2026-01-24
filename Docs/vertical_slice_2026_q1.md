# Genesis — Q1 2026 Vertical Slice Definition

## Goal
Create a small, shippable gameplay slice that validates the engine’s core systems end‑to‑end and serves as a regression gate for future work.

## Scenario (playable loop)
A compact third‑person scene where the player:
1. Spawns in a small courtyard.
2. Collects 3 artifacts.
3. Unlocks a door and exits the level.

## Required systems (minimum viable)
- Rendering: PBR materials, shadow map, post‑process (bloom + tone mapping).
- Animation: idle/walk/run and pickup animation.
- Physics: rigidbody + collider for player and pickups.
- Input: keyboard + gamepad.
- UI: HUD showing artifacts collected.
- Audio: ambient + pickup SFX.
- Scene: save/load using `.scene` assets.

## Editor requirements
- Place/transform entities, assign models/materials.
- Configure lights and camera.
- Set up colliders and rigidbodies.
- Assign simple scripts to pickups and door.

## Acceptance criteria
- Runs at 60 FPS on a mid‑range PC (debug or release build is fine for now).
- No crashes over a 10‑minute play session.
- Artifacts collected persist correctly if the scene is reloaded.
- Editor can load the scene, edit it, and save without corrupting data.

## Tests to gate the slice
- Smoke: load the vertical slice scene and run 30 seconds of automated play.
- Assets: verify all referenced textures/models exist and load.
- Rendering: verify shader compilation for PBR, post‑process, and shadow passes.

## Deliverables
- `Assets/scenes/vertical_slice.scene`
- `Assets/prefabs/player.prefab`
- `Assets/prefabs/pickup.prefab`
- `Assets/prefabs/door.prefab`
- Automated test(s) in `Tests/` covering the slice.
