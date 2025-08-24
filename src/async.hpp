#pragma once

#include <future>
#include <stop_token>
#include <functional>
#include <thread>
#include <atomic>

namespace tj::util {

// 异步任务包装器类 (Async task wrapper class)
// 提供对std::jthread和std::future的封装，支持取消操作
// Provides wrapper for std::jthread and std::future with cancellation support
template<typename T>
class AsyncFurture {
private:
    std::jthread thread_;           // 工作线程 (Worker thread)
    std::future<T> future_;         // 异步结果 (Async result)
    std::atomic<bool> started_{false}; // 是否已启动 (Whether started)
    
public:
    AsyncFurture() = default;
    
    // 禁止拷贝构造和赋值 (Disable copy construction and assignment)
    AsyncFurture(const AsyncFurture&) = delete;
    AsyncFurture& operator=(const AsyncFurture&) = delete;
    
    // 支持移动构造和赋值 (Support move construction and assignment)
    AsyncFurture(AsyncFurture&& other) noexcept 
        : thread_(std::move(other.thread_))
        , future_(std::move(other.future_))
        , started_(other.started_.load()) {
        other.started_ = false;
    }
    
    AsyncFurture& operator=(AsyncFurture&& other) noexcept {
        if (this != &other) {
            // 先停止当前任务 (Stop current task first)
            stop();
            
            thread_ = std::move(other.thread_);
            future_ = std::move(other.future_);
            started_ = other.started_.load();
            other.started_ = false;
        }
        return *this;
    }
    
    // 析构函数，自动停止任务 (Destructor, automatically stop task)
    ~AsyncFurture() {
        stop();
    }
    
    // 启动异步任务 (Start async task)
    // func: 接受std::stop_token参数的可调用对象 (Callable object that accepts std::stop_token)
    template<typename Func>
    void start(Func&& func) {
        // 如果已经启动，先停止 (If already started, stop first)
        if (started_.load()) {
            stop();
        }
        
        // 创建promise用于获取结果 (Create promise to get result)
        auto promise = std::make_shared<std::promise<T>>();
        future_ = promise->get_future();
        
        // 启动新线程 (Start new thread)
        thread_ = std::jthread([promise, func = std::forward<Func>(func)](std::stop_token stop_token) {
            try {
                if constexpr (std::is_void_v<T>) {
                    // 对于void返回类型 (For void return type)
                    func(stop_token);
                    promise->set_value();
                } else {
                    // 对于非void返回类型 (For non-void return type)
                    auto result = func(stop_token);
                    promise->set_value(std::move(result));
                }
            } catch (...) {
                // 捕获异常并传递给future (Catch exception and pass to future)
                promise->set_exception(std::current_exception());
            }
        });
        
        started_ = true;
    }
    
    // 停止异步任务 (Stop async task)
    void stop() {
        if (started_.load()) {
            // 请求停止 (Request stop)
            if (thread_.joinable()) {
                thread_.request_stop();
                thread_.join();
            }
            started_ = false;
        }
    }
    
    // 检查任务是否完成 (Check if task is completed)
    bool is_ready() const {
        if (!started_.load()) {
            return false;
        }
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    
    // 检查任务是否正在运行 (Check if task is running)
    bool is_running() const {
        return started_.load() && !is_ready();
    }
    
    // 获取结果（阻塞等待）(Get result, blocking wait)
    T get() {
        if (!started_.load()) {
            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                throw std::runtime_error("AsyncFurture not started");
            }
        }
        return future_.get();
    }
    
    // 尝试获取结果（非阻塞）(Try to get result, non-blocking)
    // 返回std::optional，如果任务未完成则返回std::nullopt
    // Returns std::optional, returns std::nullopt if task not completed
    std::optional<T> try_get() {
        if (!is_ready()) {
            return std::nullopt;
        }
        
        if constexpr (std::is_void_v<T>) {
            future_.get(); // 可能抛出异常 (May throw exception)
            return {}; // 对于void类型，返回空的optional (For void type, return empty optional)
        } else {
            return future_.get();
        }
    }
    
    // 等待指定时间 (Wait for specified time)
    template<typename Rep, typename Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout_duration) const {
        if (!started_.load()) {
            return std::future_status::deferred;
        }
        return future_.wait_for(timeout_duration);
    }
    
    // 等待直到指定时间点 (Wait until specified time point)
    template<typename Clock, typename Duration>
    std::future_status wait_until(const std::chrono::time_point<Clock, Duration>& timeout_time) const {
        if (!started_.load()) {
            return std::future_status::deferred;
        }
        return future_.wait_until(timeout_time);
    }
};

// 特化版本：void类型的try_get (Specialized version: try_get for void type)
template<>
inline std::optional<void> AsyncFurture<void>::try_get() {
    if (!is_ready()) {
        return std::nullopt;
    }
    
    future_.get(); // 可能抛出异常 (May throw exception)
    return std::make_optional(); // 返回有值的optional (Return optional with value)
}

} // namespace tj::util