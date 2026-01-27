#include "engine/JobSystem.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace Genesis::Engine {
namespace {
    std::mutex g_mutex;
    std::condition_variable g_workerCv;
    std::condition_variable g_idleCv;
    std::deque<std::function<void()>> g_jobs;
    std::vector<std::thread> g_workers;
    std::atomic<uint32_t> g_activeJobs{0};
    bool g_running = false;

    void WorkerLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(g_mutex);
                g_workerCv.wait(lock, [] { return !g_running || !g_jobs.empty(); });
                if (!g_running && g_jobs.empty()) {
                    break;
                }
                job = std::move(g_jobs.front());
                g_jobs.pop_front();
                g_activeJobs.fetch_add(1, std::memory_order_relaxed);
            }

            job();

            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_activeJobs.fetch_sub(1, std::memory_order_relaxed);
                if (g_jobs.empty() && g_activeJobs.load(std::memory_order_relaxed) == 0) {
                    g_idleCv.notify_all();
                } else {
                    g_idleCv.notify_all();
                }
            }
        }
    }
}

void JobSystem::Start(uint32_t workerCount) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_running) return;

    if (workerCount == 0) {
        workerCount = std::max<uint32_t>(1u, std::thread::hardware_concurrency());
    }

    g_running = true;
    g_workers.reserve(workerCount);
    for (uint32_t i = 0; i < workerCount; ++i) {
        g_workers.emplace_back(&WorkerLoop);
    }
}

void JobSystem::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_running) return;
        g_running = false;
    }
    g_workerCv.notify_all();

    for (auto& t : g_workers) {
        if (t.joinable()) t.join();
    }
    g_workers.clear();

    std::lock_guard<std::mutex> lock(g_mutex);
    g_jobs.clear();
    g_activeJobs.store(0, std::memory_order_relaxed);
}

bool JobSystem::IsRunning() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_running;
}

void JobSystem::EnsureStarted() {
    if (!IsRunning()) {
        Start();
    }
}

JobSystem::JobHandle JobSystem::Enqueue(const std::function<void()>& job) {
    if (!job) return JobHandle{};
    EnsureStarted();

    auto counter = std::make_shared<std::atomic<uint32_t>>(1);
    std::function<void()> wrapped = [job, counter]() {
        job();
        counter->fetch_sub(1, std::memory_order_release);
        g_idleCv.notify_all();
    };

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_jobs.push_back(std::move(wrapped));
    }
    g_workerCv.notify_one();
    return JobHandle(counter);
}

JobSystem::JobHandle JobSystem::ParallelFor(uint32_t count, uint32_t batchSize, const std::function<void(uint32_t)>& job) {
    if (!job) return JobHandle{};
    if (count == 0) {
        return JobHandle(std::make_shared<std::atomic<uint32_t>>(0));
    }

    EnsureStarted();
    if (batchSize == 0) batchSize = 1;

    uint32_t batchCount = (count + batchSize - 1) / batchSize;
    auto counter = std::make_shared<std::atomic<uint32_t>>(batchCount);

    for (uint32_t batch = 0; batch < batchCount; ++batch) {
        uint32_t start = batch * batchSize;
        uint32_t end = std::min<uint32_t>(count, start + batchSize);
        std::function<void()> wrapped = [job, counter, start, end]() {
            for (uint32_t i = start; i < end; ++i) {
                job(i);
            }
            counter->fetch_sub(1, std::memory_order_release);
            g_idleCv.notify_all();
        };
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_jobs.push_back(std::move(wrapped));
        }
        g_workerCv.notify_one();
    }

    return JobHandle(counter);
}

void JobSystem::WaitIdle() {
    std::unique_lock<std::mutex> lock(g_mutex);
    g_idleCv.wait(lock, [] {
        return g_jobs.empty() && g_activeJobs.load(std::memory_order_relaxed) == 0;
    });
}

uint32_t JobSystem::GetWorkerCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<uint32_t>(g_workers.size());
}

uint32_t JobSystem::GetActiveJobCount() {
    return g_activeJobs.load(std::memory_order_relaxed);
}

uint32_t JobSystem::GetQueuedJobCount() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return static_cast<uint32_t>(g_jobs.size());
}

bool JobSystem::JobHandle::IsComplete() const {
    return !m_counter || m_counter->load(std::memory_order_acquire) == 0;
}

void JobSystem::JobHandle::Wait() const {
    if (!m_counter) return;
    std::unique_lock<std::mutex> lock(g_mutex);
    g_idleCv.wait(lock, [&]() { return m_counter->load(std::memory_order_acquire) == 0; });
}

} // namespace Genesis::Engine
