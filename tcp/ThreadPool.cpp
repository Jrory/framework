// ThreadPool.cpp
#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t threads) {
    workers_.reserve(threads);
}

ThreadPool::~ThreadPool() {
    stop();
}

void ThreadPool::start() {
    for (size_t i = 0; i < workers_.capacity(); ++i) {
        workers_.emplace_back([this] { workerThread(); });
    }
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }
    condition_.notify_all();
    
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks_.emplace(std::move(task));
    }
    condition_.notify_one();
}

size_t ThreadPool::queueSize() const {
    std::unique_lock<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

size_t ThreadPool::activeThreads() const {
    return activeThreads_.load();
}

void ThreadPool::workerThread() {
    while (!stop_) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            condition_.wait(lock, [this] { 
                return stop_ || !tasks_.empty(); 
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
            activeThreads_++;
        }
        
        try {
            task();
        } catch (...) {
            activeThreads_--;
            throw;
        }
        
        activeThreads_--;
    }
}