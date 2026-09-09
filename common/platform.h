#ifndef TP_PLATFORM_H
#define TP_PLATFORM_H

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef HANDLE tp_thread_t;

typedef struct {
    CRITICAL_SECTION cs;
} tp_mutex_t;

typedef struct {
    HANDLE  sem;
    CRITICAL_SECTION cs;
    volatile LONG waiters;
    volatile LONG gen;
} tp_cond_t;

/* ==================== Mutex ==================== */

static void tp_mutex_init(tp_mutex_t* m) {
    InitializeCriticalSection(&m->cs);
}

static void tp_mutex_lock(tp_mutex_t* m) {
    EnterCriticalSection(&m->cs);
}

static void tp_mutex_unlock(tp_mutex_t* m) {
    LeaveCriticalSection(&m->cs);
}

static void tp_mutex_destroy(tp_mutex_t* m) {
    DeleteCriticalSection(&m->cs);
}

/* ==================== Condition Variable ==================== */

static void tp_cond_init(tp_cond_t* c) {
    c->sem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
    InitializeCriticalSection(&c->cs);
    c->waiters = 0;
    c->gen = 0;
}

static void tp_cond_wait(tp_cond_t* c, tp_mutex_t* m) {
    LONG my_gen;

    EnterCriticalSection(&c->cs);
    c->waiters++;
    my_gen = c->gen;
    LeaveCriticalSection(&c->cs);

    LeaveCriticalSection(&m->cs);
    WaitForSingleObject(c->sem, INFINITE);

    EnterCriticalSection(&c->cs);
    if (c->gen == my_gen) {
        ReleaseSemaphore(c->sem, 1, NULL);
        LeaveCriticalSection(&c->cs);
    } else {
        c->waiters--;
        LeaveCriticalSection(&c->cs);
    }

    EnterCriticalSection(&m->cs);
}

/*
 * Timed wait. Returns 1 if signaled, 0 on timeout.
 * timeout_ms: milliseconds to wait.
 */
static int tp_cond_wait_timeout(tp_cond_t* c, tp_mutex_t* m, DWORD timeout_ms) {
    LONG my_gen;
    DWORD rc;

    EnterCriticalSection(&c->cs);
    c->waiters++;
    my_gen = c->gen;
    LeaveCriticalSection(&c->cs);

    LeaveCriticalSection(&m->cs);
    rc = WaitForSingleObject(c->sem, timeout_ms);

    EnterCriticalSection(&c->cs);
    if (rc == WAIT_OBJECT_0) {
        if (c->gen == my_gen) {
            ReleaseSemaphore(c->sem, 1, NULL);
            LeaveCriticalSection(&c->cs);
            EnterCriticalSection(&m->cs);
            return 0; /* spurious, treat as timeout */
        } else {
            c->waiters--;
            LeaveCriticalSection(&c->cs);
            EnterCriticalSection(&m->cs);
            return 1; /* properly signaled */
        }
    } else {
        /* timeout: thread still counted as waiter, remove it */
        c->waiters--;
        LeaveCriticalSection(&c->cs);
        EnterCriticalSection(&m->cs);
        return 0;
    }
}

static void tp_cond_signal(tp_cond_t* c) {
    EnterCriticalSection(&c->cs);
    if (c->waiters > 0) {
        c->gen++;
        ReleaseSemaphore(c->sem, 1, NULL);
    }
    LeaveCriticalSection(&c->cs);
}

static void tp_cond_broadcast(tp_cond_t* c) {
    LONG w;
    EnterCriticalSection(&c->cs);
    w = c->waiters;
    if (w > 0) {
        c->gen++;
        ReleaseSemaphore(c->sem, w, NULL);
    }
    LeaveCriticalSection(&c->cs);
}

static void tp_cond_destroy(tp_cond_t* c) {
    if (c->sem) CloseHandle(c->sem);
    DeleteCriticalSection(&c->cs);
}

/* ==================== Thread ==================== */

static int tp_thread_create(tp_thread_t* t, void* (*func)(void*), void* arg) {
    *t = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    return *t != NULL ? 0 : -1;
}

static void tp_thread_join(tp_thread_t t) {
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

static void tp_sleep_ms(DWORD ms) {
    Sleep(ms);
}

#else
/* ==================== POSIX ==================== */

#include <pthread.h>
#include <unistd.h>
#include <time.h>

typedef pthread_t tp_thread_t;
typedef pthread_mutex_t tp_mutex_t;

typedef struct {
    pthread_cond_t cond;
} tp_cond_t;

static void tp_mutex_init(tp_mutex_t* m)       { pthread_mutex_init(m, NULL); }
static void tp_mutex_lock(tp_mutex_t* m)       { pthread_mutex_lock(m); }
static void tp_mutex_unlock(tp_mutex_t* m)     { pthread_mutex_unlock(m); }
static void tp_mutex_destroy(tp_mutex_t* m)    { pthread_mutex_destroy(m); }

static void tp_cond_init(tp_cond_t* c)         { pthread_cond_init(&c->cond, NULL); }
static void tp_cond_destroy(tp_cond_t* c)      { pthread_cond_destroy(&c->cond); }

static void tp_cond_wait(tp_cond_t* c, tp_mutex_t* m) {
    pthread_cond_wait(&c->cond, m);
}

static int tp_cond_wait_timeout(tp_cond_t* c, tp_mutex_t* m, unsigned int timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_cond_timedwait(&c->cond, m, &ts) == 0 ? 1 : 0;
}

static void tp_cond_signal(tp_cond_t* c)       { pthread_cond_signal(&c->cond); }
static void tp_cond_broadcast(tp_cond_t* c)    { pthread_cond_broadcast(&c->cond); }

static int tp_thread_create(tp_thread_t* t, void* (*func)(void*), void* arg) {
    return pthread_create(t, NULL, func, arg);
}

static void tp_thread_join(tp_thread_t t) {
    pthread_join(t, NULL);
}

static void tp_sleep_ms(unsigned int ms) {
    usleep(ms * 1000);
}

#endif

#endif
