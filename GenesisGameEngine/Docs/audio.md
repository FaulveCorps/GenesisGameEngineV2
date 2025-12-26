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

Playback test
- There's a unit test `tests/test_audio_playback.cpp` that generates a short WAV file at runtime and attempts to play it using the `miniaudio` backend (to avoid bundling binary assets in the repo).
- The test is tolerant of systems without an audio device: if playback is not possible, the test will skip gracefully.

CMake
- The tests' CMake configuration will copy an available `miniaudio.dll` to the test output directory on Windows (if the DLL exists), ensuring runtime dependencies are available for the playback test.

Testing
- Unit tests check the availability and basic initialization of the `miniaudio` backend when compiled with `HAVE_MINIAUDIO`.
- To add a test that exercises actual playback with a committed asset, add a small WAV under `tests/assets/` and update `tests/test_audio_playback.cpp` to use the checked-in file.
