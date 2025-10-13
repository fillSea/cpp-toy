#pragma once

#include <functional>

#include "channel.h"
#include "event_loop.h"
#include "noncopyable.h"
#include "socket.h"

// 这个类主要是封装了listenfd对应的Channel。
// 它的任务就是创建listenfd，然后accept新的tcp连接是它最核心的职责
// 所以这个类已经具备了一个tcp服务器大部分的能力
// 但这个类只负责接受tcp连接，并不负责tcp连接的分配，这个是Acceptor的上层-TcpServer的任务
class Acceptor : Noncopyable {
public:
	using NewConnectionCallback = std::function<void(int sock_fd, const InetAddress&)>;

	Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port);
	~Acceptor();

	// 设置新连接的回调函数
	void SetNewConnectionCallback(const NewConnectionCallback& cb) {
		new_connection_callback_ = cb;
	}
	// 判断是否在监听
	bool GetListenning() const { return listenning_; }
	// 监听本地端口
	void Listen();

private:
	void HandleRead();	// 处理新用户的连接事件
private:
	EventLoop* loop_;  // Acceptor 用的就是用户定义的那个 base_loop, 也称作 mainLoop
	Socket accept_socket_;	  // 专门用于接收新连接的socket
	Channel accept_channel_;  // 专门用于监听新连接的channel
	// TcpServer构造函数中将TcpServer::newConnection()
	// 函数注册给了这个成员变量。这个TcpServer::newConnection函数的功能是
    // 公平的选择一个subEventLoop，并把已经接受的连接分发给这个subEventLoop。
	NewConnectionCallback new_connection_callback_;	 // 新连接的回调函数
	bool listenning_;								 // 是否在监听
};