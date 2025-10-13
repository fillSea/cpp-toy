#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "callbacks.h"

class InetAddress;
class SOcket;
class EventLoop;
class EventLoopThreadPool;
class Acceptor;

// 对外的服务器编程使用的类
// TcpServer类管理TcpConnetcion，供用户直接使用，生命周期由用户控制。
// 用户只需要设置好callback，然后调用Start()即可
class TcpServer {
public:
	using ThreadInitCallback = std::function<void(EventLoop*)>;

	enum Option {
		kNoReusePort,  // 不重用端口
		kReusePort,	   // 重用端口
	};

	TcpServer(EventLoop* loop, const InetAddress& listen_addr,
			  const std::string& name_arg, Option option = kNoReusePort);
	~TcpServer();

	// set
	void SetThreadInitCallback(const ThreadInitCallback& cb) {
		thread_init_callback_ = cb;
	}
	void SetConnectionCallback(const ConnectionCallback& cb) {
		connection_callback_ = cb;
	}
	void SetMessageCallback(const MessageCallback& cb) { message_callback_ = cb; }
	void SetWriteCompleteCallback(const WriteCompleteCallback& cb) {
		write_complete_callback_ = cb;
	}
	// 设置底层 SubLoop 的个数
	void SetThreadNum(int num_threads);

	// 开启服务器监听
	void Start();

private:
	void NewConnection(int sock_fd, const InetAddress& peer_addr);
	void RemoveConnection(const TcpConnectionPtr& conn);
	void RemoveConnectionInLoop(const TcpConnectionPtr& conn);

private:
	// 连接名称到conn的映射
	using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

	// baseLoop 用户定义的 loop,
	// 负责接受tcp连接的EventLoop，如果threadNums为1，那么它是唯一的IO线程
	EventLoop* loop_;
	const std::string ip_port_;//ip+port
	const std::string name_;// 服务器名称
    // 持有的listenfd对应的Channel，负责tcp的建立和接受新请求
	std::unique_ptr<Acceptor> acceptor_;
	std::shared_ptr<EventLoopThreadPool> thread_pool_;	// one loop per thread

	// 回调函数
	ConnectionCallback connection_callback_;		 // 连接建立和关闭时的callback
	MessageCallback message_callback_;				 // 有读写消息时的回调
	WriteCompleteCallback write_complete_callback_;	 // 消息发送完成后的回调

	ThreadInitCallback thread_init_callback_;  // loop 线程初始化时的回调

	std::atomic<int> started_;// 是否启动

	int next_conn_id_;// 序号，用于给tcp连接提供名称
	ConnectionMap connections_;	 // 保存所有的连接, 可以看做维持TcpConnection的生命周期
};