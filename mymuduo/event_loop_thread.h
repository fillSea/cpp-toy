#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "event_loop.h"
#include "noncopyable.h"
#include "thread.h"

// 之前的EventLoop都是占据了一个线程，但是有些情况下，我们需要使用
// 一个单独的线程去执行EventLoop，例如TCP的client，我们可能需要在
// 当前线程中读取stdin，所以希望EventLoop在一个新的线程中运行
// 这就是EventLoopThread产生的必要性
class EventLoopThread : Noncopyable {
public:
	// 线程初始化回调函数
	using ThreadInitCallback = std::function<void(EventLoop*)>;

	EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
					const std::string& name = std::string());
	~EventLoopThread();

	EventLoop* StartLoop();
private:
	void ThreadFunc();// 线程的回调函数
private:
	EventLoop* loop_;				// 本对象拥有的EventLoop的指针
	bool exiting_;					// 是否正在退出
	Thread thread_;					// 线程，在内部执行EventLoop
	std::mutex mutex_;				// 互斥锁
	std::condition_variable cond_;	// 条件变量
	ThreadInitCallback callback_;	// 线程初始时的回调函数，只执行一次
};