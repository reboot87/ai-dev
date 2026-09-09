#ifndef THREAD_POOL_CPP03_HPP
#define THREAD_POOL_CPP03_HPP

#include "../common/platform.h"
#include <cstdlib>
#include <stdexcept>

/* ==================== TaskHandle ==================== */

struct TaskHandleImpl {
    tp_mutex_t mu;
    tp_cond_t  cv;
    volatile int cancelled;
    volatile int done;
    volatile int refcount;
    void*    result;
    void     (*result_destructor)(void*);

    TaskHandleImpl() : cancelled(0), done(0), refcount(1), result(0), result_destructor(0) {
        tp_mutex_init(&mu);
        tp_cond_init(&cv);
    }
    void addref() {
        tp_mutex_lock(&mu);
        refcount++;
        tp_mutex_unlock(&mu);
    }
    void release() {
        tp_mutex_lock(&mu);
        int n = --refcount;
        tp_mutex_unlock(&mu);
        if (n == 0) {
            if (result && result_destructor) result_destructor(result);
            tp_mutex_destroy(&mu);
            tp_cond_destroy(&cv);
            delete this;
        }
    }
    ~TaskHandleImpl() {
    }

private:
    TaskHandleImpl(const TaskHandleImpl&);
    TaskHandleImpl& operator=(const TaskHandleImpl&);
};

class TaskHandle {
public:
    TaskHandle() : impl_(0) {}
    TaskHandle(const TaskHandle& o) : impl_(o.impl_) { if (impl_) impl_->addref(); }
    TaskHandle& operator=(const TaskHandle& o) {
        if (impl_) impl_->release();
        impl_ = o.impl_;
        if (impl_) impl_->addref();
        return *this;
    }
    ~TaskHandle() { if (impl_) impl_->release(); }

    bool cancel() {
        if (!impl_) return false;
        tp_mutex_lock(&impl_->mu);
        if (impl_->done || impl_->cancelled) {
            tp_mutex_unlock(&impl_->mu);
            return false;
        }
        impl_->cancelled = 1;
        tp_cond_broadcast(&impl_->cv);
        tp_mutex_unlock(&impl_->mu);
        return true;
    }

    bool is_cancelled() const { return impl_ && impl_->cancelled; }
    bool is_done()      const { return impl_ && impl_->done; }

    bool wait(int timeout_ms = -1) {
        if (!impl_) return true;
        tp_mutex_lock(&impl_->mu);
        if (timeout_ms < 0) {
            while (!impl_->done && !impl_->cancelled)
                tp_cond_wait(&impl_->cv, &impl_->mu);
        } else {
            while (!impl_->done && !impl_->cancelled) {
                int rc = tp_cond_wait_timeout(&impl_->cv, &impl_->mu, timeout_ms);
                if (rc == 0 && !impl_->done && !impl_->cancelled) {
                    tp_mutex_unlock(&impl_->mu);
                    return false;
                }
            }
        }
        tp_mutex_unlock(&impl_->mu);
        return true;
    }

    void* result() const { return impl_ ? impl_->result : 0; }
    explicit operator bool() const { return impl_ != 0; }

private:
    friend class ThreadPool;
    explicit TaskHandle(TaskHandleImpl* p) : impl_(p) {}
    TaskHandleImpl* impl_;
};

/* ==================== ThreadPool ==================== */

class ThreadPool {
public:
    typedef void (*TaskFunc)(void* arg);
    typedef void (*ResultFunc)(void* result, void* user_data);

    ThreadPool(int min_threads, int max_threads, int idle_timeout_ms = 2000)
        : threads_(0), active_(0)
        , min_threads_(min_threads), max_threads_(max_threads)
        , cur_threads_(0), busy_count_(0)
        , task_head_(0), task_tail_(0)
        , stop_(false), idle_timeout_ms_(idle_timeout_ms)
    {
        if (min_threads_ <= 0) min_threads_ = 2;
        if (max_threads_ < min_threads_) max_threads_ = min_threads_;
        if (idle_timeout_ms_ <= 0) idle_timeout_ms_ = 2000;

        tp_mutex_init(&mutex_);
        tp_cond_init(&task_cond_);
        tp_cond_init(&done_cond_);

        threads_ = new tp_thread_t[max_threads_];
        active_  = new int[max_threads_];
        for (int i = 0; i < max_threads_; ++i) active_[i] = 0;

        for (int i = 0; i < min_threads_; ++i) {
            if (spawn_thread(i) == 0) cur_threads_++;
        }
    }

    ~ThreadPool() { destroy(); }

    /* ---- submit: void task ---- */
    TaskHandle submit(TaskFunc func, void* arg) {
        TaskHandleImpl* impl = new TaskHandleImpl;
        TaskHandle handle(impl);

        InternalTask* t = static_cast<InternalTask*>(std::calloc(1, sizeof(InternalTask)));
        if (!t) { impl->release(); throw std::bad_alloc(); }
        t->func = func;
        t->arg  = arg;
        t->next = 0;
        t->handle = impl;
        t->handle->addref();

        tp_mutex_lock(&mutex_);
        if (task_tail_) task_tail_->next = t;
        else            task_head_ = t;
        task_tail_ = t;
        try_scale_up();
        tp_cond_signal(&task_cond_);
        tp_mutex_unlock(&mutex_);

        return handle;
    }

    /* ---- submit: task with result callback ---- */
    TaskHandle submit_r(TaskFunc func, void* arg,
                        ResultFunc result_func, void* result_data)
    {
        TaskHandleImpl* impl = new TaskHandleImpl;
        TaskHandle handle(impl);

        InternalTask* t = static_cast<InternalTask*>(std::calloc(1, sizeof(InternalTask)));
        if (!t) { impl->release(); throw std::bad_alloc(); }
        t->func = func;
        t->arg  = arg;
        t->next = 0;
        t->handle = impl;
        t->handle->addref();
        t->result_func = result_func;
        t->result_data = result_data;

        tp_mutex_lock(&mutex_);
        if (task_tail_) task_tail_->next = t;
        else            task_head_ = t;
        task_tail_ = t;
        try_scale_up();
        tp_cond_signal(&task_cond_);
        tp_mutex_unlock(&mutex_);

        return handle;
    }

    void wait_all() {
        tp_mutex_lock(&mutex_);
        while (busy_count_ > 0 || task_head_ != 0)
            tp_cond_wait(&done_cond_, &mutex_);
        tp_mutex_unlock(&mutex_);
    }

    int thread_count() const { return cur_threads_; }

    ThreadPool(const ThreadPool&);
    ThreadPool& operator=(const ThreadPool&);

private:
    struct InternalTask {
        TaskFunc     func;
        void*        arg;
        InternalTask* next;
        TaskHandleImpl* handle;
        ResultFunc   result_func;
        void*        result_data;
    };

    int spawn_thread(int idx) {
        WorkerArg* wa = new WorkerArg;
        wa->pool  = this;
        wa->index = idx;
        active_[idx] = 1;
        if (tp_thread_create(&threads_[idx], worker_fn, wa) != 0) {
            active_[idx] = 0;
            delete wa;
            return -1;
        }
        return 0;
    }

    void try_scale_up() {
        int queued = 0;
        for (InternalTask* t = task_head_; t; t = t->next) queued++;
        if (busy_count_ >= cur_threads_ && queued > 0 && cur_threads_ < max_threads_) {
            if (spawn_thread(cur_threads_) == 0) cur_threads_++;
        }
    }

    struct WorkerArg {
        ThreadPool* pool;
        int index;
    };

    static void* worker_fn(void* data) {
        WorkerArg* wa = static_cast<WorkerArg*>(data);
        ThreadPool* pool = wa->pool;
        int my_idx = wa->index;
        delete wa;

        for (;;) {
            InternalTask* task;

            tp_mutex_lock(&pool->mutex_);
            while (pool->task_head_ == 0 && !pool->stop_) {
                int rc = tp_cond_wait_timeout(&pool->task_cond_, &pool->mutex_,
                                              pool->idle_timeout_ms_);
                if (rc == 0 && pool->task_head_ == 0 && !pool->stop_) {
                    if (pool->cur_threads_ > pool->min_threads_) {
                        pool->cur_threads_--;
                        pool->active_[my_idx] = 0;
                        tp_mutex_unlock(&pool->mutex_);
                        return 0;
                    }
                }
            }

            if (pool->stop_ && pool->task_head_ == 0) {
                pool->cur_threads_--;
                pool->active_[my_idx] = 0;
                tp_mutex_unlock(&pool->mutex_);
                break;
            }

            task = pool->task_head_;
            pool->task_head_ = task->next;
            if (!pool->task_head_) pool->task_tail_ = 0;

            pool->busy_count_++;
            tp_mutex_unlock(&pool->mutex_);

            /* check cancel before execution */
            int cancelled = 0;
            tp_mutex_lock(&task->handle->mu);
            if (task->handle->cancelled) cancelled = 1;
            tp_mutex_unlock(&task->handle->mu);

            if (!cancelled) {
                task->func(task->arg);
                if (task->result_func) {
                    task->result_func(task->arg, task->result_data);
                }
            }

            /* signal done */
            tp_mutex_lock(&task->handle->mu);
            task->handle->done = 1;
            tp_cond_broadcast(&task->handle->cv);
            tp_mutex_unlock(&task->handle->mu);

            task->handle->release();
            std::free(task);

            tp_mutex_lock(&pool->mutex_);
            pool->busy_count_--;
            if (pool->busy_count_ == 0 && pool->task_head_ == 0)
                tp_cond_broadcast(&pool->done_cond_);
            tp_mutex_unlock(&pool->mutex_);
        }
        return 0;
    }

    void destroy() {
        tp_mutex_lock(&mutex_);
        stop_ = true;
        tp_cond_broadcast(&task_cond_);
        tp_mutex_unlock(&mutex_);

        for (int i = 0; i < max_threads_; ++i)
            if (active_[i]) tp_thread_join(threads_[i]);

        InternalTask* t = task_head_;
        while (t) {
            InternalTask* next = t->next;
            /* mark as cancelled so waiters don't block forever */
            tp_mutex_lock(&t->handle->mu);
            t->handle->done = 1;
            tp_cond_broadcast(&t->handle->cv);
            tp_mutex_unlock(&t->handle->mu);
            t->handle->release();
            std::free(t);
            t = next;
        }

        tp_mutex_destroy(&mutex_);
        tp_cond_destroy(&task_cond_);
        tp_cond_destroy(&done_cond_);
        delete[] threads_;
        delete[] active_;
    }

    tp_thread_t* threads_;
    int*         active_;
    int          min_threads_;
    int          max_threads_;
    int          cur_threads_;
    int          busy_count_;
    InternalTask* task_head_;
    InternalTask* task_tail_;
    tp_mutex_t   mutex_;
    tp_cond_t    task_cond_;
    tp_cond_t    done_cond_;
    bool         stop_;
    int          idle_timeout_ms_;
};

#endif
