#pragma once

#include <functional>
#include <memory>
#include <utility>

#include "noncopyable.h"
#include "timestamp.h"

class EventLoop;

/**
Channel 是通道, 封装了 sockfd 和其感兴趣的 event, 绑定了 poller 返回的具体事件
 */
class Channel : Noncopyable {
public:
	using EventCallback = std::function<void()>;
	using ReadEventCallback = std::function<void(Timestamp)>;

	Channel(EventLoop* loop, int fd);
	~Channel();
	// 事件回调
	void HandleEvent(Timestamp receive_time);
	// 设置回调函数对象
	void SetReadCallback(ReadEventCallback cb) { read_callback_ = std::move(cb); }
	void SetWriteCallback(EventCallback cb) { write_callback_ = std::move(cb); }
	void SetCloseCallback(EventCallback cb) { close_callback_ = std::move(cb); }
	void SetErrorCallback(EventCallback cb) { error_callback_ = std::move(cb); }
	// 设置 fd 相应的事件状态
	void EnableReading() {
		events_ |= kReadEvent;
		Update(); // 调用 epoll_ctl
	}
    void EnableWriting(){
        events_ |= kWriteEvent;
        Update();
    }
    void DisableReading(){
        events_ &= ~kReadEvent;
        Update();
    }
    void DisableWriting(){
        events_ &= ~kWriteEvent;
        Update();
    }
    void DisableAll(){
        events_ = kNoneEvent;
        Update();
    }
    // 返回 fd 当前的事件状态
    bool IsNoneEvent() const {return events_ == kNoneEvent;}
    bool IsReadEvent() const {return events_ == kReadEvent;}
    bool IsWriteEvent() const {return events_ == kWriteEvent;}

    // Get/Set
    int GetIndex() {return index_;}
    void SetIndex(int idx) {index_ = idx;}
    int GetFd() const {return fd_;}
    int GetEvents() const {return events_;}
    void SetRevents(int revt){revents_ = revt;}
    // 防止当 Channel 被手动 remove 掉, Channel 还在执行回调操作
    void Tie(const std::weak_ptr<void>&);
    // 返回当前 Channel 所属的 EventLoop
    EventLoop* OwnerLoop(){return loop_;}
    // 在 Channel 所属的 EventLoop 中, 把当前的 Channel 删除掉
    void Remove();
private:
    // 更新监听的事件
    void Update();
    void HandleEventWithGuard(Timestamp receive_time);
private:
	// 事件状态
	static const int kNoneEvent;
	static const int kReadEvent;
	static const int kWriteEvent;

	EventLoop* loop_;  // 事件循环, channel 所属的 EveltLoop
	const int fd_;
	int events_;   // 监听的事件
	int revents_;  // 发生的事件
	int index_; // 在 IO 复用模块中的状态, 删除、添加

     // 绑定TcpConnection, 判断是否存活, 存在就执行回调, 否则就不执行
	std::weak_ptr<void> tie_;
	bool tied_; // 是否被标记过

	// 事件回调函数
	ReadEventCallback read_callback_;
	EventCallback write_callback_;
	EventCallback close_callback_;
	EventCallback error_callback_;
};