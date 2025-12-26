#pragma once

#include "engine/ISubsystem.h"
#include <string>

namespace Genesis::Engine {

class IAudio : public ISubsystem {
public:
    virtual ~IAudio() = default;

    // Play a transient sound (asset path or identifier)
    virtual bool PlayOneShot(const std::string& assetPath, float volume = 1.0f) = 0;

    // Stop all currently playing sounds
    virtual void StopAll() = 0;
};

} // namespace Genesis::Engine
