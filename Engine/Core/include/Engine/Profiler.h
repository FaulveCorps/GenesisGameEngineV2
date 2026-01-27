#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace Genesis::Engine {

class Profiler {
public:
    Profiler();
    void BeginFrame();
    void EndFrame();
    double GetLastFrameMS() const;
    double GetFPS() const;

    uint32_t GetJobWorkerCount() const;
    uint32_t GetJobQueuedCount() const;
    uint32_t GetJobActiveCount() const;

    void SetFrameAllocatorUsage(size_t usedBytes, size_t capacityBytes);
    size_t GetFrameAllocatorUsed() const;
    size_t GetFrameAllocatorCapacity() const;

private:
    void CaptureJobStats();

    std::chrono::high_resolution_clock::time_point m_start;
    double m_lastFrameMS = 0.0;
    double m_fps = 0.0;
    uint32_t m_jobWorkers = 0;
    uint32_t m_jobQueued = 0;
    uint32_t m_jobActive = 0;
    size_t m_frameAllocUsed = 0;
    size_t m_frameAllocCapacity = 0;
};

} // namespace Genesis::Engine
