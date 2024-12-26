#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    // 建構子：建立指定數量的工作執行緒
    explicit ThreadPool(size_t numThreads) : stop_(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queueMutex_);
                        // 如果 queue 为空且沒被 stop，就等待
                        this->condition_.wait(lock, [this] {
                            return this->stop_ || !this->tasks_.empty();
                        });
                        if (this->stop_ && this->tasks_.empty()) {
                            return; // 結束該工作執行緒
                        }
                        // 取得 queue 裡一個待辦任務
                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    // 執行任務
                    task();
                }
            });
        }
    }

    // enqueue(): 加入一個工作
    template <class F, class... Args>
    void enqueue(F&& f, Args&&... args) {
        // 封裝成 void() 形式
        auto taskWrapper = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            // 若已經 stop，就不允許再 enqueue 了(可自行調整策略)
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.push([taskWrapper]() { taskWrapper(); });
        }
        condition_.notify_one();
    }

    // 解構子：等待所有 worker 結束
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

private:
    // 工作執行緒
    std::vector<std::thread> workers_;
    // 工作隊列
    std::queue<std::function<void()>> tasks_;
    // 互斥鎖 & 條件變數
    std::mutex queueMutex_;
    std::condition_variable condition_;
    // 停止旗號
    bool stop_;
};
