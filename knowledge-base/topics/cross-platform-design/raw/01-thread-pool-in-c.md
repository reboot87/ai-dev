# Thread Pool in C

## 元数据
- **来源**: https://nachtimwald.com/2019/04/12/thread-pool-in-c
- **获取时间**: 2026-09-09 17:55
- **作者**: John's Blog
- **类型**: 教程
- **重要性**: ⭐⭐⭐⭐

---

## Key Design Decisions

1. **Fixed thread count**: Use number of cores/processors + 1 as default
2. **Linked list queue**: Allows infinite work items (can be changed to fixed-size)
3. **Two condition variables**: `work_cond` signals work available, `working_cond` signals all work done
4. **Detached threads**: Threads clean up on exit, no need to store thread IDs

## Data Structures

```c
struct tpool_work {
    thread_func_t      func;
    void              *arg;
    struct tpool_work *next;
};

struct tpool {
    tpool_work_t    *work_first;
    tpool_work_t    *work_last;
    pthread_mutex_t  work_mutex;
    pthread_cond_t   work_cond;
    pthread_cond_t   working_cond;
    size_t           working_cnt;
    size_t           thread_cnt;
    bool             stop;
};
```

## Worker Function Pattern

```c
static void *tpool_worker(void *arg) {
    while (1) {
        pthread_mutex_lock(&(tm->work_mutex));
        while (tm->work_first == NULL && !tm->stop)
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));
        if (tm->stop) break;
        work = tpool_work_get(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));
        if (work != NULL) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }
        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;
        if (!tm->stop && tm->working_cnt == 0 && tm->work_first == NULL)
            pthread_cond_signal(&(tm->working_cond));
        pthread_mutex_unlock(&(tm->work_mutex));
    }
    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));
    return NULL;
}
```

## Wait Pattern

```c
void tpool_wait(tpool_t *tm) {
    pthread_mutex_lock(&(tm->work_mutex));
    while (1) {
        if (tm->work_first != NULL || (!tm->stop && tm->working_cnt != 0) ||
            (tm->stop && tm->thread_cnt != 0)) {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}
```
