#include "thread_pool.h"
#include <stdlib.h>

/* ==================== Internal Task ==================== */

typedef struct tp_internal_task {
    tp_task_func_t    func;
    void*             arg;
    struct tp_internal_task* next;
    tp_handle_t*      handle;
    tp_result_func_t  result_func;
    void*             result_data;
} tp_internal_task_t;

typedef struct {
    thread_pool_t* pool;
    int index;
} worker_arg_t;

/* forward declaration */
static void* worker_fn(void* data);

/* ==================== Handle ==================== */

tp_handle_t* tp_handle_create(void) {
    tp_handle_t* h = (tp_handle_t*)calloc(1, sizeof(tp_handle_t));
    if (!h) return 0;
    tp_mutex_init(&h->mu);
    tp_cond_init(&h->cv);
    h->cancelled = 0;
    h->done = 0;
    h->refcount = 1;
    h->result = 0;
    return h;
}

static void tp_handle_addref(tp_handle_t* h) {
    if (!h) return;
    tp_mutex_lock(&h->mu);
    h->refcount++;
    tp_mutex_unlock(&h->mu);
}

void tp_handle_destroy(tp_handle_t* h) {
    int n;
    if (!h) return;
    tp_mutex_lock(&h->mu);
    n = --h->refcount;
    tp_mutex_unlock(&h->mu);
    if (n == 0) {
        tp_mutex_destroy(&h->mu);
        tp_cond_destroy(&h->cv);
        free(h);
    }
}

int tp_handle_cancel(tp_handle_t* h) {
    if (!h) return 0;
    tp_mutex_lock(&h->mu);
    if (h->done || h->cancelled) {
        tp_mutex_unlock(&h->mu);
        return 0;
    }
    h->cancelled = 1;
    tp_cond_broadcast(&h->cv);
    tp_mutex_unlock(&h->mu);
    return 1;
}

int tp_handle_is_cancelled(tp_handle_t* h) { return h ? h->cancelled : 0; }
int tp_handle_is_done(tp_handle_t* h)      { return h ? h->done : 0; }

int tp_handle_wait(tp_handle_t* h, int timeout_ms) {
    if (!h) return 1;
    tp_mutex_lock(&h->mu);
    if (timeout_ms < 0) {
        while (!h->done && !h->cancelled)
            tp_cond_wait(&h->cv, &h->mu);
    } else {
        while (!h->done && !h->cancelled) {
            int rc = tp_cond_wait_timeout(&h->cv, &h->mu, timeout_ms);
            if (rc == 0 && !h->done && !h->cancelled) {
                tp_mutex_unlock(&h->mu);
                return 0;
            }
        }
    }
    tp_mutex_unlock(&h->mu);
    return 1;
}

void* tp_handle_result(tp_handle_t* h) { return h ? h->result : 0; }

/* ==================== Worker ==================== */

static void try_scale_up(thread_pool_t* pool) {
    int queued = 0;
    tp_internal_task_t* t;
    for (t = (tp_internal_task_t*)pool->task_head; t; t = t->next) queued++;

    if (pool->busy_count >= pool->cur_threads &&
        queued > 0 && pool->cur_threads < pool->max_threads)
    {
        worker_arg_t* wa = (worker_arg_t*)malloc(sizeof(worker_arg_t));
        if (!wa) return;
        int idx = pool->cur_threads;
        wa->pool = pool;
        wa->index = idx;
        pool->active[idx] = 1;
        if (tp_thread_create(&pool->threads[idx], worker_fn, wa) != 0) {
            pool->active[idx] = 0;
            free(wa);
            return;
        }
        pool->cur_threads++;
    }
}

static void* worker_fn(void* data) {
    worker_arg_t* wa = (worker_arg_t*)data;
    thread_pool_t* pool = wa->pool;
    int my_idx = wa->index;
    free(wa);

    for (;;) {
        tp_internal_task_t* task;

        tp_mutex_lock(&pool->mutex);
        while (pool->task_head == 0 && !pool->stop) {
            int rc = tp_cond_wait_timeout(&pool->task_cond, &pool->mutex,
                                          pool->idle_timeout_ms);
            if (rc == 0 && pool->task_head == 0 && !pool->stop) {
                if (pool->cur_threads > pool->min_threads) {
                    pool->cur_threads--;
                    pool->active[my_idx] = 0;
                    tp_mutex_unlock(&pool->mutex);
                    return 0;
                }
            }
        }

        if (pool->stop && pool->task_head == 0) {
            pool->cur_threads--;
            pool->active[my_idx] = 0;
            tp_mutex_unlock(&pool->mutex);
            break;
        }

        task = (tp_internal_task_t*)pool->task_head;
        pool->task_head = task->next;
        if (!pool->task_head) pool->task_tail = 0;

        pool->busy_count++;
        tp_mutex_unlock(&pool->mutex);

        /* check cancel before execution */
        int cancelled = 0;
        tp_mutex_lock(&task->handle->mu);
        if (task->handle->cancelled) cancelled = 1;
        tp_mutex_unlock(&task->handle->mu);

        if (!cancelled) {
            task->func(task->arg);
            if (task->result_func)
                task->result_func(task->arg, task->result_data);
        }

        /* signal done */
        tp_mutex_lock(&task->handle->mu);
        task->handle->done = 1;
        tp_cond_broadcast(&task->handle->cv);
        tp_mutex_unlock(&task->handle->mu);

        tp_handle_destroy(task->handle);
        free(task);

        tp_mutex_lock(&pool->mutex);
        pool->busy_count--;
        if (pool->busy_count == 0 && pool->task_head == 0)
            tp_cond_broadcast(&pool->done_cond);
        tp_mutex_unlock(&pool->mutex);
    }
    return 0;
}

/* ==================== Pool ==================== */

thread_pool_t* tp_create(int min_threads, int max_threads, int idle_timeout_ms) {
    thread_pool_t* pool;
    int i;

    if (min_threads <= 0) min_threads = 2;
    if (max_threads < min_threads) max_threads = min_threads;
    if (idle_timeout_ms <= 0) idle_timeout_ms = 2000;

    pool = (thread_pool_t*)calloc(1, sizeof(thread_pool_t));
    if (!pool) return 0;

    pool->threads = (tp_thread_t*)malloc(sizeof(tp_thread_t) * max_threads);
    pool->active  = (int*)calloc(max_threads, sizeof(int));
    if (!pool->threads || !pool->active) {
        free(pool->threads); free(pool->active); free(pool);
        return 0;
    }

    pool->min_threads = min_threads;
    pool->max_threads = max_threads;
    pool->idle_timeout_ms = idle_timeout_ms;

    tp_mutex_init(&pool->mutex);
    tp_cond_init(&pool->task_cond);
    tp_cond_init(&pool->done_cond);

    for (i = 0; i < min_threads; i++) {
        worker_arg_t* wa = (worker_arg_t*)malloc(sizeof(worker_arg_t));
        wa->pool = pool;
        wa->index = i;
        pool->active[i] = 1;
        if (tp_thread_create(&pool->threads[i], worker_fn, wa) != 0) {
            pool->active[i] = 0; free(wa);
            tp_destroy(pool);
            return 0;
        }
        pool->cur_threads++;
    }
    return pool;
}

void tp_destroy(thread_pool_t* pool) {
    tp_internal_task_t* t;
    int i;
    if (!pool) return;

    tp_mutex_lock(&pool->mutex);
    pool->stop = 1;
    tp_cond_broadcast(&pool->task_cond);
    tp_mutex_unlock(&pool->mutex);

    for (i = 0; i < pool->max_threads; i++)
        if (pool->active[i]) tp_thread_join(pool->threads[i]);

    t = (tp_internal_task_t*)pool->task_head;
    while (t) {
        tp_internal_task_t* next = t->next;
        tp_mutex_lock(&t->handle->mu);
        t->handle->done = 1;
        tp_cond_broadcast(&t->handle->cv);
        tp_mutex_unlock(&t->handle->mu);
        tp_handle_destroy(t->handle);
        free(t);
        t = next;
    }

    tp_mutex_destroy(&pool->mutex);
    tp_cond_destroy(&pool->task_cond);
    tp_cond_destroy(&pool->done_cond);
    free(pool->threads);
    free(pool->active);
    free(pool);
}

tp_handle_t* tp_submit(thread_pool_t* pool, tp_task_func_t func, void* arg) {
    tp_handle_t* h;
    tp_internal_task_t* t;
    if (!pool || !func) return 0;

    h = tp_handle_create();
    if (!h) return 0;

    t = (tp_internal_task_t*)malloc(sizeof(tp_internal_task_t));
    if (!t) { tp_handle_destroy(h); return 0; }

    t->func = func; t->arg = arg; t->next = 0;
    t->handle = h; tp_handle_addref(h); t->result_func = 0; t->result_data = 0;

    tp_mutex_lock(&pool->mutex);
    if (pool->task_tail) ((tp_internal_task_t*)pool->task_tail)->next = t;
    else                 pool->task_head = t;
    pool->task_tail = t;
    try_scale_up(pool);
    tp_cond_signal(&pool->task_cond);
    tp_mutex_unlock(&pool->mutex);
    return h;
}

tp_handle_t* tp_submit_r(thread_pool_t* pool,
                          tp_task_func_t func, void* arg,
                          tp_result_func_t result_func, void* result_data)
{
    tp_handle_t* h;
    tp_internal_task_t* t;
    if (!pool || !func) return 0;

    h = tp_handle_create();
    if (!h) return 0;

    t = (tp_internal_task_t*)malloc(sizeof(tp_internal_task_t));
    if (!t) { tp_handle_destroy(h); return 0; }

    t->func = func; t->arg = arg; t->next = 0;
    t->handle = h; tp_handle_addref(h); t->result_func = result_func; t->result_data = result_data;

    tp_mutex_lock(&pool->mutex);
    if (pool->task_tail) ((tp_internal_task_t*)pool->task_tail)->next = t;
    else                 pool->task_head = t;
    pool->task_tail = t;
    try_scale_up(pool);
    tp_cond_signal(&pool->task_cond);
    tp_mutex_unlock(&pool->mutex);
    return h;
}

void tp_wait_all(thread_pool_t* pool) {
    if (!pool) return;
    tp_mutex_lock(&pool->mutex);
    while (pool->busy_count > 0 || pool->task_head != 0)
        tp_cond_wait(&pool->done_cond, &pool->mutex);
    tp_mutex_unlock(&pool->mutex);
}
