#include "socket.h"
#include <asm-generic/socket.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include "logger.h"
#include <netinet/tcp.h>
#include "inet_address.h"

Socket::~Socket(){
    ::close(sock_fd_);
}


void Socket::BindAddress(const InetAddress& local_addr){
    if (0 != ::bind(sock_fd_, (sockaddr*)local_addr.GetSockAddr(), sizeof(sockaddr_in))){
        LOG_FATAL("bind sock fd: %d fail \n", sock_fd_);
    }
}

void Socket::Listen(){
    if (0 != ::listen(sock_fd_, 1024)){
        LOG_FATAL("listen sock fd: %d fail \n", sock_fd_);
    }
}

int Socket::Accept(InetAddress* peer_addr){
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ::bzero(&addr, sizeof(addr));
    int conn_fd = ::accept4(sock_fd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (conn_fd >= 0){
        peer_addr->SetSockAddr(addr);
    }

    return conn_fd;
}

void Socket::ShutdownWrite(){
    if (::shutdown(sock_fd_, SHUT_WR) < 0){
        LOG_ERROR("shutdownWrite error");
    }
}

void Socket::SetTcpNoDelay(bool on){
    // TCP_NODELAY 用于禁用 Nagle 算法。
    // Nagle 算法用于减少网络上传输的小数据包数量。
    // 将 TCP_NODELAY 设置为 1 可以禁用该算法，允许小数据包立即发送。
    int optval = on ? 1 : 0;
    ::setsockopt(sock_fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::SetReuseAddr(bool on){
    // SO_REUSEADDR 允许一个套接字强制绑定到一个已被其他套接字使用的端口。
    // 这对于需要重启并绑定到相同端口的服务器应用程序非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::SetReusePort(bool on){
    // SO_REUSEPORT 允许同一主机上的多个套接字绑定到相同的端口号。
    // 这对于在多个线程或进程之间负载均衡传入连接非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::SetKeepAlive(bool on){
    // SO_KEEPALIVE 启用在已连接的套接字上定期传输消息。
    // 如果另一端没有响应，则认为连接已断开并关闭。
    // 这对于检测网络中失效的对等方非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sock_fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}