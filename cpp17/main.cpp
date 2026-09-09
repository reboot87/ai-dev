#include "thread_pool.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

int main() {
    std::cout << "=== C++17 - Handle / Cancel / Timeout / Return Value ===" << std::endl << std::endl;

    ThreadPool pool(2, 4, 5000);

    /* Test 1: void submit + wait */
    std::cout << "[Test 1] Submit 3 tasks, wait_all" << std::endl;
    std::vector<TaskHandle<void>> handles1;
    for (int i = 0; i < 3; i++) {
        handles1.push_back(pool.submit([i] {
            std::cout << "  task " << i << ": running (1s)" << std::endl;
            sleep_ms(1000);
            std::cout << "  task " << i << ": done" << std::endl;
        }));
    }
    pool.wait_all();
    handles1.clear();
    std::cout << "  All done." << std::endl << std::endl;

    /* Test 2: Cancel queued task */
    std::cout << "[Test 2] Cancel queued task" << std::endl;
    {
        auto h = pool.submit([] {
            std::cout << "  [should not run]" << std::endl;
            sleep_ms(2000);
        });
        bool ok = h.cancel();
        std::cout << "  cancel() returned " << ok << std::endl;
        h.wait_for(std::chrono::seconds(5));
        std::cout << "  is_cancelled=" << h.is_cancelled()
                  << " is_done=" << h.is_done() << std::endl;
    }
    std::cout << std::endl;

    /* Test 3: Cancel running task */
    std::cout << "[Test 3] Cancel running task" << std::endl;
    {
        auto h = pool.submit([] {
            std::cout << "  [running...]" << std::endl;
            sleep_ms(2000);
            std::cout << "  [finished anyway]" << std::endl;
        });
        sleep_ms(100);
        bool ok = h.cancel();
        std::cout << "  cancel() returned " << ok << std::endl;
        h.wait_for(std::chrono::seconds(5));
        std::cout << "  is_done=" << h.is_done() << std::endl;
    }
    std::cout << std::endl;

    /* Test 4: Timeout */
    std::cout << "[Test 4] Submit 2s task, wait_for 300ms" << std::endl;
    {
        auto h = pool.submit([] { sleep_ms(2000); });
        bool completed = h.wait_for(std::chrono::milliseconds(300));
        std::cout << "  wait_for(300ms) completed=" << completed << std::endl;
        h.wait_for(std::chrono::seconds(5));
        std::cout << "  finally done." << std::endl;
    }
    std::cout << std::endl;

    /* Test 5: Return value */
    std::cout << "[Test 5] submit_r with return value" << std::endl;
    {
        auto h1 = pool.submit_r([](int a, int b) -> int {
            sleep_ms(100);
            return a + b;
        }, 10, 20);

        auto h2 = pool.submit_r([](int x) -> std::string {
            return "result=" + std::to_string(x * x);
        }, 7);

        auto h3 = pool.submit_r([]() -> double {
            return 3.14159;
        });

        int sum = h1.get();
        std::string s = h2.get();
        double pi = h3.get();
        std::cout << "  10 + 20 = " << sum << std::endl;
        std::cout << "  7^2: " << s << std::endl;
        std::cout << "  pi = " << pi << std::endl;
    }
    std::cout << std::endl;

    /* Test 6: Cancel submit_r */
    std::cout << "[Test 6] Cancel submit_r before execution" << std::endl;
    {
        auto block1 = pool.submit([] { sleep_ms(500); });
        auto block2 = pool.submit([] { sleep_ms(500); });
        sleep_ms(10);

        auto h = pool.submit_r([]() -> int { return 42; });
        bool cancelled = h.cancel();
        std::cout << "  cancel() returned " << cancelled << std::endl;

        try {
            int val = h.get();
            std::cout << "  got value: " << val << std::endl;
        } catch (const TaskCancelled&) {
            std::cout << "  get() threw TaskCancelled" << std::endl;
        }

        block1.wait_for(std::chrono::seconds(3));
        block2.wait_for(std::chrono::seconds(3));
    }

    /* Test 7: Multiple return values */
    std::cout << std::endl << "[Test 7] Multiple parallel tasks with results" << std::endl;
    {
        std::vector<TaskHandle<int>> handles;
        for (int i = 0; i < 8; i++) {
            handles.push_back(pool.submit_r([i]() -> int {
                sleep_ms(100);
                return i * i;
            }));
        }
        for (size_t i = 0; i < handles.size(); i++) {
            std::cout << "  " << i << "^2 = " << handles[i].get() << std::endl;
        }
    }

    std::cout << std::endl << "=== Done ===" << std::endl;
    return 0;
}
