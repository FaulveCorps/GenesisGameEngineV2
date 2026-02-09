# Genesis Game Engine — Context Summary & Long‑Term Plan (2026‑01‑24)

## Current snapshot

### Engine strengths
- Multi‑backend renderer (OpenGL, Vulkan, DirectX, Software) with PBR and runtime renderer switching.
- ECS (`entt`), Lua + WASM scripting, Bullet/Box2D physics, ENet networking, and plugin API.
- Asset loading via Assimp/stb; shader hot‑reload.

### Editor reality
- Functional editor exists with docking UI, hierarchy, inspector, content browser, gizmos, play/pause/step.
- Autosave, undo/redo, prefab save/update, asset reimport, command palette, viewport tooling.

### Known doc mismatch to verify
- `Docs/status_report_20260103.md` says no post‑processing/shadows; `Docs/architecture.md` says bloom/tone‑mapping and shadow mapping exist. Confirm actual runtime behavior and update docs.

## Missing vs a modern engine

### Rendering & visuals
- Global illumination, ray tracing, HDR/tonemap verification, TAA/TSR, motion blur, volumetrics, SSAO/SSR.
- Cascaded/contact shadows, PCF/PCSS soft shadow filtering.
- LODs, streaming, GPU culling/occlusion, large‑scene support.
- Light baking (GI) + light probes + reflection probes.
- Decals, terrain splats, and screen‑space effects (outline, depth‑fog, color‑grading).

### Animation & gameplay
- Skeletal animation pipeline (import, skinning, blend trees, retargeting, IK).
- Navigation mesh + pathfinding + AI tooling.
- In‑game UI framework (layout, text, input, styling).
- Animation state machine editor, pose library, and additive animation support.
- Ragdoll/cloth simulation + character controller tooling.
- Input action map editor (rebindable inputs) + gameplay ability system scaffolding.

### Systems & performance
- Job system / task graph, render graph, frame allocator.
- Robust profiling (CPU/GPU), memory budgets, crash reporting.
- Deterministic scene serialization + diff/merge.
- Entity/component reflection, property metadata, and runtime inspection.
- Deterministic replay/record, save/load system, and rollback‑friendly networking.
- Asset streaming with budgets, background IO, and shader pipeline cache.

### Editor & tooling
- Material/shader graph editor.
- Animation editor (clip preview, blend graphs/state machines).
- Particle/VFX editor, audio mixer, terrain, navmesh bake tools.
- Import settings UI, asset thumbnails for all types.
- Build/package/export pipeline and project settings UI.
- Sequencer/timeline (cutscenes, camera tracks, keyframes).
- Visual scripting (node graph) and gameplay debug overlay.
- UI layout editor (anchors, flex/grid), prefab variants, and multi‑scene editing.
- Source control integration, asset tagging/labels, and advanced search.
- Localization editor + data table/CSV editor.

### Pipeline & ecosystem
- Platform export & packaging workflow, CI artifact publishing.
- Modding workflow around WASM (tooling + distribution).
- Build profiles (dev/qa/release), symbol server integration, and crash dump triage.

## Long‑term plan (followable)

### Phase 0 — Alignment & audit (2–4 weeks)
- Verify renderer features vs docs; update `Docs/status_report_20260103.md`.
- Define a vertical‑slice sample game to gate progress.
- Add benchmark + smoke tests (startup, load scene, play mode).

### Phase 1 — Core foundation (0–3 months)
- Asset database v2: dependency graph, import settings, deterministic reimport.
- Job system + frame allocator + profiler hooks.
- Stable scene serialization + diff/merge support.

### Phase 2 — Visual baseline (3–6 months)
- Post‑processing stack (HDR, bloom, LUTs, vignette, tone mapping).
- Shadow upgrades (CSM + PCF/PCSS).
- Skeletal animation import + GPU skinning.

### Phase 3 — Workflow tooling (6–12 months)
- Material graph editor + shader variant system.
- Animation editor + state machine/blend tree UI.
- VFX/particle editor + audio mixer panel.
- Build/export pipeline + project settings UI.
- Sequencer/timeline + UI layout editor + localization tooling.

### Phase 4 — Scale & differentiation (12–24 months)
- Streaming + LOD + occlusion culling + large‑world support.
- Optional ray‑tracing/GI path.
- Modding focus: WASM packaging, permissions, hot‑reload UX.
- Source control integration + multiplayer replication/rollback tooling.

## Checkpoints (definition of done)
- Each milestone ships with a sample scene, an automated test, and a profiler capture.
- Each editor tool has round‑trip import/edit/export validation.
