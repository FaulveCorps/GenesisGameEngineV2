#include "engine/Profiler.h"
#include "engine/JobSystem.h"

using namespace std::chrono;

namespace Genesis::Engine {

Profiler::Profiler() = default;

void Profiler::BeginFrame() {
    m_start = high_resolution_clock::now();
    CaptureJobStats();
}

void Profiler::EndFrame() {
    auto end = high_resolution_clock::now();
    m_lastFrameMS = duration<double, std::milli>(end - m_start).count();
    m_fps = (m_lastFrameMS > 0.0) ? (1000.0 / m_lastFrameMS) : 0.0;
}

double Profiler::GetLastFrameMS() const { return m_lastFrameMS; }

double Profiler::GetFPS() const { return m_fps; }

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

} // namespace Genesis::Engine
