# Thread Pool Project - Experience Log

## 元数据
- **来源**: 本地项目经验
- **获取时间**: 2026-09-09 17:55
- **作者**: 项目开发过程
- **类型**: 实战经验
- **重要性**: ⭐⭐⭐⭐⭐

---

## 2026-09-09: Fix Use-After-Free in C++03 & Pure C Implementations

### Problem
Both C++03 and Pure C thread pool implementations crashed/hung when using TaskHandles.
The C++03 version produced no output at all (silent crash).
The Pure C version hung at `tp_destroy()`.

### Root Cause
**Use-after-free on TaskHandle pointers.** When `submit()` returned a `TaskHandle`
by value, the temporary was destroyed (refcount→0, impl freed), but `InternalTask`
still held a raw pointer to the freed memory. Workers crashed when trying to signal
`done` on the handle.

Key debug signal: Two workers printing the **same handle address** — classic
use-after-free indicator (freed memory reallocated).

### Solution
Added proper reference counting to handle the ownership:

**C++03:**
```cpp
// In submit(): InternalTask must hold a reference
t->handle = impl;
t->handle->addref();

// In worker_fn(): release after signaling done
task->handle->release();
std::free(task);

// In destroy(): release for queued tasks
t->handle->release();
std::free(t);
```

**Pure C:**
```c
// Added refcount field to tp_handle_t
typedef struct {
    ...
    volatile int refcount;
    ...
} tp_handle_t;

// submit() addrefs, worker releases, destroy releases
```

### Debugging Pattern Applied
1. `fflush(stdout)` — rule out output buffering
2. Worker entry/exit printf — confirm threads start
3. Pointer address comparison — detect use-after-free
4. Check destroy order — verify no dangling pointers

### Key Insight
> When a resource (Handle/pointer) is shared between the caller and an async
> worker, **both parties must participate in reference counting**. A raw pointer
> in the InternalTask is insufficient without a matching release.

### Environment
- Compiler: MSVC 2017 (Visual Studio 15 2017)
- Target: Windows XP+ (hand-rolled condition variables)
- Build: `cmake -G "Visual Studio 15 2017" -A x64`

### Files Modified
- `cpp03/thread_pool.hpp` — addref/release in submit/worker/destroy
- `c_threadpool/thread_pool.h` — added refcount field to tp_handle_t
- `c_threadpool/thread_pool.c` — addref/release + refcount-based destroy

---

## 2026-09-09: Fix Handle Leaks + C++17 if constexpr Cleanup

### Problem
1. Test 1 in all 4 implementations submitted tasks and discarded the returned handles,
   causing memory leaks (C++03/Pure C) or shared_ptr refcount leaks (C++11/C++17).
2. C++17 `Runner<T>` used template specialization to handle void/non-void returns,
   which could be simplified with `if constexpr`.

### Solution

**Handle leaks (all 4 implementations):**
- Capture handles in an array/vector, call `wait_all()`, then explicitly release/destroy
- C++03: `TaskHandle handles[3]; ... handles[i] = pool.submit(...); ... handles[i] = TaskHandle();`
- C++11/17: `std::vector<TaskHandle<void>> handles1; ... handles1.clear();`
- Pure C: `tp_handle_t* handles[3]; ... handles[i] = tp_submit(...); ... tp_handle_destroy(handles[i]);`

**C++17 `if constexpr` cleanup:**
```cpp
// Before: two template specializations (Runner<T> + Runner<void>)
// After: single template with if constexpr
void operator()() {
    try {
        if constexpr (!std::is_void_v<R>) {
            state->result = func();
        } else {
            func();
        }
    } catch (...) { ... }
}
```

### Files Modified
- `cpp03/main.cpp` — capture handles in Test 1
- `cpp11/main.cpp` — capture handles in Test 1
- `cpp17/main.cpp` — capture handles in Test 1
- `c_threadpool/main.c` — capture handles in Test 1
- `cpp17/thread_pool.hpp` — replaced Runner<T>/Runner<void> with if constexpr

---

## 2026-09-09: Production-Readiness Audit & Fixes

### Audit Findings (62 issues across 4 implementations)

| Severity | Count | Key Examples |
|----------|-------|-------------|
| CRITICAL | 3 | Null deref in C++03 submit, exception-safety in C++11/17 submit |
| HIGH | 10 | destroy() not draining tasks, missing RAII in constructors |
| MEDIUM | 22 | Volatile vs atomic, division by zero, handle leaks in benchmarks |
| LOW | 7 | Missing const, unnecessary atomics |

### Fixes Applied

**1. C++11/17 submit() exception safety (CRITICAL)**
- Problem: If user's task function throws, `done` is never set → `get()` hangs forever
- Fix: Wrapped `func()` in try-catch that sets `done=true` before re-throwing
```cpp
tasks_.push([state, f = std::move(func)]() mutable {
    try {
        f();
    } catch (...) {
        std::lock_guard<std::mutex> lk(state->mu);
        state->done = true;
        state->cv.notify_all();
        throw;
    }
    // ... set done
});
```

**2. C++03 submit()/submit_r() null-check (CRITICAL)**
- Problem: `calloc` can return NULL → null-pointer dereference
- Fix: Added `if (!t) { impl->release(); throw std::bad_alloc(); }` after calloc

**3. C++17 destroy() task draining (HIGH)**
- Problem: Remaining queued tasks are destroyed without setting `done` → callers hang
- Fix: After joining workers, drain remaining tasks by executing them
```cpp
void destroy() {
    { std::lock_guard<std::mutex> lock(mutex_); stop_ = true; }
    cv_.notify_all();
    for (auto& w : workers_)
        if (w.joinable()) w.join();
    /* drain remaining tasks */
    std::queue<std::function<void()>> remaining;
    { std::lock_guard<std::mutex> lock(mutex_); remaining.swap(tasks_); }
    while (!remaining.empty()) {
        try { remaining.front(); } catch (...) {}
        remaining.pop();
    }
}
```

**4. C main.c null-check (MEDIUM)**
- Problem: `tp_create` can return NULL → crash on dereference
- Fix: Added `if (!pool) { printf("ERROR"); return 1; }`

### Files Modified
- `cpp11/thread_pool.hpp` — exception safety in submit() + destroy() task draining
- `cpp17/thread_pool.hpp` — exception safety in submit() + destroy() task draining
- `cpp03/thread_pool.hpp` — null-check on calloc in submit()/submit_r()
- `c_threadpool/main.c` — null-check after tp_create
