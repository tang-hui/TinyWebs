# tinyWS
A C++ Tiny Web Server

## 已完成

- 完成基本的Tcp（被动连接）库；
- 简易的HTTP服务器，可访问主页HTML和网站favicon图标。

## 技术

- 主从 reactor 模式：
    - 主 reactor 负责监听连接，当有新的连接，accept 到新的 socket 后，使用 Round Robin 方法选择从 reactor，将 socket 派发给从 reactor；
    - 从 reactor 负责管理时间描述符（timerfd用于定时任务）、事件描述符（eventfd 用于唤醒 IO 线程）和 派发过来的 socket 文件描述符。
- multiple reactors + thread pool (one loop per thread + threadpool)； 
- EventLoop：使用 Epoll 水平触发的 IO 多路复用技术，非阻塞 IO；
- 线程池：
    - 使用多线程能发挥多核的优势；
    - 线程池可以避免线程的频繁地创建和销毁的开销。
- 简单的日志系统；
- 使用智能指针等 RAII 机制，来为降低内存泄漏的可能性；

## 并发模型

并发模型为 multiple reactors + thread pool (one loop per thread + threadpool)； + 非阻塞 IO，新连接使用 Round Robin 策略派发。

![并发模型](doc/model.png)

## 压测

[压测结果](doc/pressure_test.md)。

## TODO

- 为了获取 TimeType 而暴露了 Timer（内部对象），应调整结构，隐藏 Timer 对象
- 解决代码中的TODO
- Preactor模式、actor模式和reactor模式等模式的区别
- 编写文档，解释核心原理

## 参考

- [陈硕老师的Blog](http://www.cppblog.com/solstice/)
- [linyacool](https://github.com/linyacool)的[WebServer](https://github.com/linyacool/WebServer)
- [uv-cpp](https://github.com/wlgq2/uv-cpp)
- [开源HTTP解析器](https://www.cnblogs.com/arnoldlu/p/6497837.html)
- [HTTP请求和响应格式](https://www.cnblogs.com/yaozhongxiao/archive/2013/03/02/2940252.html)
- [写一个并发http服务器](https://zhuanlan.zhihu.com/p/23336565)（有代码）
- [有什么适合提高 C/C++ 网络编程能力的开源项目推荐？ - Variation的回答 - 知乎](https://www.zhihu.com/question/20124494/answer/733016078)（master/worker、proactor 等 IO 模型）
- [Reactor事件驱动的两种设计实现：面向对象 VS 函数式编程](http://www.cnblogs.com/me115/p/5088914.html)
- [网络编程中的关键问题总结](https://www.cnblogs.com/me115/p/5092091.html)
- [muduo源码剖析 - cyhone的文章 - 知乎](https://zhuanlan.zhihu.com/p/85101271)
- [Muduo 源码分析](https://youjiali1995.github.io/network/muduo/)