# Thread Pool Design - 知识总结

## 概述
线程池是管理并发任务的核心设计模式，通过复用固定数量的线程来执行异步任务，避免频繁创建/销毁线程的开销。

## 核心知识点

### ⭐⭐⭐⭐⭐ 核心知识

1. **线程池基本架构**
   - 描述: 预创建固定数量线程，从任务队列中取任务执行
   - 重要性: 所有线程池实现的基础
   - 应用: 任务队列 + 工作线程 + 同步原语

2. **任务队列管理**
   - 描述: 使用链表或循环数组存储待执行任务
   - 重要性: 决定线程池的吞吐量和内存使用
   - 应用: FIFO队列、优先级队列、工作窃取

3. **同步原语选择**
   - 描述: mutex + condition_variable 是标准组合
   - 重要性: 保证线程安全和正确唤醒
   - 应用: POSIX threads / Win32 CRITICAL_SECTION / C++ std::mutex

4. **优雅关闭 (Graceful Shutdown)**
   - 描述: 等待所有任务完成后再销毁线程池
   - 重要性: 避免资源泄漏和未定义行为
   - 应用: stop标志 + 广播 + join所有线程

### ⭐⭐⭐⭐ 重要知识

1. **自动扩缩容**
   - 描述: 根据负载动态调整线程数量
   - 最佳实践: min/max线程数限制，空闲超时回收
   - 应用: 避免线程过多导致上下文切换开销

2. **任务返回值处理**
   - 描述: 使用future/promise或回调获取任务结果
   - 最佳实践: shared_ptr<State> + condition_variable通知
   - 应用: TaskHandle<T>模式

3. **异常安全**
   - 描述: 任务抛出异常时必须设置done标志
   - 最佳实践: try-catch包装任务函数，确保done=true
   - 应用: 避免get()永久阻塞

### ⭐⭐⭐ 有用知识

1. **工作窃取 (Work Stealing)**
   - 描述: 空闲线程从忙碌线程的队列中偷取任务
   - 使用场景: 高并发场景，负载不均衡时
   - 优势: 比单一队列减少锁竞争

2. **任务依赖管理**
   - 描述: 支持DAG（有向无环图）任务执行
   - 使用场景: 科学计算、数据处理流水线
   - 优势: 自动调度依赖任务

## 快速参考

| 概念 | 说明 | 重要性 |
|------|------|--------|
| 线程数 | cores + 1 是经验值 | ⭐⭐⭐⭐⭐ |
| 任务粒度 | 短小独立任务最佳 | ⭐⭐⭐⭐⭐ |
| 关闭策略 | 等待完成 vs 丢弃待执行 | ⭐⭐⭐⭐ |
| 结果获取 | future vs 回调 | ⭐⭐⭐⭐ |
| 异常处理 | 必须catch并设置done | ⭐⭐⭐⭐⭐ |

## 常见问题

### Q: 线程池应该创建多少个线程？
**A**: 对于CPU密集型任务，使用 `std::thread::hardware_concurrency()` 或 cores + 1。对于IO密集型任务，可以创建更多线程（如 cores * 2）。

### Q: 如何处理任务抛出的异常？
**A**: 必须在worker中catch异常，并确保设置任务的done标志，否则等待结果的线程会永久阻塞。

### Q: 线程池关闭时如何处理未执行的任务？
**A**: 有两种策略：1) 执行完所有任务再关闭；2) 丢弃未执行的任务。选择取决于业务需求。

## 相关资源
- [Microsoft Thread Pool Best Practices](https://learn.microsoft.com/en-us/windows/apps/develop/threading/best-practices-for-using-the-thread-pool)
- [pthreadpool - Portable C/C++ Thread Pool](https://github.com/Maratyszcza/pthreadpool)
- [Thread Pool in C](https://nachtimwald.com/2019/04/12/thread-pool-in-c)

## 更新日志
- 2026-09-09: 初始创建
