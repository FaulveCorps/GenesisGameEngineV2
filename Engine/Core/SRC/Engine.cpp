#include "engine/Engine.h"
#include <iostream>

namespace Genesis::Engine {

// Forward declarations for built-in subsystem registrations
void RegisterNullAudioFactory();

bool Init(const std::string& config) {
    // Ensure built-in subsystems are registered
    RegisterNullAudioFactory();

    std::cout << "Genesis Engine initialized (config='" << config << "')" << std::endl;
    return true;
}

void Shutdown() {
    std::cout << "Genesis Engine shutdown" << std::endl;
}

} // namespace Genesis::Engine
