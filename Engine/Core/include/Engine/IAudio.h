#pragma once

#include "engine/ISubsystem.h"
#include <algorithm>
#include <string>

namespace Genesis::Engine {

struct AudioPlayParams {
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool spatial = false;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float minDistance = 1.0f;
    float maxDistance = 20.0f;
};

struct AudioListener {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float forward[3] = {0.0f, 0.0f, -1.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
};

class IAudio : public ISubsystem {
public:
    virtual ~IAudio() = default;

    // Play a transient sound (asset path or identifier)
    virtual bool PlayOneShot(const std::string& assetPath, float volume = 1.0f) = 0;

    // Play a transient sound with extended parameters.
    virtual bool PlayOneShot(const std::string& assetPath, const AudioPlayParams& params) {
        return PlayOneShot(assetPath, params.volume);
    }

    // Stop all currently playing sounds
    virtual void StopAll() = 0;

    // Set listener transform for spatial audio.
    virtual void SetListener(const AudioListener& /*listener*/) {}

    // Master volume (0..1+). Default implementation stores value only.
    virtual void SetMasterVolume(float volume) { m_masterVolume = std::max(0.0f, volume); }
    virtual float GetMasterVolume() const { return m_masterVolume; }

protected:
    float m_masterVolume = 1.0f;
};

} // namespace Genesis::Engine
