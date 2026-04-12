#include "threading/ThreadPool.h"

#include <algorithm>

ThreadPool::ThreadPool(uint32_t threadCount)
    : workerCount(threadCount)
{
    workers.reserve(workerCount);
    for (uint32_t i = 0; i < workerCount; ++i)
        workers.emplace_back(&ThreadPool::workerLoop, this, i);
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
        generation += 1;
    }
    workerCv.notify_all();

    for (auto& worker : workers)
    {
        if (worker.joinable())
            worker.join();
    }
}

uint32_t ThreadPool::defaultThreadCount()
{
    const uint32_t concurrency = std::thread::hardware_concurrency();
    if (concurrency <= 1)
        return 1;

    return std::max(1u, concurrency - 1u);
}

void ThreadPool::parallelForImpl(uint32_t taskCount, void (*taskInvoker)(void*, uint32_t), void* userData)
{
    if (taskCount == 0)
        return;

    if (workerCount == 0 || taskCount == 1)
    {
        for (uint32_t i = 0; i < taskCount; ++i)
            taskInvoker(userData, i);
        return;
    }

    const uint32_t workerTasks = std::min(workerCount, taskCount - 1);

    {
        std::lock_guard<std::mutex> lock(mutex);
        activeWorkerCount = workerTasks;
        completedWorkerCount = 0;
        currentTaskCount = taskCount;
        nextTaskIndex.store(0, std::memory_order_relaxed);
        currentTaskInvoker = taskInvoker;
        currentTaskUserData = userData;
        generation += 1;
    }

    workerCv.notify_all();

    while (true)
    {
        const uint32_t taskIndex = nextTaskIndex.fetch_add(1, std::memory_order_relaxed);
        if (taskIndex >= taskCount)
            break;

        taskInvoker(userData, taskIndex);
    }

    std::unique_lock<std::mutex> lock(mutex);
    completionCv.wait(lock, [&]()
    {
        return completedWorkerCount >= activeWorkerCount;
    });
}

void ThreadPool::workerLoop(uint32_t workerIndex)
{
    uint64_t observedGeneration = 0;

    while (true)
    {
        void (*taskInvoker)(void*, uint32_t) = nullptr;
        void* userData = nullptr;
        bool shouldRun = false;

        {
            std::unique_lock<std::mutex> lock(mutex);
            workerCv.wait(lock, [&]()
            {
                return stopping || generation != observedGeneration;
            });

            if (stopping)
                return;

            observedGeneration = generation;
            taskInvoker = currentTaskInvoker;
            userData = currentTaskUserData;
            shouldRun = workerIndex < activeWorkerCount;
        }

        if (shouldRun)
        {
            while (true)
            {
                const uint32_t taskIndex = nextTaskIndex.fetch_add(1, std::memory_order_relaxed);
                if (taskIndex >= currentTaskCount)
                    break;

                taskInvoker(userData, taskIndex);
            }
        }

        if (!shouldRun)
            continue;

        {
            std::lock_guard<std::mutex> lock(mutex);
            completedWorkerCount += 1;
            if (completedWorkerCount >= activeWorkerCount)
                completionCv.notify_one();
        }
    }
}
