# 技巧和提示 - 快速参考

## 线程池配置

### 线程数选择
```
CPU密集型: cores + 1
IO密集型: cores * 2 或更多
混合型: 根据测试调整
```

### 空闲超时
```
短任务: 1-2秒
长任务: 5-10秒
动态调整: 根据负载自动伸缩
```

## 代码模式

### C++11 任务提交
```cpp
auto handle = pool.submit([]() {
    // 任务内容
    return result;
});
auto result = handle.get(); // 阻塞等待结果
```

### Pure C 任务提交
```c
tp_handle_t* h = tp_submit(pool, task_func, arg);
tp_handle_wait(h, -1); // 等待完成
tp_handle_destroy(h);  // 释放资源
```

### 条件变量等待模式
```c
// POSIX
pthread_mutex_lock(&mutex);
while (!condition)
    pthread_cond_wait(&cond, &mutex);
// 使用共享数据
pthread_mutex_unlock(&mutex);

// Win32 XP兼容
EnterCriticalSection(&cs);
while (!condition)
    tp_cond_wait(&cv, &cs);
// 使用共享数据
LeaveCriticalSection(&cs);
```

## 性能优化

### 减少锁竞争
- 使用work-stealing代替单一队列
- 读多写少场景使用读写锁
- 简单场景使用原子操作

### 缓存优化
- 使用紧凑的数据结构
- 避免伪共享（padding）
- 批量处理任务

### 内存管理
- 预分配任务对象
- 使用对象池
- 避免频繁malloc/free

## 测试检查清单

- [ ] 正常任务执行
- [ ] 任务取消
- [ ] 超时处理
- [ ] 异常安全
- [ ] 优雅关闭
- [ ] 内存泄漏检测
- [ ] 并发压力测试
