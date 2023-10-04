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
#include <chrono>
#include <vector>
#include <cassert>
#include <unordered_map>
#include <algorithm>
namespace nysy {
    class ThreadPool {
        std::unordered_map<size_t,std::thread> threads;
        std::list<size_t> kill_ids;
        std::list<std::packaged_task<void()>> cache;
        bool stopped = false, adjust_enabled = true;
        std::condition_variable add_task_cv, end_task_cv;
        std::mutex pool_lock;
        size_t max_thread_count = 0, min_thread_count = 0;
        std::atomic<size_t> working_thread_count = 0, alive_thread_count = 0, kill_thread_count = 0, max_id = 0;
        size_t adjust_intervals = 0;
    public:
        ThreadPool(size_t thread_count = std::thread::hardware_concurrency(), bool adjust_enabled = true, size_t max_thread = 100, size_t min_thread = 1, size_t adjust_intervals = 3000)
            :adjust_intervals(adjust_intervals), max_thread_count(max_thread), min_thread_count(min_thread), adjust_enabled(adjust_enabled) {
            if (adjust_enabled) {
                assert(("Invalid Thread Count", max_thread > 0 && min_thread > 0 && max_thread >= min_thread));
                std::thread n_thread(&ThreadPool::manage, this);
                threads.emplace(max_id++, std::move(n_thread));
            }
            assert(("Invalid Thread Count",thread_count > 0 && thread_count >= min_thread));
            for (size_t i = 1; i <= thread_count; ++i) {
                size_t id = max_id++;
                std::thread n_thread(&ThreadPool::exec, this, id);
                threads.emplace(id, std::move(n_thread));
                alive_thread_count++;
            }
        }
        void set_max_thread_count(size_t val) {
            assert(("Invalid Max Thread Count",val > 0 && val >= min_thread_count));
            max_thread_count = val;
        }
        void set_min_thread_count(size_t val) {
            assert(("Invalid Min Thread Count", val > 0 && val <= max_thread_count));
            min_thread_count = val;
        }
        void set_adjust_enabled(bool enabled) { adjust_enabled = enabled; }
        void set_adjust_intervals(size_t intervals_ms) { adjust_intervals = intervals_ms; }
        size_t get_max_thread_count()const { return max_thread_count; }
        size_t get_min_thread_count()const { return min_thread_count; }
        size_t get_working_thread_count()const { return working_thread_count; }
        size_t get_alive_thread_count()const { return alive_thread_count; }
        size_t get_adjust_intervals()const { return adjust_intervals; }
        bool is_adjust_enabled()const { return adjust_enabled; }
        bool is_stopped()const { return stopped; }
        template<class Fn, class... Args> std::shared_future<typename std::invoke_result<Fn, Args...>::type> add_task(Fn func, Args&&... args) {
            auto uniqueFuture = std::async(std::launch::deferred, func, std::forward<Args>(args)...);
            auto sharedFuture = uniqueFuture.share();
            cache.emplace_back([sharedFuture]() {sharedFuture.wait(); });
            add_task_cv.notify_one();
            return sharedFuture;
        }
        template<class Fn, class... Args> std::shared_future<typename std::invoke_result<Fn, Args...>::type> add_task_delay(size_t delay_ms, Fn func, Args&&... args) {
            auto uniqueFuture = std::async(std::launch::deferred, func, std::forward<Args>(args)...);
            auto sharedFuture = uniqueFuture.share();
            cache.emplace_back([sharedFuture, delay_ms]() {std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms)); sharedFuture.wait(); });
            add_task_cv.notify_one();
            return sharedFuture;
        }
        void wait() {
            if (!stopped) {
                std::unique_lock<std::mutex> locker{pool_lock};
                end_task_cv.wait(locker, [=]() {return cache.empty() && working_thread_count == 0; });
            }
        }
        void stop_and_join() {
            std::unique_lock<std::mutex> locker{pool_lock};
            if (!stopped) {
                stopped = true;
                locker.unlock();
                add_task_cv.notify_all();
                for (auto& [k, th] : threads) {
                    th.join();
                }
            }
        }
        void stop_and_detach() {
            std::unique_lock<std::mutex> locker{pool_lock};
            if (!stopped) {
                stopped = true;
                locker.unlock();
                add_task_cv.notify_all();
                for (auto& [k, th] : threads) {
                    th.detach();
                }
            }
        }
        ~ThreadPool() {
            stop_and_join();
        }
    private:
        void exec(size_t id) {
            while (!stopped) {
                std::unique_lock<std::mutex> locker(pool_lock);
                add_task_cv.wait(locker, [=]() {return !(this->cache.empty()) || this->stopped || kill_thread_count > 0; });
                if (stopped)return;
                if (kill_thread_count > 0) {
                    alive_thread_count--;
                    kill_thread_count--;
                    kill_ids.push_back(id);
                    return;
                }
                std::packaged_task<void()> task{std::move(cache.front())};
                cache.pop_front();
                locker.unlock();
                ++working_thread_count;
                task();
                --working_thread_count;
                end_task_cv.notify_all();
            }
        }
        void manage() {
            while (!stopped && adjust_enabled) {
                std::this_thread::sleep_for(std::chrono::milliseconds(adjust_intervals));
                if (stopped || !adjust_enabled)return;
                std::unique_lock<std::mutex> locker{pool_lock};
                while (!kill_ids.empty()) {
                    size_t id = kill_ids.front();
                    threads.at(id).join();
                    threads.erase(id);
                    kill_ids.pop_front();
                }
                if (cache.empty() && alive_thread_count > working_thread_count * 2 && alive_thread_count > min_thread_count) {
                    kill_thread_count.store(std::min<size_t>(alive_thread_count - working_thread_count, alive_thread_count - min_thread_count));
                    locker.unlock();
                    add_task_cv.notify_all();
                }
                else if (!cache.empty() && cache.size() > alive_thread_count && alive_thread_count < max_thread_count) {
                    size_t add_count = std::min<size_t>(max_thread_count - alive_thread_count, cache.size() - alive_thread_count);
                    locker.unlock();
                    for (size_t i = 0; i < add_count; ++i) {
                        size_t id = max_id++;
                        std::thread n_thread(&ThreadPool::exec, this, id);
                        threads.emplace(id,std::move(n_thread));
                        alive_thread_count++;
                    }
                }
                else locker.unlock();
            }
        }
    };
}//namespace nysy
#endif
