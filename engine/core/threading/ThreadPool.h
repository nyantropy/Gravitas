#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool
{
public:
    explicit ThreadPool(uint32_t threadCount = defaultThreadCount());
    ~ThreadPool();

    template<typename TaskFn>
    void parallelFor(uint32_t taskCount, TaskFn&& fn)
    {
        if (taskCount == 0)
            return;

        TaskContext<TaskFn> ctx{&fn};
        parallelForImpl(taskCount, &ThreadPool::invokeTask<TaskFn>, &ctx);
    }

    uint32_t threadCount() const
    {
        return workerCount;
    }

    static uint32_t defaultThreadCount();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    template<typename TaskFn>
    struct TaskContext
    {
        TaskFn* fn = nullptr;
    };

    template<typename TaskFn>
    static void invokeTask(void* userData, uint32_t taskIndex)
    {
        auto* ctx = static_cast<TaskContext<TaskFn>*>(userData);
        (*ctx->fn)(taskIndex);
    }

    void parallelForImpl(uint32_t taskCount, void (*taskInvoker)(void*, uint32_t), void* userData);
    void workerLoop(uint32_t workerIndex);

    uint32_t                   workerCount = 0;
    std::vector<std::thread>   workers;
    mutable std::mutex         mutex;
    std::condition_variable    workerCv;
    std::condition_variable    completionCv;
    bool                       stopping = false;
    uint64_t                   generation = 0;
    uint32_t                   activeWorkerCount = 0;
    uint32_t                   completedWorkerCount = 0;
    std::atomic<uint32_t>      nextTaskIndex = 0;
    uint32_t                   currentTaskCount = 0;
    void (*currentTaskInvoker)(void*, uint32_t) = nullptr;
    void*                      currentTaskUserData = nullptr;
};
