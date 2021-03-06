# 压力测试

## 测试环境

### 腾讯云虚拟主机

- OS: Ubuntu 16.04 LTS
- CPU: 1核
- 内存：2M

### 本地主机

- OS：Ubuntu 1.04 LTS
- CPU：8核
- 内存：16G
8
## 测试方法

- 为了避免带宽带来的影响，将服务器程序和 WebBench 程序都运行在同一台主机上。
- 使用工具Webbench，开启1000客户端进程，时间为60s。
- 分别测试短连接和长连接的情况。
- 关闭所有的普通的 Log，只留下 ERROR 级别的 Log。
- 为避免磁盘 IO 对测试结果的影响，测试响应为内存中的"Hello World"字符加上必要的HTTP头。
- 因为发送的内容很少，为避免发送可能的延迟，禁用 Nagle 算法。
- 线程池开启4线程。

## 尝试

- 在性能更好的本地主机上运行一下测试，看看硬件环境对服务器性能的影响有多大。在多核CPU上运行，性能应该能得到很大的提升。
- 对比水平触发方式（本实现）和边缘触发方式对性能的影响。

## 测试结果

| 主机           | 短连接  | 长连接  | 长连接（禁用 Nagle 算法） |
| -------------- | ------- | ------- | ------------------------- |
| 腾讯云虚拟主机 | 341837  | 1207940 | 1301368                   |
| 本地主机       | 1554467 | 7579179 | 7885731（未截图）         |

处理请求数方面，长连接比短连接能很多，差不多为3倍。

因为发送的内容很少，为避免发送可能的延迟，禁用了 Nagle 算法，请求数并没有得到显著的提升。

在本地主机上运行的程序比在腾讯云虚拟主机上运行的程序要快得多，可以看出来，硬件的性能对并发数的影响也是很大的。

## 测试结果截图

### 腾讯云虚拟主机

- 短连接

![短连接](pressure_test1_short.png)

- 长连接

![长连接](pressure_test1_keep_alive.png)

- 长连接（禁用 Nagle 算法）

![长连接（禁用 Nagle 算法）](pressure_test1_keep_alive_noNagle.png)

### 本地主机

- 短连接

![短连接](/Users/chenbright/Desktop/c:c++_workspace/tinyWS/doc/pressure_test2_short.png)

- 长连接

![长连接](/Users/chenbright/Desktop/c:c++_workspace/tinyWS/doc/pressure_test2_keep_alive.png)

## 参考

- [linyacool](https://github.com/linyacool)的[WebServer](https://github.com/linyacool/WebServer)中的[测试及改进]([https://github.com/linyacool/WebServer/blob/HEAD/%E6%B5%8B%E8%AF%95%E5%8F%8A%E6%94%B9%E8%BF%9B.md](https://github.com/linyacool/WebServer/blob/HEAD/测试及改进.md))。
- [WebBench](https://github.com/linyacool/WebBench)来自[linyacool](https://github.com/linyacool)的[WebServer](https://github.com/linyacool/WebServer)。