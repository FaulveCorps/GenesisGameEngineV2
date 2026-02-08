# Genesis Game Engine - Status Report & Competitive Analysis (2026-01-03)

## Executive Summary
Following the stabilization phase (Jan 2), the project has shifted focus to **Modernization**. The primary goal was to elevate the rendering capabilities to a modern standard (PBR) and establish a secure development lifecycle. 

**Current Status:** The engine now possesses a functional PBR (Physically Based Rendering) pipeline capable of loading and rendering glTF 2.0 assets. This marks a critical milestone in moving from a "toy" renderer to a commercially viable graphics foundation.

## Recent Progress (Since Jan 2)

### 1. Rendering Modernization (PBR)
- **Shader Pipeline**: Implemented a PBR shader supporting Albedo, Normal, and Metallic-Roughness workflows.
- **Asset Pipeline**: 
    - Integrated `glTF 2.0` loading support via Assimp and custom material extraction logic.
    - Refactored `Mesh`, `Model`, and `Material` classes to handle texture resources and UV mapping correctly.
    - Created `Scripts/generate_gltf.py` to generate valid test assets programmatically.
- **Verification**: Validated the pipeline by rendering a generated glTF PBR model in `SampleGame`.

### 2. Security & CI
- **Snyk Integration**: Implemented Snyk SAST (Static Application Security Testing) in GitHub Actions.
- **Policy**: Established a "Security at Inception" policy, requiring scans for new code.

### 3. Stability & Infrastructure
- **Runtime Fixes**: Resolved missing DLL dependencies for `SampleGame` execution.
- **Git Hygiene**: Cleaned up repository history (removed large `.dmp` files) and updated `.gitignore`.

### 4. Verification (2026-01-26)
- **Post-Processing Confirmed (OpenGL)**: Bloom, tone mapping, and gamma correction are implemented in the OpenGL post-process pipeline.
- **Shadow Mapping Confirmed (OpenGL)**: Directional shadow map is implemented (single-map), with room for CSM/PCF/PCSS upgrades.
- **Backend Parity**: Vulkan/DirectX post-processing and shadows still require validation and parity work.

---

## Competitive Analysis: Genesis vs. Commercial Engines

### 1. Rendering Capabilities
| Feature | Commercial (Unreal 5 / Unity HDRP) | Genesis Engine (Current) | Gap Analysis |
| :--- | :--- | :--- | :--- |
| **Lighting** | Global Illumination (Lumen), Raytracing | PBR (Direct Lighting) + directional shadow map (OpenGL) | **Critical**. Lacks GI; shadows are single-map and need CSM/PCF. |
| **Geometry** | Virtualized (Nanite), LODs | Standard Mesh (VBO/IBO) | **High**. No LOD system or streaming. |
| **Materials** | Node-based Shader Graphs | Code-based PBR Shaders | **High**. Harder for artists to use. |
| **Post-Process** | Full Stack (Bloom, LUT, DoF) | Bloom + tone mapping + gamma (OpenGL) | **Medium**. Needs LUTs/DoF/SSR/SSAO and validation across backends. |

**Verdict**: Genesis has laid the *mathematical foundation* (PBR) but lacks the *ecosystem* (GI, Post-FX, Tools) that makes commercial engines look "next-gen".

### 2. Architecture & Performance
| Feature | Commercial | Genesis Engine | Gap Analysis |
| :--- | :--- | :--- | :--- |
| **Core Pattern** | GameObject/MonoBehaviour (Unity), Actor (Unreal) | ECS (`entt`) + Subsystems | **Advantage Genesis**. ECS is more cache-friendly and modern than legacy OOP patterns. |
| **Modularity** | Monolithic (Hard to strip) | Modular Subsystems | **Advantage Genesis**. Lightweight and flexible. |
| **Scripting** | C#, C++, Blueprints | Lua, WASM (Sandboxed) | **Unique**. WASM offers secure, sandboxed modding that commercial engines struggle to provide safely. |

**Verdict**: Genesis has a superior *core architecture* for performance-critical and moddable games, avoiding the bloat of commercial engines.

### 3. Tooling & Workflow
| Feature | Commercial | Genesis Engine | Gap Analysis |
| :--- | :--- | :--- | :--- |
| **Editor** | WYSIWYG, Asset Browser, Profilers | ImGui editor (docking, hierarchy, inspector, content browser, gizmos) | **High**. Lacks material/animation/VFX tooling and profilers. |
| **Physics** | PhysX/Havok (Robust) | Bullet/Box2D (Basic) | **Medium**. Sufficient for indie games. |
| **Audio** | Wwise/FMOD (Middleware) | miniaudio (Basic) | **Medium**. Lacks spatialization/mixing tools. |

---

## Roadmap to Competitiveness

To compete with commercial engines *without* building a massive editor, Genesis must double down on its strengths: **Performance, Moddability, and Programmer-First Workflow**.

### Immediate Priorities (Q1 2026)
1.  **Visual Polish**: Verify and harden post-processing across backends (Bloom + tone mapping + gamma are in OpenGL).
2.  **Shadows**: Upgrade to CSM + PCF/PCSS (current OpenGL shadow map is single cascade).
3.  **Animation**: Implement Skeletal Animation support (glTF skinning).

### Strategic Differentiators
- **"The Moddable Engine"**: Lean into WASM. Build tooling that makes it trivial for users to create secure mods.
- **"The Programmer's Engine"**: Focus on hot-reloading, excellent C++ APIs, and zero-boilerplate setup.

## Conclusion
Genesis is no longer just a "toy". It is a **functional, modern rendering framework**. While it cannot compete with Unreal's visual fidelity or Unity's tooling breadth yet, it offers a cleaner, faster, and more secure foundation for specific types of games (e.g., simulation, strategy, mod-heavy titles).

---

## Addendum: Verified & Added Features (2026-01-27)

### Rendering (OpenGL verified)
- Post-processing stack expanded and verified: Bloom, Tone Mapping, Gamma, LUT, Vignette.
- Directional shadow mapping with PCF softness control (single cascade).
- Vulkan/DirectX still pending parity verification.

### Navigation & AI Tooling
- Grid-based navigation with runtime NavGrid bake from colliders.
- NavAgent component for path following; debug grid + path visualization.
- Sphere and box colliders contribute to blocked cells.

### UI Framework (runtime + editor)
- Anchors/pivot layout, background styling, and text alignment.
- Text scaling, wrapping, padding, and border styling.
- Scene/prefab serialization for UI settings.

### Audio & Asset Workflow
- Spatial audio parameters and listener updates.
- Editor audio mixer and asset preview.
- Asset import settings UI and deterministic reimport ordering.
