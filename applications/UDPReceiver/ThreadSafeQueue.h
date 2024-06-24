#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue 
{
    private:
        std::queue<T> queue;
        std::mutex queueMutex;
        std::condition_variable queueCondVar;

    public:
        void push(const T& item);
        bool pop(T& item);
        int size();
        bool empty();
};