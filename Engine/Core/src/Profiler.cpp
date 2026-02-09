#include "engine/Profiler.h"
#include "engine/JobSystem.h"

#include <algorithm>

using namespace std::chrono;

namespace Genesis::Engine {

Profiler::Profiler() = default;

Profiler::Scope::Scope(Profiler& profiler, const char* name)
    : m_profiler(&profiler), m_name(name), m_start(high_resolution_clock::now()), m_depth(profiler.m_depth) {
    m_profiler->m_depth++;
}

Profiler::Scope::~Scope() {
    if (!m_profiler) return;
    auto end = high_resolution_clock::now();
    double ms = duration<double, std::milli>(end - m_start).count();
    m_profiler->m_depth = std::max(0, m_profiler->m_depth - 1);
    m_profiler->PushSample(m_name, ms, m_depth);
}

void Profiler::BeginFrame() {
    m_start = high_resolution_clock::now();
    ClearSamples();
    CaptureJobStats();
}

void Profiler::EndFrame() {
    auto end = high_resolution_clock::now();
    m_lastFrameMS = duration<double, std::milli>(end - m_start).count();
    m_fps = (m_lastFrameMS > 0.0) ? (1000.0 / m_lastFrameMS) : 0.0;
}

double Profiler::GetLastFrameMS() const { return m_lastFrameMS; }

double Profiler::GetFPS() const { return m_fps; }

const std::vector<Profiler::Sample>& Profiler::GetSamples() const { return m_samples; }

void Profiler::ClearSamples() {
    m_samples.clear();
    m_depth = 0;
}

uint32_t Profiler::GetJobWorkerCount() const { return m_jobWorkers; }

uint32_t Profiler::GetJobQueuedCount() const { return m_jobQueued; }

uint32_t Profiler::GetJobActiveCount() const { return m_jobActive; }

void Profiler::SetFrameAllocatorUsage(size_t usedBytes, size_t capacityBytes) {
    m_frameAllocUsed = usedBytes;
    m_frameAllocCapacity = capacityBytes;
}

size_t Profiler::GetFrameAllocatorUsed() const { return m_frameAllocUsed; }

size_t Profiler::GetFrameAllocatorCapacity() const { return m_frameAllocCapacity; }

void Profiler::CaptureJobStats() {
    if (!JobSystem::IsRunning()) {
        m_jobWorkers = 0;
        m_jobQueued = 0;
        m_jobActive = 0;
        return;
    }

    m_jobWorkers = JobSystem::GetWorkerCount();
    m_jobQueued = JobSystem::GetQueuedJobCount();
    m_jobActive = JobSystem::GetActiveJobCount();
}

void Profiler::PushSample(const char* name, double ms, int depth) {
    Sample sample;
    if (name) {
        sample.name = name;
    }
    sample.ms = ms;
    sample.depth = depth;
    m_samples.push_back(std::move(sample));
}

} // namespace Genesis::Engine
