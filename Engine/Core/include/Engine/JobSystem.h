#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Genesis::Engine {

class JobSystem {
public:
    class JobHandle {
    public:
        JobHandle() = default;
        bool IsComplete() const;
        void Wait() const;

    private:
        std::shared_ptr<std::atomic<uint32_t>> m_counter;
        explicit JobHandle(std::shared_ptr<std::atomic<uint32_t>> counter) : m_counter(std::move(counter)) {}
        friend class JobSystem;
    };

    static void Start(uint32_t workerCount = 0);
    static void Shutdown();
    static bool IsRunning();

    static JobHandle Enqueue(const std::function<void()>& job);
    static JobHandle ParallelFor(uint32_t count, uint32_t batchSize, const std::function<void(uint32_t index)>& job);

    static void WaitIdle();
    static uint32_t GetWorkerCount();
    static uint32_t GetActiveJobCount();
    static uint32_t GetQueuedJobCount();

private:
    static void EnsureStarted();
};

} // namespace Genesis::Engine
