// ThreadPool.h
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();
    
    void start();
    void stop();
    void enqueue(std::function<void()> task);
    
    size_t queueSize() const;
    size_t activeThreads() const;

private:
    void workerThread();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> activeThreads_{0};
};

#endif // THREAD_POOL_H
