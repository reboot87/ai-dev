#include "thread_pool.hpp"
#include <iostream>
#include <cstring>
#include <atomic>
#include <future>
#include <vector>

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

static std::atomic<int> g_sink(0);

static void print_result(const char* name, double ms, int n) {
    printf("  %-30s %8.1f ms   %7d tasks   %10.0f tasks/s\n",
           name, ms, n, n / (ms / 1000.0));
}

int main() {
    printf("========================================\n");
    printf("  C++17 Thread Pool Benchmark\n");
    printf("========================================\n\n");

    {
        int N = 50000;
        ThreadPool pool(4, 4, 5000);
        double t0 = now_ms();
        for (int i = 0; i < N; i++) pool.submit([] {});
        pool.wait_all();
        print_result("throughput T=4, N=50k", now_ms() - t0, N);
    }

    printf("\n--- CPU-bound scaling: N=2000 ---\n");
    {
        int N = 2000;
        int counts[] = {1, 2, 4, 6};
        for (int c = 0; c < 4; c++) {
            int T = counts[c];
            ThreadPool pool(T, T, 5000);
            double t0 = now_ms();
            for (int i = 0; i < N; i++) {
                pool.submit([] {
                    volatile int x = 0;
                    for (int j = 0; j < 5000; j++) x += j;
                    g_sink += x;
                });
            }
            pool.wait_all();
            char label[32];
            sprintf(label, "cpu_bound T=%d", T);
            print_result(label, now_ms() - t0, N);
        }
    }

    printf("\n--- Throughput scaling: N=50000 ---\n");
    {
        int N = 50000;
        int counts[] = {1, 2, 4, 6};
        for (int c = 0; c < 4; c++) {
            int T = counts[c];
            ThreadPool pool(T, T, 5000);
            double t0 = now_ms();
            for (int i = 0; i < N; i++) pool.submit([] {});
            pool.wait_all();
            char label[32];
            sprintf(label, "throughput T=%d", T);
            print_result(label, now_ms() - t0, N);
        }
    }

    printf("\n--- Future overhead: T=4, N=10000 ---\n");
    {
        int N = 10000;
        ThreadPool pool(4, 4, 5000);
        std::vector<TaskHandle<int>> handles;
        handles.reserve(N);

        double t0 = now_ms();
        for (int i = 0; i < N; i++) {
            handles.push_back(pool.submit_r([i]() -> int { return i * i; }));
        }
        for (auto& h : handles) h.get();
        print_result("future get", now_ms() - t0, N);
    }

    printf("\n========================================\n");
    return 0;
}
