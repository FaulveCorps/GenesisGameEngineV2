#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>

namespace Genesis::Engine {

class Profiler {
public:
    struct Sample {
        std::string name;
        double ms = 0.0;
        int depth = 0;
    };

    struct FrameRecord {
        double frameMs = 0.0;
        double fps = 0.0;
        uint32_t jobWorkers = 0;
        uint32_t jobQueued = 0;
        uint32_t jobActive = 0;
        int drawCalls = 0;
        size_t frameAllocUsed = 0;
        size_t frameAllocCapacity = 0;
        std::vector<Sample> samples;
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

    const std::deque<FrameRecord>& GetFrameHistory() const;
    void SetHistoryCapacity(size_t capacity);
    size_t GetHistoryCapacity() const;

    uint32_t GetJobWorkerCount() const;
    uint32_t GetJobQueuedCount() const;
    uint32_t GetJobActiveCount() const;
    int GetDrawCalls() const;

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
    int m_drawCalls = 0;
    size_t m_frameAllocUsed = 0;
    size_t m_frameAllocCapacity = 0;
    int m_depth = 0;
    std::vector<Sample> m_samples;
    size_t m_historyCapacity = 240;
    std::deque<FrameRecord> m_history;
};

} // namespace Genesis::Engine
