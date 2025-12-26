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

Future work
- Scripted mods (Lua/JS), dynamic content loading, dependency resolution, and mod enabling/disabling via UI are planned.
- We can add an in-engine `ModSubsystem` and script bindings for more advanced mod support.
