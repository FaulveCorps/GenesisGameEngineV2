# Mods & Modding Support

Overview
- The engine provides a basic ModManager that scans a `mods/` directory for mod folders.
- Each mod may optionally include a `mod.json` manifest with fields: `id`, `name`, `version`, and `description`.
- The ModManager exposes a simple API for listing discovered mods (no runtime code execution is provided yet).

mod.json example:
{
  "id": "my_awesome_mod",
  "name": "My Awesome Mod",
  "version": "0.1",
  "description": "Adds some cool content"
}

How to use
- Place each mod in a separate folder under `mods/` in the game working directory.
- The engine's `ModManager` can be used to scan and enumerate available mods.
- If Lua scripting is available (compiled with `HAVE_LUA`), the engine will attempt to execute a `mod.lua` file inside each mod folder at startup.
- When Lua is available, mod scripts may register simple hooks into engine features; for example, `engine.register_contact_begin(fn)` and `engine.register_contact_end(fn)` can be used to receive physics contact notifications when a physics backend that supports contacts (e.g., `box2d`) is active.

- Additional helper functions are available in the `engine` table for simple physics manipulation when Lua is present (examples):
  - `engine.create_physics_backend("box2d")` — attempt to create a physics backend by name
  - `engine.create_body(mass, x, y, sizeX, sizeY)` — create a box rigid body and return a handle (or nil)
  - `engine.destroy_body(handle)` — destroy a body by handle
  - `engine.create_distance_joint(a, b, ax, ay, bx, by)` — create a distance joint between two bodies
  - `engine.apply_impulse(handle, ix, iy)` — apply a 2D impulse to a body (z ignored)

Example `mod.lua` snippet:
```lua
-- create a Box2D backend (if available) and spawn two boxes connected by a joint
engine.create_physics_backend("box2d")
local a = engine.create_body(1.0, 0.0, 5.0, 1.0, 1.0)
local b = engine.create_body(1.0, 1.0, 5.0, 1.0, 1.0)
local j = engine.create_distance_joint(a, b, 0.0, 5.0, 1.0, 5.0)

-- react to contact events
engine.register_contact_begin(function(x, y) print("contact:", x, y) end)
```

Future work
- Scripted mods (Lua/JS), dynamic content loading, dependency resolution, and mod enabling/disabling via UI are planned.
- We can add an in-engine `ModSubsystem` and script bindings for more advanced mod support.
