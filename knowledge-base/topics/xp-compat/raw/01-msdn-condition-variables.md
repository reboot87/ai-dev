# Condition Variables - Win32 apps

## 元数据
- **来源**: https://learn.microsoft.com/en-us/windows/win32/sync/condition-variables
- **获取时间**: 2026-09-09 17:55
- **作者**: Microsoft
- **类型**: 官方文档
- **重要性**: ⭐⭐⭐⭐⭐

---

## Key Points

Condition variables are synchronization primitives that enable threads to wait until a particular condition occurs. Condition variables are user-mode objects that cannot be shared across processes.

Condition variables enable threads to atomically release a lock and enter the sleeping state. They can be used with critical sections or slim reader/writer (SRW) locks.

**Windows Server 2003 and Windows XP: Condition variables are not supported.**

## Functions

| Function | Description |
|----------|-------------|
| InitializeConditionVariable | Initializes a condition variable. |
| SleepConditionVariableCS | Sleeps on the specified condition variable and releases the specified critical section as an atomic operation. |
| SleepConditionVariableSRW | Sleeps on the specified condition variable and releases the specified SRW lock as an atomic operation. |
| WakeAllConditionVariable | Wakes all threads waiting on the specified condition variable. |
| WakeConditionVariable | Wakes a single thread waiting on the specified condition variable. |

## Usage Pattern

```c
CRITICAL_SECTION CritSection;
CONDITION_VARIABLE ConditionVar;

void PerformOperationOnSharedData()
{
    EnterCriticalSection(&CritSection);
    while (TestPredicate() == FALSE)
    {
        SleepConditionVariableCS(&ConditionVar, &CritSection, INFINITE);
    }
    ChangeSharedData();
    LeaveCriticalSection(&CritSection);
}
```

## Important Notes

- Condition variables are subject to spurious wakeups and stolen wakeups.
- You should recheck a predicate (typically in a while loop) after a sleep operation returns.
- It is usually better to release the lock before waking other threads to reduce context switches.
