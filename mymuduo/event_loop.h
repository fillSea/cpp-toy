#pragma once

#include <sys/types.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "channel.h"
#include "current_thread.h"
#include "noncopyable.h"
#include "poller.h"
#include "timestamp.h"

// 事件循环类, 主要包含两大模块 Channel、Poller(epoll的抽象)
// 调用 Poller 监听事件, 之后调用 Channel::HandleEvent() 处理相应的事件
// Poller 和 Channel 通过 EventLoop 进行交互
class EventLoop : Noncopyable {
public:
	// 回调函数类型
	using Functor = std::function<void()>;

	EventLoop();
	~EventLoop();

	// 开启事件循环
	void Loop();
	// 退出事件循环
	void Quit();
	// 在当前的 EventLoop 中执行 cb
	void RunInLoop(Functor cb);
	// 把 cb 放入队列中, 唤醒 EventLoop 所在的线程, 执行 cb
	void QueueInLoop(Functor cb);

	// 通过 EventLoop 的方法 调用 Poller 的方法
	void UpdateChannel(Channel *channel);
	void RemoveChannel(Channel *channel);
	// 判断参数 channel 是否在当前 Poller 中
	bool HasChannel(Channel *channel);

	// 判断 EventLoop 对象是否在自己的线程里面
	bool IsInLoopThread() const { return thread_id_ == CurrentThread::Tid(); }
	// 返回 poller 发生事件的时间点
	Timestamp PollReturnTime() const { return poll_return_time_; }
	// 唤醒 EventLoop 所在的线程
	void wakeup();

private:
	// 给eventfd返回的文件描述符 wakeup_fd_ 绑定的事件回调, 当wakeup()时 即有事件发生时
	// 调用handleRead()读wakeupFd_的8字节 同时唤醒阻塞的epoll_wait
	void HandleRead();		   // wake up 的回调函数
	void DoPendingFunctors();  // 执行上层回调
private:
	using ChannelList = std::vector<Channel *>;

	std::atomic<bool> looping_;// 是否正在执行 loop 循环
	std::atomic<bool> quit_;  // 标识退出 loop 循环

	const pid_t thread_id_;	 // 当前 loop 所在线程 id

	Timestamp poll_return_time_;  // poller 返回发生事件的 channels 的时间点
	std::unique_ptr<Poller> poller_;  // 指向 Poller

	// 当 mainLoop 获取一个新用户的 channel, 通过轮询选择一个 subLoop, 通过该成员唤醒该
	// subLoop进行处理
	int wakeup_fd_;
	std::unique_ptr<Channel> wakeup_channel_;

	ChannelList active_channels_;	   // 发生事件的 channel 集合

	// 标识当前 loop 是否有需要执行的回调函数
	std::atomic<bool> calling_pending_functors_;
	std::vector<Functor> pending_functors_;	 // 存储 loop 需要执行的所有的回调操作
	std::mutex mutex_;	// 互斥锁, 用来保护对 std::vector 的线程安全
};