#include "thread_pool.hpp"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

static void slow_task(void* arg) {
    int id = *(int*)arg;
    std::printf("  task %d: running (2s)\n", id);
    sleep_ms(2000);
    std::printf("  task %d: done\n", id);
}

int main() {
    std::printf("=== C++03 - Handle / Cancel / Timeout Demo ===\n\n");
    std::printf("creating pool...\n");

    ThreadPool pool(2, 4, 5000);
    std::printf("pool created\n");

    /* Test 1: Basic submit + wait */
    std::printf("[Test 1] Submit 3 tasks, wait_all\n");
    int ids[] = {1, 2, 3};
    TaskHandle handles[3];
    for (int i = 0; i < 3; i++) {
        std::printf("  submitting task %d...\n", i);
        handles[i] = pool.submit(slow_task, &ids[i]);
    }
    std::printf("  waiting...\n");
    pool.wait_all();
    for (int i = 0; i < 3; i++) handles[i] = TaskHandle();
    std::printf("  All done.\n\n");

    /* Test 2: Cancel */
    std::printf("[Test 2] Submit task, try cancel\n");
    {
        int id = 10;
        TaskHandle h = pool.submit(slow_task, &id);
        sleep_ms(50);
        if (h.cancel())
            std::printf("  Cancelled!\n");
        else
            std::printf("  Could not cancel (already running)\n");
        h.wait(-1);
        std::printf("  is_cancelled=%d, is_done=%d\n", h.is_cancelled(), h.is_done());
    }
    std::printf("\n");

    /* Test 3: Timeout */
    std::printf("[Test 3] Submit 2s task, wait 200ms\n");
    {
        int id = 20;
        TaskHandle h = pool.submit(slow_task, &id);
        int ok = h.wait(200);
        std::printf("  wait(200ms) returned %d (0=timeout)\n", ok);
        h.wait(-1);
    }
    std::printf("\n");

    /* Test 4: Result callback */
    std::printf("[Test 4] Submit with result callback\n");
    {
        for (int i = 0; i < 3; i++) {
            TaskHandle h = pool.submit(slow_task, &i);
            h.wait(-1);
        }
        std::printf("  All completed.\n");
    }

    std::printf("\n=== Done ===\n");
    return 0;
}
