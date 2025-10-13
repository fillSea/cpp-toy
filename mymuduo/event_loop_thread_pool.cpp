#include "event_loop_thread_pool.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "event_loop.h"
#include "event_loop_thread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* base_loop,
										 const std::string& name_arg)
	: base_loop_(base_loop),  // 从外界接收一个loop
	  name_(name_arg),
	  started_(false),
	  num_threads_(0),
	  next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
	// 这里无需delete loop，因为他们都是栈上的对象，
	// 从EventLoopThread的源码中可以看到，loop是一个局部对象，
	// 无须手工delete。
	// 注意这个线程池如果析构：
	// 1.成员对象threads_过期，需要析构
	// 2.threads_析构导致内部的每个EventLoopThread被析构
	// 3.每个线程内的EventLoop被执行loop
	// 4.每个线程被join
	// 5.所有的EventLoop以及所在线程全部销毁
	// 6.本线程池不需要做任何任务
}

// 启动线程池
void EventLoopThreadPool::Start(const ThreadInitCallback& cb) {
	started_ = true;
	for (int i = 0; i < num_threads_; ++i) {
		std::string thread_name = name_ + std::to_string(i);
		EventLoopThread* t = new EventLoopThread(cb, thread_name);
		threads_.emplace_back(std::unique_ptr<EventLoopThread>(t));
		// 底层创建线程, 绑定一个新的 EventLoop, 并返回该 loop 的地址
		loops_.emplace_back(t->StartLoop());
	}

	// 整个服务端只有一个线程运行着 baseLoop
	if (num_threads_ == 0 && cb) {
		cb(base_loop_);
	}
}

// 如果工作在多线程中, base_loop_ 默认以轮询的方式分配 Channel 给 SubLoop
EventLoop* EventLoopThreadPool::GetNextLoop() {
	EventLoop* loop = base_loop_;
	if (!loops_.empty()) {
		loop = loops_[next_];
        ++next_;
		if (next_ >= loops_.size()) {
			next_ = 0;
		}
	}

	return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::GetAllLoops() {
	if (loops_.empty()) {
		return std::vector<EventLoop*>(1, base_loop_);
	} else {
		return loops_;
	}
}