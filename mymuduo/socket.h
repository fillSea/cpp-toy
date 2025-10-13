#pragma once


#include "noncopyable.h"

class InetAddress;

// 封装 sock fd 的操作
class Socket: Noncopyable{
public:
    explicit Socket(int sock_fd): sock_fd_(sock_fd){}
    ~Socket();

    int GetFd() const {return sock_fd_;}
    // 封装 sock 编程函数
    void BindAddress(const InetAddress& local_addr);
    void Listen();
    int Accept(InetAddress *peer_addr);

    // 关闭写端口
    void ShutdownWrite();

    // 设置 sock_fd_ 的特性
    void SetTcpNoDelay(bool on);
    void SetReuseAddr(bool on);
    void SetReusePort(bool on);
    void SetKeepAlive(bool on);
private:
    const int sock_fd_;
};