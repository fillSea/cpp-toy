#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "event_loop.h"
#include "event_loop_thread.h"
#include "noncopyable.h"

// 前面的EventLoop和EventLoopThread都是单线程Reactor，区别仅仅是在当前
// 线程内执行loop还是新开一个线程而已。
// 这里的EventLoopThreadPool则是新开一个线程池，池内的每个Thread均执行一个
// EventLoop，另外EventLoopThreadPool从外面接收一个loop作为master
// 它与pool内的EventLoop是一个主从关系
class EventLoopThreadPool : Noncopyable {
public:
    // 线程初始回调函数
	using ThreadInitCallback = std::function<void(EventLoop*)>;

	EventLoopThreadPool(EventLoop* base_loop, const std::string& name_arg);
	~EventLoopThreadPool();

	void Start(const ThreadInitCallback& cb = ThreadInitCallback());
	
    // 如果工作在多线程中, base_loop_ 默认以轮询的方式分配 Channel 给 SubLoop
	EventLoop* GetNextLoop();
    // 获取所有的EventLoop
	std::vector<EventLoop*> GetAllLoops();

	void SetThreadNum(int num_threads) { num_threads_ = num_threads; }
    bool GetStarted() const { return started_; }
	const std::string GetName() const { return name_; }
private:
	EventLoop* base_loop_;	// main EventLoop
	// 线程池名称，通常由用户指定，线程池中EventLoopThread名称依赖于线程池名称
	std::string name_;
	bool started_;											 // 是否开始
	int num_threads_;										 // 线程数量
	int next_;												 // 索引
	std::vector<std::unique_ptr<EventLoopThread>> threads_;	 // IO线程列表
	// 线程池中EventLoop的列表，指向的是EVentLoopThread线程函数创建的EventLoop对象
	std::vector<EventLoop*> loops_;
};