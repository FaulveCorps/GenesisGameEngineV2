#include "engine/Profiler.h"

using namespace std::chrono;

namespace Genesis::Engine {

Profiler::Profiler() = default;

void Profiler::BeginFrame() {
    m_start = high_resolution_clock::now();
}

void Profiler::EndFrame() {
    auto end = high_resolution_clock::now();
    m_lastFrameMS = duration<double, std::milli>(end - m_start).count();
    m_fps = (m_lastFrameMS > 0.0) ? (1000.0 / m_lastFrameMS) : 0.0;
}

double Profiler::GetLastFrameMS() const { return m_lastFrameMS; }

double Profiler::GetFPS() const { return m_fps; }

} // namespace Genesis::Engine
