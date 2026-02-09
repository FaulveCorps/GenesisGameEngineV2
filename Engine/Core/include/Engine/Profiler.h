#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Genesis::Engine {

class Profiler {
public:
    struct Sample {
        std::string name;
        double ms = 0.0;
        int depth = 0;
    };

    class Scope {
    public:
        Scope(Profiler& profiler, const char* name);
        ~Scope();

    private:
        Profiler* m_profiler = nullptr;
        const char* m_name = nullptr;
        std::chrono::high_resolution_clock::time_point m_start;
        int m_depth = 0;
    };

    Profiler();
    void BeginFrame();
    void EndFrame();
    double GetLastFrameMS() const;
    double GetFPS() const;

    const std::vector<Sample>& GetSamples() const;
    void ClearSamples();

    uint32_t GetJobWorkerCount() const;
    uint32_t GetJobQueuedCount() const;
    uint32_t GetJobActiveCount() const;

    void SetFrameAllocatorUsage(size_t usedBytes, size_t capacityBytes);
    size_t GetFrameAllocatorUsed() const;
    size_t GetFrameAllocatorCapacity() const;

private:
    void CaptureJobStats();
    void PushSample(const char* name, double ms, int depth);

    std::chrono::high_resolution_clock::time_point m_start;
    double m_lastFrameMS = 0.0;
    double m_fps = 0.0;
    uint32_t m_jobWorkers = 0;
    uint32_t m_jobQueued = 0;
    uint32_t m_jobActive = 0;
    size_t m_frameAllocUsed = 0;
    size_t m_frameAllocCapacity = 0;
    int m_depth = 0;
    std::vector<Sample> m_samples;
};

} // namespace Genesis::Engine
