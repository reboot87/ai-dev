# 注意事项 - 快速参考

## 常见陷阱

### ❌ Use-After-Free
- **症状**: 程序崩溃或无输出
- **原因**: 临时对象析构后指针仍被使用
- **解决**: 引用计数或shared_ptr

### ❌ 未处理的异常
- **症状**: get()永久阻塞
- **原因**: 任务抛出异常但done标志未设置
- **解决**: try-catch包装任务函数

### ❌ destroy() 未处理剩余任务
- **症状**: 等待者永久阻塞
- **原因**: 队列中的任务被销毁但done未设置
- **解决**: 执行剩余任务或通知等待者

### ❌ 空指针未检查
- **症状**: 访问违规
- **原因**: 内存分配失败返回NULL
- **解决**: 每次分配后检查返回值

### ❌ 虚假唤醒未处理
- **症状**: 条件变量提前唤醒
- **原因**: 操作系统可能虚假唤醒线程
- **解决**: 使用while循环检查谓词

## 平台差异

| 问题 | POSIX | Win32 XP | Win32 Vista+ |
|------|-------|----------|--------------|
| 条件变量 | pthread_cond | 手写实现 | CONDITION_VARIABLE |
| 读写锁 | pthread_rwlock | 手写实现 | SRWLOCK |
| 线程局部存储 | __thread | __declspec(thread) | TlsAlloc |
| 原子操作 | __atomic_* | Interlocked* | std::atomic |

## 调试技巧

1. **fflush(stdout)** — 排除输出缓冲问题
2. **Worker入口/出口printf** — 确认线程是否启动
3. **指针地址比较** — 检测use-after-free
4. **检查destroy顺序** — 验证无悬空指针
5. **Valgrind/AddressSanitizer** — 检测内存问题
