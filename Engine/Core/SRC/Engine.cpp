#include "engine/Engine.h"
#include <iostream>

namespace Genesis::Engine {

bool Init(const std::string& config) {
    std::cout << "Genesis Engine initialized (config='" << config << "')" << std::endl;
    return true;
}

void Shutdown() {
    std::cout << "Genesis Engine shutdown" << std::endl;
}

} // namespace Genesis::Engine
