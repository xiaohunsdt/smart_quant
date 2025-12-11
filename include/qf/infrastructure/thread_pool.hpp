#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>

namespace qf {

class ThreadPool {
public:
    // 创建固定 worker 数量的线程池。
    explicit ThreadPool(std::size_t workers);
    ~ThreadPool();

    // 投递任务，FIFO 执行。
    void enqueue(std::function<void()> job);
    // 停止线程池并等待退出。
    void stop();

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::condition_variable cv_;
    std::mutex mutex_;
    std::atomic<bool> running_{true};
};

} // namespace qf

