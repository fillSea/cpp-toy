#include "event_loop_thread.h"

#include <functional>
#include <mutex>
#include <string>

#include "event_loop.h"
#include "thread.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
	: loop_(nullptr),
	  exiting_(false),
	  thread_(std::bind(&EventLoopThread::ThreadFunc, this), name),
	  callback_(cb) {}

// 此EventLoop的退出流程：
// 1.EventLoopThread对象过期
// 2.如果loop_不为NULL，说明正在执行（因为loop初始为NULL，一旦运行起来，就再也不是NULL）
// 需要将内部的EventLoop关闭
// 3.join线程
EventLoopThread::~EventLoopThread() {
	exiting_ = true;
	if (loop_) {
		loop_->Quit();
		thread_.Join();
	}
}

EventLoop* EventLoopThread::StartLoop() {
	thread_.Start();  // 启用底层线程Thread类对象thread_中通过start()创建的线程

	EventLoop* loop = nullptr;

	// 这里存在一个race Condition
	// 线程刚刚启动时，里面的EventLoop的指针还没有赋给loop_，所以
	// 这里需要等待，当指针赋值成功后，cond_会通知这里继续执行
	{
		std::unique_lock<std::mutex> lock(mutex_);
		// 等待新线程创建的 EventLoop 对象初始化完成
		cond_.wait(lock, [&]() { return loop_ != nullptr; });

		loop = loop_;
	}

	return loop;
}

// 开启的新线程的执行逻辑
void EventLoopThread::ThreadFunc() {
	// 创建一个独立的 EventLoop, 和线程一一对应, one loop per thread
	// 因为EventLoopThread运行在一个线程中，而
	// 本函数又代表了线程的全部执行逻辑，所以这里将loop作为
	// 一个栈对象，线程执行结束后，会自动销毁
	EventLoop loop;

    // 执行线程的初始化回调函数，主要与loop有关
	if (callback_) {
		callback_(&loop);
	}

    // 这里主要是消除race condition 防止上面的startLoop返回一个空指针
	{
		std::unique_lock<std::mutex> lock(mutex_);
		loop_ = &loop;
		cond_.notify_one();
	}

	// EventLoop->Loop() => Pooler->Poll()
	loop.Loop();
	std::unique_lock<std::mutex> lock(mutex_);
	loop_ = nullptr;
}