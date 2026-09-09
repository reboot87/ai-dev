# Thread Pool Design - 来源列表

## 来源统计
- 总计: 4
- 核心来源: 2
- 有用来源: 2

## 来源列表

### 1. Microsoft Thread Pool Best Practices
- **URL**: https://learn.microsoft.com/en-us/windows/apps/develop/threading/best-practices-for-using-the-thread-pool
- **类型**: 官方文档
- **重要性**: ⭐⭐⭐⭐⭐
- **本地文件**: `raw/02-msdn-best-practices.md`
- **关键内容**: Windows线程池最佳实践，包括Do's和Don'ts

### 2. Thread Pool in C
- **URL**: https://nachtimwald.com/2019/04/12/thread-pool-in-c
- **类型**: 教程
- **重要性**: ⭐⭐⭐⭐
- **本地文件**: `raw/01-thread-pool-in-c.md`
- **关键内容**: 完整的C语言线程池实现，包含worker函数、wait模式、shutdown模式

### 3. pthreadpool
- **URL**: https://github.com/Maratyszcza/pthreadpool
- **类型**: 开源项目
- **重要性**: ⭐⭐⭐⭐
- **本地文件**: `raw/02-pthreadpool.md`
- **关键内容**: 跨平台C/C++线程池，支持work-stealing和wait-free同步

### 4. BS::thread_pool (arXiv paper)
- **URL**: https://arxiv.org/html/2105.00613v4
- **类型**: 学术论文
- **重要性**: ⭐⭐⭐
- **本地文件**: 未保存
- **关键内容**: C++17高性能科学计算线程池，header-only设计
