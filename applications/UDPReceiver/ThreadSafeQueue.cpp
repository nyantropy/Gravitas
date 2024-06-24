#include "ThreadSafeQueue.h"
#include "FrameData.h"
#include "ThreadSafeQueueInstantiations.h"

template <typename T>
void ThreadSafeQueue<T>::push(const T& item) 
{
    std::lock_guard<std::mutex> lock(queueMutex);
    queue.push(item);
    queueCondVar.notify_one();
}

template <typename T>
bool ThreadSafeQueue<T>::pop(T& item) 
{
    std::unique_lock<std::mutex> lock(queueMutex);
    queueCondVar.wait(lock, [this]{ return !queue.empty(); });
    if (queue.empty()) 
    {
        return false;
    }

    item = queue.front();
    queue.pop();
    return true;
}

template <typename T>
bool ThreadSafeQueue<T>::empty() 
{
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.empty();
}

template <typename T>
int ThreadSafeQueue<T>::size() 
{
    std::lock_guard<std::mutex> lock(queueMutex);
    return queue.size();
}