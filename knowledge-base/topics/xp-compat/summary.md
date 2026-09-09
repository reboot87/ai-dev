# Windows XP 兼容同步原语 - 知识总结

## 概述
Windows XP 不支持 Vista 引入的 Condition Variables 和 SRWLOCK，需要使用手写方案（semaphore + CRITICAL_SECTION + generation counter）来实现条件变量功能。

## 核心知识点

### ⭐⭐⭐⭐⭐ 核心知识

1. **XP 不支持的 API**
   - 描述: CONDITION_VARIABLE, SRWLOCK, InitializeConditionVariable 等
   - 重要性: 直接使用会导致链接失败或运行时崩溃
   - 应用: 必须使用手写实现或降级方案

2. **手写条件变量方案**
   - 描述: Semaphore + CRITICAL_SECTION + generation counter
   - 重要性: XP兼容的唯一可靠方案
   - 应用: platform.h 中的 tp_cond_* 系列函数

3. **Generation Counter 模式**
   - 描述: 使用计数器检测虚假唤醒和信号丢失
   - 重要性: 保证条件变量的正确性
   - 应用: 每次wait记录当前generation，signal时递增

### ⭐⭐⭐⭐ 重要知识

1. **Semaphore 信号量**
   - 描述: Win32 API，XP兼容
   - 最佳实践: CreateSemaphore + WaitForSingleObject + ReleaseSemaphore
   - 应用: 作为条件变量的底层原语

2. **CRITICAL_SECTION 临界区**
   - 描述: 用户模式锁，比Mutex快
   - 最佳实践: InitializeCriticalSection + EnterCriticalSection + LeaveCriticalSection
   - 应用: 保护共享数据

3. **Spurious Wakeup 处理**
   - 描述: 条件变量可能虚假唤醒
   - 最佳实践: 使用while循环检查谓词
   - 应用: while(!condition) wait(cv, mutex);

### ⭐⭐⭐ 有用知识

1. **Vista+ API 降级策略**
   - 描述: 运行时检测OS版本，选择最优实现
   - 使用场景: 需要同时支持XP和现代Windows
   - 优势: 现代系统上获得更好性能

## 快速参考

| API | XP支持 | 替代方案 |
|-----|--------|----------|
| CONDITION_VARIABLE | ❌ | Semaphore + generation counter |
| SRWLOCK | ❌ | CRITICAL_SECTION |
| InitializeConditionVariable | ❌ | 手写 tp_cond_init |
| SleepConditionVariableCS | ❌ | 手写 tp_cond_wait |

## 常见问题

### Q: 为什么不能直接用Semaphore模拟条件变量？
**A**: Semaphore没有原子性地释放锁并等待的能力。手写方案需要generation counter来正确处理信号和虚假唤醒。

### Q: 手写条件变量的性能如何？
**A**: 比Vista+的原生实现慢约2-3倍，但在XP上是唯一选择。对于大多数应用足够。

## 相关资源
- [MSDN Condition Variables](https://learn.microsoft.com/en-us/windows/win32/sync/condition-variables)
- [MSDN Synchronization Primitives New to Vista](https://learn.microsoft.com/en-us/archive/msdn-magazine/2007/june/concurrency-synchronization-primitives-new-to-windows-vista)

## 更新日志
- 2026-09-09: 初始创建
