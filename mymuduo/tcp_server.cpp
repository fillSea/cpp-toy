#include "tcp_server.h"

#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>

#include <functional>
#include <string>

#include "acceptor.h"
#include "callbacks.h"
#include "event_loop.h"
#include "event_loop_thread_pool.h"
#include "logger.h"
#include "tcp_connection.h"

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
	if (loop == nullptr) {
		LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
	}

	return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listen_addr,
					 const std::string& name_arg, Option option)
	: loop_(CheckLoopNotNull(loop)),
	  ip_port_(listen_addr.ToIpPort()),
	  name_(name_arg),
	  acceptor_(new Acceptor(loop, listen_addr, option == kReusePort)),
	  thread_pool_(new EventLoopThreadPool(loop, name_)),
	  connection_callback_(),
	  message_callback_(),
	  started_(0),
	  next_conn_id_(1) {
	// 当有新用户连接时，Acceptor类中绑定的acceptChannel_会有读事件发生，
	// 执行handleRead()调用TcpServer::newConnection回调
	acceptor_->SetNewConnectionCallback(std::bind(
		&TcpServer::NewConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
	for (auto& item : connections_) {
		// 这个局部的 shared_ptr 智能指针对象出了右括号就会自动释放 new 出来的
		// TcpConnection 对象
		TcpConnectionPtr conn(item.second);
		item.second.reset();

		// 销毁连接
		conn->GetLoop()->RunInLoop(std::bind(&TcpConnection::ConnectDestroyed, conn));
	}
}

// 设置底层 SubLoop 的个数
void TcpServer::SetThreadNum(int num_threads) { thread_pool_->SetThreadNum(num_threads); }

// 开启服务器监听
void TcpServer::Start() {
	// 防止一个 TcpServer 对象被 start 多次
	if (started_++ == 0) {
		// 启动底层的 loop 线程池
		thread_pool_->Start(thread_init_callback_);
		// 开始listen
		loop_->RunInLoop(std::bind(&Acceptor::Listen, acceptor_.get()));
	}
}

// 有一个新的客户端的连接，acceptor会执行这个回调操作
void TcpServer::NewConnection(int sock_fd, const InetAddress& peer_addr) {
	// 轮询算法, 选择一个 SubLoop 来管理 channel
	EventLoop* io_loop = thread_pool_->GetNextLoop();
	std::string conn_name = name_ + "-" + ip_port_ + "#" + std::to_string(next_conn_id_);
	++next_conn_id_;// 这里没有设置为原子类是因为其只在mainloop中执行 不涉及线程安全问题

	LOG_INFO("TcpServer::NewConnection [%s] - new connection [%s] from %s \n",
			 name_.c_str(), conn_name.c_str(), peer_addr.ToIpPort().c_str());

	// 通过 sock_fd 获取其绑定的本机的 ip 地址和端口信息
	sockaddr_in local;
	::bzero(&local, sizeof(local));
	socklen_t addrlen = sizeof(local);
	if (::getsockname(sock_fd, (sockaddr*)&local, &addrlen) < 0) {
		LOG_ERROR("NewConnection get local addr");
	}
	InetAddress local_addr(local);

	// 根据连接成功的 sock fd 创建 TcpConnection 连接对象
	TcpConnectionPtr conn(
		new TcpConnection(io_loop, conn_name, sock_fd, local_addr, peer_addr));

	connections_[conn_name] = conn;

	// 下面的回调都是用户设置给 TcpServer => TcpConnection => Channel => Pooler
	// => notify channel 调用回调
	conn->SetConnectionCallback(connection_callback_);
	conn->SetMessageCallback(message_callback_);
	conn->SetWriteCompleteCallback(write_complete_callback_);
	// 设置关闭连接的回调
	conn->SetCloseCallback(
		std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
	// 直接调用 TcpConnection 的 ConnectEstablished 方法，表示连接建立
	io_loop->RunInLoop(std::bind(&TcpConnection::ConnectEstablished, conn));
}


// muduo被动关闭tcp连接的流程： 
// 1. read收到0，或者epoll监听到HUP事件
// 2. 调用conn中的handleClose函数
// 3. 停止监听所有的事件
// 4. 执行用户的close逻辑
// 5. 执行close回调函数：
// 6. 执行TcpServer中的removeConnection（removeConnectionInLoop）
// 7. connections_中移除conn，引用计数-1
// 8. 执行TcpTcpConnection中connectDestroyed，将Channel指针从loop中移除
// 在上述关闭过程中，为什么需要用到TcpServer中的函数，原因是connections_这个数据结构的存在
// 为了维持TcpConnection的生存期，需要将ptr保存在connections_中，当tcp关闭时，
// 也必须去处理这个数据结构
void TcpServer::RemoveConnection(const TcpConnectionPtr& conn) {
	loop_->RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
}

void TcpServer::RemoveConnectionInLoop(const TcpConnectionPtr& conn) {
	LOG_INFO("TcpServer::RemoveConnectionInLoop [%s] - connection %s \n", name_.c_str(),
			 conn->GetName().c_str());

	connections_.erase(conn->GetName());
	EventLoop* io_loop = conn->GetLoop();
	io_loop->QueueInLoop(std::bind(&TcpConnection::ConnectDestroyed, conn));
}