#include <iostream>
#include "engine/Engine.h"

int main(int argc, char** argv) {
    if (!Genesis::Engine::Init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }

    std::cout << "SampleGame running..." << std::endl;

    Genesis::Engine::Shutdown();
    return 0;
}
