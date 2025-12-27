# Save Subsystem

The Save subsystem provides a simple pluggable API for persisting game data via named "slots". It follows the same multi-backend pattern used across the engine ("null", "file", etc.).

API (C++):
- `bool Save(const std::string& slot, const std::string& data)` - write raw data to a named slot
- `bool Load(const std::string& slot, std::string& out)` - load raw data for a named slot
- `bool Delete(const std::string& slot)` - remove a slot
- `std::vector<std::string> ListSlots() const` - list available slot names

Backends:
- `null` - no-op implementation (useful for headless or test builds)
- `file` - writes files to a save directory (default: `saves` in the current working directory)
  - You can override the save directory with the environment variable `GENESIS_SAVE_DIR`.
  - Files are written atomically using a temporary file + rename.

Usage example (SampleGame):

- The SampleGame will attempt to create the `file` backend at startup and fall back to `null` if unavailable.
- Hotkeys: `F5` to save to slot `autosave` and `F6` to load it (example data contains a timestamp).

Notes and guidance:
- The File backend sanitizes slot names to `[A-Za-z0-9_-]` and replaces other chars with `_`.
- Saves are stored as files named `<slot>.sav` in the save directory.
- When adding a new backend, register its factory with `SubsystemRegistry::Instance().RegisterFactory("Save", "<name>", factoryFunc);` and provide a `Register<Backend>NameFactory()` helper and call it from `Engine::Init()`.

Testing:
- Unit tests for the Save subsystem are in `Tests/test_save.cpp`. They create temporary directories and set `GENESIS_SAVE_DIR` to avoid polluting the tree.
