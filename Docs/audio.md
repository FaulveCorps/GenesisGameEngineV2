# Audio Subsystem (miniaudio)

Overview
- The engine exposes an Audio subsystem via `IAudio` (a subclass of `ISubsystem`).
- Two backends are provided: `null` (no-op) and `miniaudio` (actual playback using `miniaudio`).

How to use
- At engine init (`Genesis::Engine::Init()`), built-in factories are registered (including `null` and, if available, `miniaudio`).
- Create a global audio subsystem with:

    Genesis::Engine::CreateAudioSubsystem("miniaudio"); // or "null"
    auto audio = Genesis::Engine::GetAudioSubsystem();

- Play a one-shot sound:

    audio->PlayOneShot("assets/audio/sound.wav", 0.9f);

Notes
- The `miniaudio` backend is fetched via CMake's `FetchContent` (it will be available automatically if git fetch succeeds).
- Playback support is limited to formats supported by the bundled miniaudio build (WAV by default). If you want broader codec support, add libvorbis/libopus support to the miniaudio fetch step.
- On Windows, `miniaudio` is built as part of the project and does not require additional runtime DLLs by default.

Testing
- Unit tests check the availability and basic initialization of the `miniaudio` backend when compiled with `HAVE_MINIAUDIO`.
- To add a test that exercises actual playback, add a small WAV under `tests/assets/` and a unit test that calls `PlayOneShot` and verifies no crash (see TODOs).
