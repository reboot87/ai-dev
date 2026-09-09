# Best practices for using the thread pool - Windows apps

## 元数据
- **来源**: https://learn.microsoft.com/en-us/windows/apps/develop/threading/best-practices-for-using-the-thread-pool
- **获取时间**: 2026-09-09 17:55
- **作者**: Microsoft
- **类型**: 官方文档
- **重要性**: ⭐⭐⭐⭐⭐

---

## Do's

- Use the thread pool to do parallel work in your app.
- Use work items to accomplish extended tasks without blocking the UI thread.
- Create work items that are short-lived and independent. Work items run asynchronously and they can be submitted to the pool in any order from the queue.
- Dispatch updates to the UI thread.
- Use **ThreadPoolTimer.CreateTimer** instead of the **Sleep** function.
- Use the thread pool instead of creating your own thread management system. The thread pool runs at the OS level with advanced capability and is optimized to dynamically scale according to device resources and activity within the process and across the system.
- In C++, ensure that work item delegates use the agile threading model (C++ delegates are agile by default).
- Use pre-allocated work items when you can't tolerate a resource allocation failure at time of use.

## Don'ts

- Don't create periodic timers with a *period* value of <1 millisecond (including 0). This will cause the work item to behave as a single-shot timer.
- Don't submit periodic work items that take longer to complete than the amount of time you specified in the *period* parameter.
- Don't try to send UI updates (other than toasts and notifications) from a work item dispatched from a background task.
- When you use work-item handlers that use the **async** keyword, don't assume that all code in the handler has executed when the complete state has been set on the work item.
- Don't try to run a pre-allocated work item more than once without reinitializing it.
