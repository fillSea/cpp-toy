#include "acceptor.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <functional>

#include "event_loop.h"
#include "inet_address.h"
#include "logger.h"

// 创建 socket_fd
static int CreateNonblocking() {
	int sock_fd =
		::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	if (sock_fd < 0) {
		LOG_FATAL("%s:%s:%d listen socket create err: %d \n", __FILE__, __FUNCTION__,
				  __LINE__, errno);
	}

	return sock_fd;
}

// Acceptor这类对象，内部持有一个Channel，和TcpConnection相同，必须在构造函数中设置各种回调函数
// 然后在其他动作中开始监听，向epoll注册fd
Acceptor::Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port)
	: loop_(loop),
	  accept_socket_(CreateNonblocking()),
	  accept_channel_(loop, accept_socket_.GetFd()),
	  listenning_(false) {
	accept_socket_.SetReuseAddr(true);		  // 复用addr
	accept_socket_.SetReusePort(reuse_port);		  // 复用port
	accept_socket_.BindAddress(listen_addr);  // 绑定ip和port
	// TcpServer::Start() => Acceptor.listen() 有新的用户连接就执行一个回调
	// baseLoop => accept_channel_(listen_fd)
	accept_channel_.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
}

Acceptor::~Acceptor() {
	accept_channel_.DisableAll();  // 把从Poller中感兴趣的事件删除掉
	// 调用EventLoop->removeChannel => Poller->removeChannel
	// 把Poller的ChannelMap对应的部分删除
	accept_channel_.Remove();
}

// 监听fd
void Acceptor::Listen() {
	listenning_ = true;
	accept_socket_.Listen();
	accept_channel_.EnableReading();  // 开始在epoll中监听read事件
}

// 当epoll监听到listenfd时，开始执行此函数
void Acceptor::HandleRead() {
	InetAddress peer_addr;
	int conn_fd = accept_socket_.Accept(&peer_addr);
	if (conn_fd >= 0) {	 // 成功连接
		if (new_connection_callback_) {
            // 轮询找到subLoop 唤醒并分发当前的新客户端的Channel
			new_connection_callback_(conn_fd, peer_addr);
		} else {
			::close(conn_fd);
		}
	} else {
		LOG_ERROR("%s:%s:%d accept err: %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
		if (errno == EMFILE) {// fd的数目达到上限
			LOG_ERROR("%s:%s:%d sock fd reached limit! \n", __FILE__, __FUNCTION__,
					  __LINE__);
		}
	}
}
