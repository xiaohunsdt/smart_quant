#include "qf/infrastructure/thread_pool.hpp"

namespace qf {

ThreadPool::ThreadPool(std::size_t workers) {
    if (workers == 0) {
        workers = 1;
    }
    // 启动指定数量的工作线程。
    for (std::size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::enqueue(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
    }
    cv_.notify_one();
}

void ThreadPool::stop() {
    // 通知线程退出并等待回收。
    running_.store(false);
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::worker_loop() {
    while (running_.load()) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return !running_.load() || !jobs_.empty(); });
            if (!running_.load() && jobs_.empty()) return;
            job = std::move(jobs_.front());
            jobs_.pop();
        }
        // 执行单个任务。
        if (job) job();
    }
}

} // namespace qf

