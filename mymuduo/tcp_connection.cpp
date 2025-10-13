#include "tcp_connection.h"

#include <asm-generic/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <functional>
#include <string>

#include "callbacks.h"
#include "channel.h"
#include "event_loop.h"
#include "logger.h"
#include "socket.h"
#include "timestamp.h"

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
	if (loop == nullptr) {
		LOG_FATAL("%s:%s:%d TcpConnection loop is null! \n", __FILE__, __FUNCTION__,
				  __LINE__);
	}

	return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name_arg, int sock_fd,
							 const InetAddress& local_addr, const InetAddress& peer_addr)
	: loop_(CheckLoopNotNull(loop)),
	  name_(name_arg),
	  state_(kConnecting),
	  reading_(true),
	  socket_(new Socket(sock_fd)),
	  channel_(new Channel(loop, sock_fd)),
	  local_addr_(local_addr),
	  peer_addr_(peer_addr),
	  // 64M
	  high_water_mark_(64 * 1024 * 1024) {
	// 下面给 channel 设置相应的回调函数, poller 给 channel 通知感兴趣的事件发送了,
	// channel 会回调相应的操作函数
	channel_->SetReadCallback(
		std::bind(&TcpConnection::HandleRead, this, std::placeholders::_1));
	channel_->SetWriteCallback(std::bind(&TcpConnection::HandleWrite, this));
	channel_->SetCloseCallback(std::bind(&TcpConnection::HandleClose, this));
	channel_->SetErrorCallback(std::bind(&TcpConnection::HandleError, this));

	LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sock_fd);

	socket_->SetKeepAlive(true);
}

TcpConnection::~TcpConnection() {
	LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n", name_.c_str(),
			 channel_->GetFd(), state_.load());
}

// 处理read事件，receiveTime指的是poll调用返回的时间点
// 读是相对服务器而言的, 当对端客户端有数据到达, 服务器端检测到 EPOLLIN
// 就会触发该fd上的回调 handleRead取读走对端发来的数据
void TcpConnection::HandleRead(Timestamp receive_time) {
	int saved_errno = 0;
	ssize_t n = input_buffer_.ReadFd(channel_->GetFd(), &saved_errno);
	if (n > 0) {  // 有数据到达
		// 已建立连接的用户, 有读事件发生了, 调用用户传入的回调操作OnMessage
		message_callback_(shared_from_this(), &input_buffer_, receive_time);
	} else if (n == 0) {  // 客户端断开
		HandleClose();
	} else {  // 出错了
		errno = saved_errno;
		LOG_ERROR("TcpConnection::HandleRead");
		HandleError();
	}
}

// 处理写事件
void TcpConnection::HandleWrite() {
	// 如果Channel正在监听write事件
	if (channel_->IsWriteEvent()) {
		int saved_errno = 0;
		ssize_t n = output_buffer_.WriteFd(channel_->GetFd(), &saved_errno);
		if (n > 0) {
			// 从输出缓冲区中将已经发送的数据移除
			output_buffer_.Retrieve(n);
			// 所有数据已经发送完毕
			if (output_buffer_.ReadableBytes() == 0) {
				// 停止监听fd的写事件，因为非阻塞需要监听写事件，所以需要关注是否还有字节可写
				channel_->DisableWriting();
				// 数据全部发送完毕，需要在loop中执行这个函数，这个函数可以控制发送的速度，使其不超过接收的速度
				if (write_complete_callback_) {
					// 唤醒 loop 对应的 thread 线程, 执行回调
					loop_->QueueInLoop(
						std::bind(write_complete_callback_, shared_from_this()));
				}

				// kDisconnecting表示TCP出于半关闭
				if (state_ == kDisconnecting) {
					ShutdownInLoop();  // 数据全部发送完毕，这里需要彻底关闭连接
				}
			}
		} else {
			LOG_ERROR("TcpConnection::HandleWrite");
		}
	} else {
		LOG_ERROR("TcpConnection fd=%d is down, no more writing \n", channel_->GetFd());
	}
}

// 处理连接关闭事件
// 当read返回0，或者epoll遇到hup时，调用此函数，处理close事件
void TcpConnection::HandleClose() {
	LOG_INFO("TcpConnection::HandleClose fd=%d state=%d \n", channel_->GetFd(),
			 state_.load());
	SetState(kDisconnected);
	channel_->DisableAll();	 // Channel停止监听所有的事件

	// 会通过ConnectDestroyed调用channel->Remove()
	TcpConnectionPtr conn_ptr(shared_from_this());
	connection_callback_(conn_ptr);	 // 执行用户的关闭连接逻辑
    // 执行上层的Tcpserver注册的函数, 执行的是TcpServer::RemoveConnection()
	close_callback_(conn_ptr);		 
}

// 处理错误事件
void TcpConnection::HandleError() {
	int optval;
	socklen_t opt_len = sizeof(optval);
	int err = 0;
	if (::getsockopt(channel_->GetFd(), SOL_SOCKET, SO_ERROR, &optval, &opt_len) < 0) {
		err = errno;
	} else {
		err = optval;
	}

	LOG_ERROR("TcpConnection::HandleError name:%s - SO_ERROR:%d \n", name_.c_str(), err);
}

// 发送数据
void TcpConnection::Send(const std::string& buf) {
	if (state_ == kConnected) {
		// 如果是在loop线程内，就直接发送数据
		if (loop_->IsInLoopThread()) {
			SendInLoop(buf.c_str(), buf.size());
		} else {
			// 如果是在别的线程发送数据，则将任务放入loop的任务队列
			loop_->RunInLoop(std::bind(&TcpConnection::SendInLoop, shared_from_this(),
									   buf.c_str(), buf.size()));
		}
	}
}

// 因为muduo中的IO不能跨线程，所以发送msg必须在EventLoop中，所以这里的sendInLoop底层
// 有判断，如果跨线程，则将其放入队列，这几个函数供send调用
/**
 * 发送数据 应用写的快, 而内核发送数据慢, 需要把待发送数据写入缓冲区, 而且设置了水位回调
 */
// 1.将应用层的数据写入到内核的发送缓冲区。
// 2.处理内核缓冲区写满的情况：将剩余数据保存在用户空间的缓冲区(outputBuffer_)中。
// 3.如果数据发送完全，则触发发送完成的回调(writeCompleteCallback_)。
// 4.如果数据未发送完全，需要注册写事件(EPOLLOUT)，当内核缓冲区由空间时重新发送。
void TcpConnection::SendInLoop(const void* data, size_t len) {
	ssize_t nwrote = 0;		   // 发送的数据的字节数
	size_t remaing = len;	   // 剩余未写的数据
	bool fault_error = false;  // 是否发生了错误

	// 之前调用过该 connection 的 shutdown, 不能再发送了
	if (state_ == kDisconnected) {
		LOG_ERROR("disconnected, give up writing!");
		return;
	}

	// 表示 channel_ 第一次开始写数据, 而且缓冲区没有待发送的数据
	// 如果输出缓冲区中没有数据，可以直接对fd写入数据
	if (!channel_->IsWriteEvent() && output_buffer_.ReadableBytes() == 0) {
		nwrote = ::write(channel_->GetFd(), data, len);
		if (nwrote >= 0) {	// 发送成功
			remaing = len - nwrote;
			if (remaing == 0 && write_complete_callback_) {
				// 既然数据在这里全部发送完成, 就不用再给 channel 设置 epollout 事件了
				// 如果全部发送完毕，触发writeCompleteCallback_函数
				loop_->QueueInLoop(
					std::bind(write_complete_callback_, shared_from_this()));
			}
		} else {  // nwrote < 0
			nwrote = 0;
			// EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回, 等同于EAGAIN
			// 当前操作在非阻塞模式下无法立即完成，需要稍后重试
			if (errno == EWOULDBLOCK) {
				LOG_ERROR("TcpConnection::SendInLoop");
				// SIGPIPE RESET
				// EPIPE: 向一个 “读端已关闭的管道（或套接字）” 写入数据
				// ECONNRESET:
				// “连接被对端重置”（即对端主动关闭了连接，且未正常处理剩余数据）
				if (errno == EPIPE || errno == ECONNRESET) {
					fault_error = true;
				}
			}
		}
	}

	// 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
	// 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
	// 也就是调用TcpConnection::handlewrite方法，把发送缓冲区中的数据全部发送完成
	if (!fault_error && remaing > 0) {
		// 目前发送缓冲区剩余的待发送数据的长度
		size_t old_len = output_buffer_.ReadableBytes();
		if (old_len + remaing >= high_water_mark_ && old_len < high_water_mark_ &&
			high_water_mark_callback_) {
			// 在loop线程中执行高水位回调函数
			loop_->QueueInLoop(std::bind(high_water_mark_callback_, shared_from_this(),
										 old_len + remaing));
		}
		// 将未发送的data中的数据放入输出缓冲区
		output_buffer_.Append(static_cast<const char*>(data) + nwrote, remaing);
		// 如果对应的Channel没有在监听write事件
		if (!channel_->IsWriteEvent()) {
			// 这里一定要注册 channel 的写事件, 否则 poller 不会给 channel 通知 epollout
			// 开启Channel的write事件，实际上在epoll中添加对该fd的write监听
			channel_->EnableWriting();
		}
	}
}

// 连接建立
void TcpConnection::ConnectEstablished() {
	SetState(kConnected);
	channel_->Tie(shared_from_this());
	// 向 poller 注册 channel 的 epollin 事件
	channel_->EnableReading();
	// 新连接建立, 执行回调
	connection_callback_(shared_from_this());
}

// 连接销毁
void TcpConnection::ConnectDestroyed() {
	if (state_ == kConnected) {
		SetState(kDisconnected);
		// 销毁 channel 感兴趣的所有事件
		channel_->DisableAll();
        // 执行用户的关闭逻辑
		connection_callback_(shared_from_this());
	}

	channel_->Remove();	 // 将 channel 从 poller 中删除掉
}

// 关闭写端
void TcpConnection::Shutdown(){
    if (state_ == kConnected){
        SetState(kDisconnecting);
        loop_->RunInLoop(std::bind(&TcpConnection::ShutdownInLoop, this));
    }
}

void TcpConnection::ShutdownInLoop(){
    // 说明 oputput_buffer 中的数据已经全部发送完
    if (!channel_->IsWriteEvent()){
        socket_->ShutdownWrite(); // 关闭写端
    }
}