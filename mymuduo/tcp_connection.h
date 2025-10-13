#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>

#include "buffer.h"
#include "callbacks.h"
#include "inet_address.h"
#include "noncopyable.h"

class Socket;
class Channel;
class EventLoop;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * =》TcpConnection 设置回调 =》Channel =》Poller =》Channel的回调操作
 */
class TcpConnection : Noncopyable, public std::enable_shared_from_this<TcpConnection> {
public:
	TcpConnection(EventLoop* loop, const std::string& name_arg, int sock_fd,
				  const InetAddress& local_addr, const InetAddress& peer_addr);
	~TcpConnection();

	// 连接建立
	void ConnectEstablished();
	// 连接销毁
	void ConnectDestroyed();
	// 是否已连接
	bool IsConnected() const { return state_ == kConnected; }
	// 关闭写端
	void Shutdown();

	// 发送数据
	void Send(const std::string& buf);

	// get/set
	EventLoop* GetLoop() const { return loop_; }
	const std::string& GetName() const { return name_; }
	const InetAddress& LocalAddr() const { return local_addr_; }
	const InetAddress& PeerAddr() const { return peer_addr_; }

	void SetConnectionCallback(const ConnectionCallback& cb) {
		connection_callback_ = cb;
	}
	void SetMessageCallback(const MessageCallback& cb) { message_callback_ = cb; }
	void SetWriteCompleteCallback(const WriteCompleteCallback& cb) {
		write_complete_callback_ = cb;
	}
	void SetHighWaterMarkCallback(const HighWaterMarkCallback& cb,
								  size_t high_water_mark) {
		high_water_mark_callback_ = cb;
		high_water_mark_ = high_water_mark;
	}
    void SetCloseCallback(const CloseCallback& cb){
        close_callback_ = cb;
    }

private:
	// 处理read事件，receiveTime指的是poll调用返回的时间点
	void HandleRead(Timestamp receive_time);
	void HandleWrite();	 // 处理写事件
	void HandleClose();	 // 处理连接关闭事件
	void HandleError();	 // 处理错误事件

	// 因为muduo中的IO不能跨线程，所以发送msg必须在EventLoop中，所以这里的sendInLoop底层
	// 有判断，如果跨线程，则将其放入队列，这几个函数供send调用
	void SendInLoop(const void* data, size_t len);
	void ShutdownInLoop();

	void SetState(int state) { state_ = state; }

private:
	enum StateE {
		kDisconnected,	// 已经断开连接
		kConnecting,	// 正在连接
		kConnected,		// 已连接
		kDisconnecting	// 正在断开连接
	};

	// 处理该TCP连接的EventLoop，该EventLoop内部的epoll监听TCP连接对应的fd
	// 这里是baseloop还是subloop由TcpServer中创建的线程数决定 若为多Reactor
	// 该loop_指向subloop 若为单Reactor 该loop_指向baseloop
	EventLoop* loop_;
	const std::string name_;  // 连接的名字
	std::atomic<int> state_;  // 本条TCP连接的状态
	bool reading_;			  // 连接是否在监听读事件

	// Socket Channel 这里和Acceptor类似
	// Acceptor => mainloop    TcpConnection => subloop
	std::unique_ptr<Socket> socket_;  // TCP连接的fd所在的socket对象 fd的关闭由它决定
	std::unique_ptr<Channel> channel_;	// TCP连接fd对应的Channel，将其放入EventLoop

	const InetAddress local_addr_;	// TCP连接中本地的ip地址和端口号
	const InetAddress peer_addr_;	// TCP连接中对方的ip地址和端口号

	// 回调函数
	// 这些回调TcpServer 由用户通过写入TcpServer注册,
	// TcpServer再将注册的回调传递给TcpConnection,
	// TcpConnection再将回调注册到Channel中
	ConnectionCallback connection_callback_;  // 连接建立和关闭时的callback
	MessageCallback message_callback_;		  // 有读写消息时的回调
	WriteCompleteCallback write_complete_callback_;	  // 消息发送完成后的回调
	HighWaterMarkCallback high_water_mark_callback_;  // 高水位回调函数
	CloseCallback close_callback_;					  // 关闭TCP连接的回调函数
	size_t high_water_mark_;						  // 高水位标记

	// 缓冲区
	Buffer input_buffer_;	// 接收数据的缓冲区
	Buffer output_buffer_;	// 发送数据的缓冲区, 用户send向outputBuffer_发
};