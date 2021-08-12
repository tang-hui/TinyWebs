# TinyWebs

**A C++ Tiny Web Server**
## 已完成
 + 基本的TCP被动连接库
 + 简易的HTTP服务器，可以访问主页HTML
***

## 技术
+ 主从Reactor模式：
    - 主Reactor负责监听连接(被动连接)，当有新的连接，accept到新的socket后，使用 Round Robin 方法下发给从Reactor
    - 从Reactor负责三种事件：1、负责管理事件描述符(timefd处理定时任务) 2、事件描述符(eventfd用于唤醒IO线程) 3、派发下来的socket文件描述符
+ multiple Reactors + thread pool (one loop per thread + thread pool)；
+ EventLoop：使用 Epoll 水平触发的模式结合非阻塞 IO；
+ 线程池：
    - 使用多线程能发挥多核的优势；
    - 线程池可以避免线程的频繁地创建和销毁的开销；
+ 双缓冲异步日志系统；
+ 基于最小堆的定时器；
+ 使用智能指针等 RAII 机制，降低内存泄漏的可能性；
***

## 压测


***

## 参考
+ [陈硕老师的Blog](http://www.cppblog.com/solstice/)
+ [Muduo源码分析](https://youjiali1995.github.io/network/muduo/)
+ [muduo源码剖析 - cyhone的文章 - 知乎](https://zhuanlan.zhihu.com/p/85101271)
+ [开源HTTP解析器---http-parser和fast-http](https://www.cnblogs.com/arnoldlu/p/6497837.html)
+ [HTTP请求和响应格式](https://www.cnblogs.com/yaozhongxiao/archive/2013/03/02/2940252.html)
+ [Reactor事件驱动的两种设计实现：面向对象 VS 函数式编程](https://www.cnblogs.com/me115/p/5088914.html)
+ [写一个并发http服务器](https://zhuanlan.zhihu.com/p/23336565)
