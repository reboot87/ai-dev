#ifndef THREAD_POOL_CPP11_HPP
#define THREAD_POOL_CPP11_HPP

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <vector>
#include <stdexcept>
#include <memory>
#include <atomic>
#include <chrono>
#include <future>
#include <type_traits>

/* ==================== TaskHandle ==================== */

class TaskCancelled : public std::runtime_error {
public:
    TaskCancelled() : std::runtime_error("task cancelled") {}
};

template <typename T>
class TaskHandle {
public:
    TaskHandle() : state_(nullptr) {}

    bool cancel() {
        if (!state_) return false;
        std::lock_guard<std::mutex> lock(state_->mu);
        if (state_->done || state_->cancelled) return false;
        state_->cancelled = true;
        state_->cv.notify_all();
        return true;
    }

    bool is_cancelled() const {
        return state_ && state_->cancelled.load();
    }

    bool is_done() const {
        return state_ && state_->done.load();
    }

    T get() {
        if (!state_) throw std::runtime_error("invalid handle");
        std::unique_lock<std::mutex> lock(state_->mu);
        state_->cv.wait(lock, [this] { return state_->done || state_->cancelled; });
        if (state_->cancelled) throw TaskCancelled();
        return std::move(state_->result);
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        if (!state_) return true;
        std::unique_lock<std::mutex> lock(state_->mu);
        return state_->cv.wait_for(lock, timeout,
            [this] { return state_->done || state_->cancelled; });
    }

    explicit operator bool() const { return state_ != nullptr; }

    struct State {
        std::mutex mu;
        std::condition_variable cv;
        std::atomic<bool> cancelled{false};
        std::atomic<bool> done{false};
        T result{};
    };

private:
    friend class ThreadPool;
    template <typename R> friend struct Runner;

    explicit TaskHandle(std::shared_ptr<State> s) : state_(std::move(s)) {}
    std::shared_ptr<State> state_;
};

/* ==================== TaskHandle<void> specialization ==================== */

template <>
class TaskHandle<void> {
public:
    TaskHandle() : state_(nullptr) {}

    bool cancel() {
        if (!state_) return false;
        std::lock_guard<std::mutex> lock(state_->mu);
        if (state_->done || state_->cancelled) return false;
        state_->cancelled = true;
        state_->cv.notify_all();
        return true;
    }

    bool is_cancelled() const {
        return state_ && state_->cancelled.load();
    }

    bool is_done() const {
        return state_ && state_->done.load();
    }

    void get() {
        if (!state_) throw std::runtime_error("invalid handle");
        std::unique_lock<std::mutex> lock(state_->mu);
        state_->cv.wait(lock, [this] { return state_->done || state_->cancelled; });
        if (state_->cancelled) throw TaskCancelled();
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        if (!state_) return true;
        std::unique_lock<std::mutex> lock(state_->mu);
        return state_->cv.wait_for(lock, timeout,
            [this] { return state_->done || state_->cancelled; });
    }

    explicit operator bool() const { return state_ != nullptr; }

    struct State {
        std::mutex mu;
        std::condition_variable cv;
        std::atomic<bool> cancelled{false};
        std::atomic<bool> done{false};
    };

private:
    friend class ThreadPool;
    template <typename R> friend struct Runner;

    explicit TaskHandle(std::shared_ptr<State> s) : state_(std::move(s)) {}
    std::shared_ptr<State> state_;
};

/* ==================== Runner (type erasure for submit_r) ==================== */

template <typename R>
struct Runner {
    std::shared_ptr<typename TaskHandle<R>::State> state;
    std::function<R()> func;

    Runner(std::shared_ptr<typename TaskHandle<R>::State> s, std::function<R()> f)
        : state(std::move(s)), func(std::move(f)) {}

    void operator()() {
        try {
            set_result(func());
        } catch (...) {
            std::lock_guard<std::mutex> lk(state->mu);
            state->done = true;
            state->cv.notify_all();
            throw;
        }
        std::lock_guard<std::mutex> lk(state->mu);
        state->done = true;
        state->cv.notify_all();
    }

    template <typename T>
    void set_result(T&& val) { state->result = std::forward<T>(val); }
};

template <>
struct Runner<void> {
    std::shared_ptr<typename TaskHandle<void>::State> state;
    std::function<void()> func;

    Runner(std::shared_ptr<typename TaskHandle<void>::State> s, std::function<void()> f)
        : state(std::move(s)), func(std::move(f)) {}

    void operator()() {
        try {
            func();
        } catch (...) {
            std::lock_guard<std::mutex> lk(state->mu);
            state->done = true;
            state->cv.notify_all();
            throw;
        }
        std::lock_guard<std::mutex> lk(state->mu);
        state->done = true;
        state->cv.notify_all();
    }
};

/* ==================== ThreadPool ==================== */

class ThreadPool {
public:
    explicit ThreadPool(
        size_t min_threads = 2,
        size_t max_threads = std::thread::hardware_concurrency(),
        int idle_timeout_ms = 2000)
        : min_threads_(min_threads)
        , max_threads_(max_threads > 0 ? max_threads : 4)
        , cur_threads_(0)
        , busy_count_(0)
        , stop_(false)
        , idle_timeout_ms_(idle_timeout_ms)
    {
        if (min_threads_ > max_threads_) min_threads_ = max_threads_;
        for (size_t i = 0; i < min_threads_; ++i) spawn_thread();
    }

    ~ThreadPool() { destroy(); }

    /* ---- submit: void task ---- */
    TaskHandle<void> submit(std::function<void()> func) {
        auto state = std::make_shared<typename TaskHandle<void>::State>();
        TaskHandle<void> handle(state);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) throw std::runtime_error("submit on stopped pool");
            tasks_.push([state, func = std::move(func)]() {
                try {
                    func();
                } catch (...) {
                    std::lock_guard<std::mutex> lk(state->mu);
                    state->done = true;
                    state->cv.notify_all();
                    throw;
                }
                std::lock_guard<std::mutex> lk(state->mu);
                state->done = true;
                state->cv.notify_all();
            });
            try_scale_up();
        }
        cv_.notify_one();
        return handle;
    }

    /* ---- submit: task with return value ---- */
    template <typename F, typename... Args>
    auto submit_r(F&& f, Args&&... args)
        -> TaskHandle<typename std::result_of<F(Args...)>::type>
    {
        using R = typename std::result_of<F(Args...)>::type;
        auto state = std::make_shared<typename TaskHandle<R>::State>();
        TaskHandle<R> handle(state);

        /* wrap in std::function to erase type */
        auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) throw std::runtime_error("submit on stopped pool");
            tasks_.push(Runner<R>(state, bound));
            try_scale_up();
        }
        cv_.notify_one();
        return handle;
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        done_cv_.wait(lock, [this] {
            return busy_count_ == 0 && tasks_.empty();
        });
    }

    size_t thread_count() const { return cur_threads_.load(); }
    size_t min_threads()  const { return min_threads_; }
    size_t max_threads()  const { return max_threads_; }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    void spawn_thread() {
        size_t idx = cur_threads_.load();
        workers_.emplace_back([this, idx] { worker_loop(idx); });
        ++cur_threads_;
    }

    void try_scale_up() {
        size_t queued = tasks_.size();
        if (static_cast<size_t>(busy_count_) >= cur_threads_ &&
            queued > 0 && cur_threads_ < max_threads_)
        {
            spawn_thread();
        }
    }

    void worker_loop(size_t) {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                bool got = cv_.wait_for(lock,
                    std::chrono::milliseconds(idle_timeout_ms_),
                    [this] { return stop_ || !tasks_.empty(); });

                if (!got && !stop_ && tasks_.empty()) {
                    if (cur_threads_ > min_threads_) {
                        --cur_threads_;
                        return;
                    }
                    continue;
                }
                if (stop_ && tasks_.empty()) return;

                task = std::move(tasks_.front());
                tasks_.pop();
                ++busy_count_;
            }

            try { task(); }
            catch (...) {}

            {
                std::lock_guard<std::mutex> lock(mutex_);
                --busy_count_;
                if (busy_count_ == 0 && tasks_.empty())
                    done_cv_.notify_all();
            }
        }
    }

    void destroy() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_)
            if (t.joinable()) t.join();

        /* drain remaining queued tasks — execute them so done is set */
        std::queue<std::function<void()>> remaining;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            remaining.swap(tasks_);
        }
        while (!remaining.empty()) {
            try { remaining.front(); } catch (...) {}
            remaining.pop();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    size_t min_threads_;
    size_t max_threads_;
    std::atomic<size_t> cur_threads_;
    std::atomic<size_t> busy_count_;
    bool stop_;
    int idle_timeout_ms_;
};

#endif
