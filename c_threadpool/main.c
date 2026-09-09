#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

static void slow_task(void* arg) {
    int id = *(int*)arg;
    printf("  task %d: running (2s)\n", id); fflush(stdout);
    sleep_ms(2000);
    printf("  task %d: done\n", id); fflush(stdout);
}

typedef struct { int id; int result; } compute_arg_t;

static void compute_task(void* arg) {
    compute_arg_t* a = (compute_arg_t*)arg;
    int sum = 0;
    for (int i = 0; i <= a->id * 100; i++) sum += i;
    a->result = sum;
    printf("  task %d: computed %d\n", a->id, sum);
}

int main(void) {
    printf("=== Pure C - Handle / Cancel / Timeout Demo ===\n\n");
    fflush(stdout);

    printf("creating pool...\n"); fflush(stdout);
    thread_pool_t* pool = tp_create(2, 4, 5000);
    if (!pool) { printf("ERROR: tp_create failed\n"); return 1; }
    printf("pool created, cur_threads=%d\n", pool->cur_threads); fflush(stdout);

    /* Test 1: Basic submit + wait */
    printf("[Test 1] Submit 3 tasks, wait_all\n"); fflush(stdout);
    int ids[] = {1, 2, 3};
    tp_handle_t* handles[3];
    for (int i = 0; i < 3; i++) {
        printf("  submitting task %d...\n", i); fflush(stdout);
        handles[i] = tp_submit(pool, slow_task, &ids[i]);
    }
    printf("  waiting...\n"); fflush(stdout);
    tp_wait_all(pool);
    for (int i = 0; i < 3; i++) tp_handle_destroy(handles[i]);
    printf("  All done.\n\n"); fflush(stdout);

    /* Test 2: Cancel a task */
    printf("[Test 2] Submit task, try cancel before it starts\n");
    {
        int id = 10;
        tp_handle_t* h = tp_submit(pool, slow_task, &id);
        sleep_ms(50); /* give worker time to pick it up */
        if (tp_handle_cancel(h))
            printf("  Cancelled!\n");
        else
            printf("  Could not cancel (already running)\n");
        tp_handle_wait(h, -1);
        printf("  is_cancelled=%d, is_done=%d\n",
               tp_handle_is_cancelled(h), tp_handle_is_done(h));
        tp_handle_destroy(h);
    }
    printf("\n");

    /* Test 3: Timeout */
    printf("[Test 3] Submit 2s task, wait 200ms (should timeout)\n");
    {
        int id = 20;
        tp_handle_t* h = tp_submit(pool, slow_task, &id);
        int ok = tp_handle_wait(h, 200);
        printf("  wait_for(200ms) returned %d (0=timeout)\n", ok);
        tp_handle_wait(h, -1); /* wait for real */
        tp_handle_destroy(h);
    }
    printf("\n");

    /* Test 4: Result callback */
    printf("[Test 4] Submit with result callback\n");
    {
        compute_arg_t args[] = {{5, 0}, {10, 0}, {15, 0}};
        tp_handle_t* handles[3];
        for (int i = 0; i < 3; i++)
            handles[i] = tp_submit_r(pool, compute_task, &args[i], 0, 0);
        for (int i = 0; i < 3; i++) {
            tp_handle_wait(handles[i], -1);
            printf("  result[%d] = %d\n", i, args[i].result);
            tp_handle_destroy(handles[i]);
        }
    }

    tp_destroy(pool);
    printf("\n=== Done ===\n");
    return 0;
}
