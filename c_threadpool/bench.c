#include "thread_pool.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
static double now_ms(void) {
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / freq.QuadPart * 1000.0;
}
#else
#include <time.h>
static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
#endif

static volatile int g_sink = 0;

typedef struct { int id; volatile int done; } bench_arg_t;

static void empty_task(void* arg) { (void)arg; }

static void cpu_task(void* arg) {
    bench_arg_t* a = (bench_arg_t*)arg;
    volatile int x = 0;
    for (int i = 0; i < 5000; i++) x += i;
    g_sink += x;
    a->done = 1;
}

static void print_result(const char* name, double ms, int n) {
    printf("  %-30s %8.1f ms   %7d tasks   %10.0f tasks/s\n",
           name, ms, n, n / (ms / 1000.0));
}

int main(void) {
    printf("========================================\n");
    printf("  Pure C Thread Pool Benchmark\n");
    printf("========================================\n\n");

    /* Throughput: empty tasks */
    {
        int N = 50000;
        bench_arg_t* args = (bench_arg_t*)calloc(N, sizeof(bench_arg_t));
        thread_pool_t* pool = tp_create(4, 4, 5000);

        double t0 = now_ms();
        for (int i = 0; i < N; i++) tp_submit(pool, empty_task, &args[i]);
        tp_wait_all(pool);
        print_result("throughput T=4, N=50k", now_ms() - t0, N);

        tp_destroy(pool);
        free(args);
    }

    /* CPU-bound scaling */
    printf("\n--- CPU-bound scaling: N=2000 ---\n");
    {
        int N = 2000;
        int counts[] = {1, 2, 4, 6};
        for (int c = 0; c < 4; c++) {
            int T = counts[c];
            bench_arg_t* args = (bench_arg_t*)calloc(N, sizeof(bench_arg_t));
            thread_pool_t* pool = tp_create(T, T, 5000);

            double t0 = now_ms();
            for (int i = 0; i < N; i++) tp_submit(pool, cpu_task, &args[i]);
            tp_wait_all(pool);
            char label[32];
            sprintf(label, "cpu_bound T=%d", T);
            print_result(label, now_ms() - t0, N);

            tp_destroy(pool);
            free(args);
        }
    }

    /* Throughput scaling */
    printf("\n--- Throughput scaling: N=50000 ---\n");
    {
        int N = 50000;
        int counts[] = {1, 2, 4, 6};
        for (int c = 0; c < 4; c++) {
            int T = counts[c];
            bench_arg_t* args = (bench_arg_t*)calloc(N, sizeof(bench_arg_t));
            thread_pool_t* pool = tp_create(T, T, 5000);

            double t0 = now_ms();
            for (int i = 0; i < N; i++) tp_submit(pool, empty_task, &args[i]);
            tp_wait_all(pool);
            char label[32];
            sprintf(label, "throughput T=%d", T);
            print_result(label, now_ms() - t0, N);

            tp_destroy(pool);
            free(args);
        }
    }

    printf("\n========================================\n");
    return 0;
}
