#include "catch_amalgamated.hpp"
#include "ENGINE/Engine.h"
#include "ENGINE/IAudio.h"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <thread>
#include <chrono>

static bool write_test_wav(const std::filesystem::path& path) {
    const int sampleRate = 44100;
    const int durationMs = 300; // 300ms tone
    const int numSamples = sampleRate * durationMs / 1000;
    const int16_t amplitude = 16000;
    const double freq = 440.0;
    const double PI = 3.14159265358979323846;

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 1;
    uint32_t byteRate = sampleRate * numChannels * 16/8;
    uint16_t blockAlign = numChannels * 16/8;
    uint16_t bitsPerSample = 16;
    uint32_t subchunk2Size = static_cast<uint32_t>(numSamples * numChannels * bitsPerSample/8);
    uint32_t chunkSize = 4 + (8 + subchunk1Size) + (8 + subchunk2Size);

    ofs.write("RIFF", 4);
    ofs.write(reinterpret_cast<const char*>(&chunkSize), 4);
    ofs.write("WAVE", 4);
    ofs.write("fmt ", 4);
    ofs.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    ofs.write(reinterpret_cast<const char*>(&audioFormat), 2);
    ofs.write(reinterpret_cast<const char*>(&numChannels), 2);
    ofs.write(reinterpret_cast<const char*>(&sampleRate), 4);
    ofs.write(reinterpret_cast<const char*>(&byteRate), 4);
    ofs.write(reinterpret_cast<const char*>(&blockAlign), 2);
    ofs.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    ofs.write("data", 4);
    ofs.write(reinterpret_cast<const char*>(&subchunk2Size), 4);

    for (int i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double s = sin(2.0 * PI * freq * t);
        int16_t sample = static_cast<int16_t>(amplitude * s);
        ofs.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
    }
    ofs.close();
    return true;
}

TEST_CASE("Audio playback: miniaudio plays generated WAV", "[subsystem][audio][miniaudio][playback]") {
#ifdef HAVE_MINIAUDIO
    Genesis::Engine::Init();

    bool ok = Genesis::Engine::CreateAudioSubsystem("miniaudio");
    if (!ok) {
        WARN("miniaudio backend not available; skipping playback test");
        Genesis::Engine::Shutdown();
        SUCCEED("miniaudio not available");
        return;
    }

    auto a = Genesis::Engine::GetAudioSubsystem();
    REQUIRE(a != nullptr);

    auto tmp = std::filesystem::temp_directory_path() / "test_miniaudio_tone.wav";
    if (!write_test_wav(tmp)) {
        FAIL("Failed to write test WAV file");
        Genesis::Engine::Shutdown();
        return;
    }

    bool played = a->PlayOneShot(tmp.string(), 0.8f);
    if (!played) {
        WARN("PlayOneShot returned false (likely no audio device); skipping playback assertions");
        Genesis::Engine::CreateAudioSubsystem("null");
        std::error_code ec; std::filesystem::remove(tmp, ec);
        Genesis::Engine::Shutdown();
        SUCCEED("Playback not supported on this host");
        return;
    }

    // Let playback start briefly then stop
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    a->StopAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Switch back to null to allow clean shutdown
    Genesis::Engine::CreateAudioSubsystem("null");

    std::error_code ec; std::filesystem::remove(tmp, ec);
    if (ec) WARN("Failed to remove temp wav: " << ec.message());
    Genesis::Engine::Shutdown();
#else
    SUCCEED("No miniaudio support at compile time; skipping") ;
#endif
}
