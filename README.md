# my_raft

# 1 简介

* 内容：借鉴raft算法思路，使用epoll框架，实现了多台服务器反馈客户端访问信息，和各个服务器之间的条目一致性
* 语言和环境：C/C++   Ubuntu18（64-bit）
* 框架+运行操作介绍：

1. 1. 三个服务器结点，`IP`地址和端口号`port`程序内部默认为0.0.0.0和8000、8001、8002。编号为A、B、C，启用时，输入一个参数A or B or C。

      编译命令：`make`

      运行命令为：`./a A`

1. 2. 一个服务器程序`client.cpp`，`IP`地址默认为0.0.0.0。启用时输入参数port，表示想要连接的服务器结点。

      编译命令为：`g++ client.cpp -o b`

      运行命令为：`./b 8000`（或者8001、8002）

1. 3. 数据：每个服务器维护自己编号的数据库，如果您修改了程序和文件的相对路径，请在代码对应位置进行修改。

* 实现的功能：

1. 1. 启动/杀死/加入任意一个服务器，均能实现raft算法里正常的选举新的leader的操作。
   2. 当客户端访问的服务器不是leader时，该follower服务器会返回leader结点端口号，客户端程序自动重新连接到leader，并告知用户请重新输入命令。
   3. 客户端进行访问操作时，服务器能正常反馈访问操作（进行了中文字符串乱码处理、大数据套接字发送接收处理）
   4. 客户端进行写操作：

1. 1. 1. 如果仅有一个服务器结点，拒绝该请求
      2. 如果有半数服务器结点，接收该请求，并保证将该条目更新到所有“活着”的服务器条目记录中（不论是在该请求前加还是后加，还是中途推出后，重新加入。也不论崩溃的是leader还是follower服务器）



算法思路、代码实现、操作讲解后续实现。（图文+视频）

# 4 参考链接

1. 解说1：[一文搞懂Raft算法 - xybaby - 博客园](https://www.cnblogs.com/xybaby/p/10124083.html)
2. 解说2：[Raft算法详解](https://zhuanlan.zhihu.com/p/32052223)
3. 解说3：[条分缕析 Raft 算法，看完每个细节你都懂了！ - 掘金](https://juejin.cn/post/6924468915724615688)
4. 解说4：[Raft算法解析-尝试讲解的透彻一点_raft状态机_hawonor的博客-CSDN博客](https://blog.csdn.net/weixin_44039270/article/details/106953316)
5. 有趣的检验：[Raft 作者亲自出的 Raft 试题，你能做对几道？](https://mp.weixin.qq.com/s?__biz=MzIwODA2NjIxOA==&mid=2247483932&idx=1&sn=895af82bf5939d9be5e862f73f74acbd&chksm=970981d9a07e08cf4c4121543aa6e2420a6a7c7c40f116bf89b534b6f2a54a46b2402522e2a5&scene=21#wechat_redirect)
6. [raft论文，写的很好懂，想实现的话请务必要看](https://web.stanford.edu/~ouster/cgi-bin/papers/raft-atc14)
7. [raft论文中文版（估计是机翻）](https://object.redisant.com/doc/raft中译版-2023年4月23日.pdf)
8. [raft算法动画演示](http://thesecretlivesofdata.com/raft/)