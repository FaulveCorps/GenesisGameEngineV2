# Genesis Game Engine — Long-Term Context (Execution Source of Truth)

**Updated:** 2026-02-16

---

## How this file is used

- This file is the persistent execution backlog for "continue"-style work sessions.
- On each "continue", execute the next highest-priority unchecked item that is not blocked.
- Keep scope incremental: complete, verify, then move to the next task.
- If blocked, log blocker + dependency, then switch to the next unblocked item.

### Completion output rule

When every required item in this file is implemented and validated, respond exactly with:

**the full task is complete**

---

## Current verified baseline (as of this update)

- OpenGL path is the most feature-complete runtime renderer (deferred + PBR + shadows + bloom/tone-map/gamma).
- Vulkan/D3D11/D3D12/WGPU exist but are not at full feature parity with OpenGL.
- Editor is functional (docking, hierarchy, inspector, content browser, gizmos, undo/redo, play/pause/step, autosave, prefab flows).
- Core architecture exists: ECS (`entt`), modular subsystems, Lua + WASM scripting, ENet backend support, plugins.
- Physics exists via Bullet + Box2D but higher-level gameplay tooling is limited.

## Incremental progress notes

- 2026-02-16: Vulkan groundwork advanced so scene state (lighting, point lights, exposure/gamma, bloom, vignette, shadow softness, camera/view influence, UI/mesh activity) now feeds a deterministic per-frame stub clear-color model used by the Vulkan no-pipeline path.
- 2026-02-16: Added Vulkan validation coverage via `test_vulkan_scene_api_smoke.cpp` and deterministic unit coverage via `test_vulkan_stub_clearcolor.cpp`.
- 2026-02-16: Vulkan stub ingestion now incorporates queued mesh/material parameters (base color, metallic, roughness, transform influence) and UI texture overlays (ARGB color, area/UV weighting), improving parity scaffolding for base pass + UI behavior.
- 2026-02-16: Expanded `test_vulkan_stub_clearcolor.cpp` assertions to verify mesh material and UI tint paths drive deterministic per-channel output changes.
- 2026-02-16: Vulkan mesh ingestion now factors mesh topology/attributes (vertex/index density, normals, UV presence) into deterministic shading output, tightening base-pass parity scaffolding with OpenGL-side expectations.
- 2026-02-16: Added deterministic unit coverage to verify topology/attribute-rich meshes influence Vulkan stub output more than minimal geometry.
- 2026-02-16: Vulkan stub now applies UI overlay as an order-sensitive alpha-over pass after post-processing/gamma (matching OpenGL pass ordering), with deterministic tests covering overlay dominance and draw-order behavior.

---

## Master feature backlog (must add)

## P0 — Critical foundations (do first)

### Renderer parity and correctness
- [ ] Bring Vulkan to parity with OpenGL baseline: PBR base pass, directional shadows, post stack, UI overlay.
- [ ] Bring D3D11 to parity with OpenGL baseline: PBR base pass, directional shadows, post stack, UI overlay.
- [ ] Bring D3D12 to parity with OpenGL baseline: PBR base pass, directional shadows, post stack, UI overlay.
- [ ] Add automated parity sweep + screenshot diffing across all backends.

### Animation production baseline
- [ ] Skeletal import path (bones, weights, animation clips from glTF).
- [ ] GPU skinning path.
- [ ] Animation playback controller with blending support.
- [ ] Animation state machine runtime and editor panel (minimum viable).

### Runtime performance infrastructure
- [ ] Job system / task graph.
- [ ] Frame allocator and transient memory pools.
- [ ] CPU profiler timeline + GPU timings (at least per-pass + per-system).
- [ ] Performance budget reporting (frame time, memory, draw calls).

### Save/load and determinism baseline
- [ ] Deterministic scene serialization format with stable IDs.
- [ ] Scene diff/merge-safe serialization strategy.
- [ ] Save/load round-trip validation tests.

---

## P1 — High-impact engine capabilities

### Rendering quality upgrades
- [ ] Cascaded shadow maps (CSM).
- [ ] Soft shadow filtering (PCF/PCSS quality tiers).
- [ ] SSAO.
- [ ] SSR.
- [ ] Temporal AA (TAA/TSR-style baseline implementation).
- [ ] Color grading (LUT workflow fully validated across supported backends).

### World scale and streaming
- [ ] LOD pipeline (asset + runtime selection).
- [ ] Occlusion culling (software/GPU-assisted approach).
- [ ] Asset streaming (background I/O + residency budgets).
- [ ] Shader pipeline cache / PSO cache strategy.

### Networking gameplay layer
- [ ] Replication model for entity/component state.
- [ ] Client interpolation + prediction baseline.
- [ ] Rollback-friendly state capture hooks.
- [ ] Multiplayer debug tools (latency, packet, correction visualizer).

---

## P2 — Gameplay systems and editor depth

### AI and navigation
- [ ] Navmesh workflow (or robust hybrid with bake tooling) beyond current grid-only baseline.
- [ ] AI behavior tooling (behavior tree/utility system minimum viable).
- [ ] AI debug visualization overlays.

### Physics and character systems
- [ ] Character controller framework.
- [ ] Ragdoll tooling.
- [ ] Cloth simulation integration path.
- [ ] Vehicle dynamics framework (minimum viable).

### UI and gameplay framework
- [ ] In-game UI framework with layout/styling workflow.
- [ ] Input action maps + rebinding pipeline.
- [ ] Gameplay ability system scaffolding.

### Editor tools expansion
- [ ] Material/shader graph editor production-ready pass.
- [ ] Animation editor (clip preview, blend graph, state graph, retargeting UX).
- [ ] VFX/particle editor.
- [ ] Sequencer/timeline for cutscenes/camera tracks/keyframes.
- [ ] UI layout editor (anchors/flex/grid workflow).
- [ ] Multi-scene editing workflow.

---

## P3 — Pipeline, ecosystem, and team workflows

### Asset and content pipeline
- [ ] Import settings UI parity for major asset classes.
- [ ] Full thumbnail coverage for key asset types.
- [ ] Asset tagging/labels + advanced search/filtering.
- [ ] Dependency graph UX and safe reimport/rebuild tooling.

### Build/export and platform workflows
- [ ] Build/package/export workflow hardened in editor.
- [ ] Platform profiles (dev/qa/release) with reproducible outputs.
- [ ] CI artifact publishing and reproducible install/package jobs.
- [ ] Mobile/web roadmap execution gates (if enabled by product direction).

### Modding and scripting ecosystem
- [ ] WASM mod packaging and distribution workflow.
- [ ] Permission model and capability gating for mods.
- [ ] Mod enable/disable dependency management UX.
- [ ] Robust hot-reload UX for scripts/mods.

---

## P4 — Advanced / differentiation

### Visual frontier
- [ ] GI path (baked and/or real-time strategy).
- [ ] Reflection probes + light probes.
- [ ] Volumetrics.
- [ ] Optional ray-tracing path.
- [ ] Decals + terrain splat/terrain tooling.

### Production hardening
- [ ] Crash reporting pipeline + symbolization workflow.
- [ ] Memory budgets + leak/regression automation.
- [ ] Replay/record system for deterministic debugging and rollback tooling.
- [ ] Source control integration in editor.
- [ ] Localization editor + data-table/CSV tooling.

---

## Execution phases (rolling)

### Phase A — Parity + baseline runtime (now)
- Backend parity, animation baseline, profiling baseline, deterministic save/load.

### Phase B — Quality + scalability
- CSM/PCF/PCSS + SSAO/SSR/TAA, LOD/streaming/culling, network gameplay layer.

### Phase C — Tooling power-up
- Material/animation/VFX/timeline/UI editor tools and content pipeline UX.

### Phase D — Ecosystem + differentiation
- Modding workflow maturity, advanced rendering, production hardening.

---

## Validation requirements (must hold)

- Every completed item has:
	- code merged,
	- automated test or regression coverage,
	- documentation update,
	- verification evidence (build/test output and/or artifact screenshots where relevant).
- Renderer features require cross-backend verification, not OpenGL-only validation.
- Performance-affecting changes require before/after measurements.

---

## Session handoff note for "continue"

- Treat this file as the backlog contract.
- Continue from the highest-priority unchecked item.
- Do not stop after planning only; execute implementation + verification each session.
- Only return **the full task is complete** when every required checkbox above is done and validated.
