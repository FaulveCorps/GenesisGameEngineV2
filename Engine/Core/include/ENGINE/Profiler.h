#pragma once

#include <chrono>

namespace Genesis::Engine {

class Profiler {
public:
    Profiler();
    void BeginFrame();
    void EndFrame();
    double GetLastFrameMS() const;
    double GetFPS() const;

private:
    std::chrono::high_resolution_clock::time_point m_start;
    double m_lastFrameMS = 0.0;
    double m_fps = 0.0;
};

} // namespace Genesis::Engine
