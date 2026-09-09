#ifndef THREAD_POOL_C_H
#define THREAD_POOL_C_H

#include "../common/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== TaskHandle ==================== */

typedef struct {
    tp_mutex_t mu;
    tp_cond_t  cv;
    volatile int cancelled;
    volatile int done;
    volatile int refcount;
    void*    result;
} tp_handle_t;

typedef void (*tp_task_func_t)(void* arg);
typedef void (*tp_result_func_t)(void* result, void* user_data);

tp_handle_t* tp_handle_create(void);
void         tp_handle_destroy(tp_handle_t* h);
int          tp_handle_cancel(tp_handle_t* h);
int          tp_handle_is_cancelled(tp_handle_t* h);
int          tp_handle_is_done(tp_handle_t* h);
int          tp_handle_wait(tp_handle_t* h, int timeout_ms);
void*        tp_handle_result(tp_handle_t* h);

/* ==================== ThreadPool ==================== */

typedef struct {
    tp_thread_t* threads;
    int*         active;
    int          min_threads;
    int          max_threads;
    int          cur_threads;
    int          busy_count;

    void*        task_head;
    void*        task_tail;

    tp_mutex_t   mutex;
    tp_cond_t    task_cond;
    tp_cond_t    done_cond;

    volatile int stop;
    int          idle_timeout_ms;
} thread_pool_t;

thread_pool_t* tp_create(int min_threads, int max_threads, int idle_timeout_ms);
void           tp_destroy(thread_pool_t* pool);

/* submit: returns handle for cancel/wait/result */
tp_handle_t*   tp_submit(thread_pool_t* pool,
                          tp_task_func_t func, void* arg);

/* submit with result callback */
tp_handle_t*   tp_submit_r(thread_pool_t* pool,
                            tp_task_func_t func, void* arg,
                            tp_result_func_t result_func, void* result_data);

void           tp_wait_all(thread_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif
