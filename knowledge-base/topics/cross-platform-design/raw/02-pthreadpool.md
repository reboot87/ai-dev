# pthreadpool - Portable Thread Pool for C/C++

## 元数据
- **来源**: https://github.com/Maratyszcza/pthreadpool
- **获取时间**: 2026-09-09 17:55
- **作者**: Maratyszcza
- **类型**: 开源项目
- **重要性**: ⭐⭐⭐⭐

---

## Features

- C interface (C++-compatible)
- 1D-6D loops with step parameters
- Run on user-specified or auto-detected number of threads
- Work-stealing scheduling for efficient work balancing
- Wait-free synchronization of work items
- Compatible with Linux (including Android), macOS, iOS, Windows, Emscripten environments
- 100% unit tests coverage

## Example

```c
static void add_arrays(struct array_addition_context* context, size_t i) {
    context->sum[i] = context->augend[i] + context->addend[i];
}

int main() {
    pthreadpool_t threadpool = pthreadpool_create(0);
    const size_t threads_count = pthreadpool_get_threads_count(threadpool);
    struct array_addition_context context = { augend, addend, sum };
    pthreadpool_parallelize_1d(threadpool,
        (pthreadpool_task_1d_t) add_arrays,
        (void*) &context,
        ARRAY_SIZE,
        PTHREADPOOL_FLAG_DISABLE_DENORMALS);
    pthreadpool_destroy(threadpool);
}
```

## Key Design

- Work-stealing for load balancing
- Wait-free synchronization
- Platform abstraction layer for POSIX/Windows/Emscripten
