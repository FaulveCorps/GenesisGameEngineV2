#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    if (!Genesis::Engine::Init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }

    Genesis::Engine::Window window;
    if (!window.Init("SampleGame - Genesis", 1280, 720)) {
        std::cerr << "Failed to create window" << std::endl;
        Genesis::Engine::Shutdown();
        return -1;
    }

    std::cout << "Entering main loop (close window to exit)..." << std::endl;
    while (window.PollEvents()) {
        // placeholder for update/render
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
