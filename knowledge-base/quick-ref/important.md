# 重要知识 - 快速参考

## ⭐⭐⭐⭐⭐ 核心知识

### 线程池设计
- 线程数: `cores + 1` 是经验值
- 任务粒度: 短小独立任务最佳
- 异常安全: 必须catch异常并设置done标志
- 优雅关闭: 等待所有任务完成后再销毁

### XP兼容
- XP不支持: CONDITION_VARIABLE, SRWLOCK
- 替代方案: Semaphore + CRITICAL_SECTION + generation counter
- 关键函数: tp_cond_init, tp_cond_wait, tp_cond_signal

### 跨平台
- POSIX: pthread_create, pthread_mutex, pthread_cond
- Win32 XP: CreateThread, CRITICAL_SECTION, Semaphore
- C++11: std::thread, std::mutex, std::condition_variable

## ⭐⭐⭐⭐ 重要知识

### 引用计数
- 共享资源必须参与引用计数
- C++03: addref()/release() 手动管理
- C++11+: std::shared_ptr 自动管理
- Pure C: volatile int refcount + 手动addref/release

### 异常处理
- C++11/17: try-catch包装任务函数
- 设置done=true后re-throw
- 避免get()永久阻塞

### destroy() 设计
- 必须处理剩余队列任务
- 执行任务并设置done标志
- 或者丢弃任务但通知等待者

## ⭐⭐⭐ 有用知识

### Work Stealing
- 每个线程有自己的本地队列
- 空闲线程从忙碌线程偷取任务
- 减少锁竞争，提高缓存局部性

### 任务返回值
- C++03: 回调函数
- C++11+: std::future + std::promise
- Pure C: 回调函数 + user_data

### 性能优化
- 避免频繁创建/销毁线程
- 使用原子操作代替锁（简单场景）
- 缓存友好的数据结构
