# 跨平台线程池设计 - 知识总结

## 概述
跨平台线程池需要抽象底层同步原语，在POSIX和Win32 API之间提供统一接口。

## 核心知识点

### ⭐⭐⭐⭐⭐ 核心知识

1. **平台抽象层设计**
   - 描述: 统一的mutex/condvar/thread接口
   - 重要性: 一套代码支持多平台
   - 应用: platform.h 头文件封装

2. **POSIX vs Win32 映射**
   - 描述: pthread_* → tp_thread_* / CRITICAL_SECTION → tp_mutex_t
   - 重要性: 正确映射确保功能一致
   - 应用: 编译时条件选择

3. **C接口优先**
   - 描述: 使用C接口保证最大兼容性
   - 重要性: C++03/11/17和Pure C都能使用
   - 应用: extern "C" 包装

### ⭐⭐⭐⭐ 重要知识

1. **Work-stealing 调度**
   - 描述: 每个线程有自己的本地队列，空闲时从其他线程偷取任务
   - 最佳实践: 减少锁竞争，提高缓存局部性
   - 应用: 高性能线程池（如pthreadpool）

2. **Wait-free 同步**
   - 描述: 使用原子操作避免锁
   - 最佳实践: 适用于简单的工作通知
   - 应用: pthreadpool 的工作项同步

3. **线程数自动检测**
   - 描述: std::thread::hardware_concurrency() / sysconf(_SC_NPROCESSORS_ONLN)
   - 最佳实践: cores + 1 作为默认值
   - 应用: 构造函数默认参数

### ⭐⭐⭐ 有用知识

1. **Header-only 实现**
   - 描述: 整个库只有一个头文件
   - 使用场景: 简化集成，减少依赖
   - 优势: 无需编译库文件

2. **CMake 构建系统**
   - 描述: 跨平台构建配置
   - 使用场景: 支持Linux/Windows/macOS
   - 优势: IDE集成，自动检测依赖

## 快速参考

| 平台 | 线程API | 锁API | 条件变量 |
|------|---------|-------|----------|
| POSIX | pthread_create | pthread_mutex | pthread_cond |
| Win32 XP | CreateThread | CRITICAL_SECTION | Semaphore+gen |
| Win32 Vista+ | CreateThread | SRWLOCK | CONDITION_VARIABLE |
| C++11 | std::thread | std::mutex | std::condition_variable |

## 常见问题

### Q: 如何选择C还是C++接口？
**A**: 如果需要支持C++03或Pure C，使用C接口。如果只支持C++11+，可以使用std::thread/std::mutex。

### Q: 跨平台线程池的性能差异大吗？
**A**: 主要差异在于同步原语。POSIX pthread通常比Win32 API快。C++11 std::thread在现代编译器上接近原生API性能。

## 相关资源
- [pthreadpool](https://github.com/Maratyszcza/pthreadpool) - Portable C/C++ Thread Pool
- [C-Thread-Pool](https://github.com/Pithikos/C-Thread-Pool) - Simple C Thread Pool
- [Thread Pool in C](https://nachtimwald.com/2019/04/12/thread-pool-in-c) - Tutorial

## 更新日志
- 2026-09-09: 初始创建
