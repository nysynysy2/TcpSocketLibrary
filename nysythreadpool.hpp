#ifndef _NYSY_THREAD_POOL_
#define _NYSY_THREAD_POOL_
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <list>
#include <future>
#include <functional>
#include <mutex>
namespace nysy {
    class ThreadPool {
        std::list<std::thread> threads;
        std::list<std::packaged_task<void()>> cache;
        bool isStopped = false;
        std::condition_variable add_task_cv, end_task_cv;
        std::mutex pool_lock;
        std::atomic<unsigned int> working_count = 0;
    public:
        ThreadPool(size_t thread_count = std::thread::hardware_concurrency()) {
            for (int i = 0; i < thread_count; ++i) {
                threads.push_back(std::thread(&ThreadPool::exec, this));
            }
        }
        template<class Fn, class... Args> auto addTask(Fn func, Args... args) {
            auto uniqueFuture = std::async(std::launch::deferred, func, std::forward<Args>(args)...);
            auto sharedFuture = uniqueFuture.share();
            cache.emplace_back([sharedFuture]() {sharedFuture.wait(); });
            add_task_cv.notify_one();
            return sharedFuture;
        }
        void wait() {
            if (!isStopped) {
                std::unique_lock locker(pool_lock);
                end_task_cv.wait(locker, [=]() {return cache.empty() && working_count == 0; });
            }
        }
        void exec() {
            while (!isStopped) {
                std::unique_lock<std::mutex> locker(pool_lock);
                add_task_cv.wait(locker, [=]() {return !(this->cache.empty()) || this->isStopped; });
                if (isStopped)return;
                std::packaged_task<void()> task = std::move(cache.front());
                cache.pop_front();
                locker.unlock();
                ++working_count;
                task();
                --working_count;
                end_task_cv.notify_all();
            }
        }
        ~ThreadPool() {
            if (!isStopped) {
                isStopped = true;
                add_task_cv.notify_all();
                for (auto& th : threads) {
                    th.join();
                }
            }
        }
    };
}//namespace nysy
#endif
