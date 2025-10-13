#include "event_loop.h"

#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "current_thread.h"
#include "logger.h"
#include "poller.h"

// 防止一个线程创建多个 EventLoop
thread_local EventLoop *loop_in_this_thread = nullptr;
// 定义默认的 Poller IO 复用接口的超时时间
constexpr int kPollTimeMs = 10000;	// 10 s

// 创建 wakeup_fd, 用来 notify 唤醒 subLoop 处理新来的 Channel
/** 创建线程之后主线程和子线程谁先运行是不确定的。
 * 通过一个eventfd在线程之间传递数据的好处是多个线程无需上锁就可以实现同步。
 * eventfd支持的最低内核版本为Linux 2.6.27,在2.6.26及之前的版本也可以使用eventfd，但是flags必须设置为0。
 * 函数原型：
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * 参数说明：
 *      initval,初始化计数器的值。
 *      flags, EFD_NONBLOCK,设置socket为非阻塞。
 *             EFD_CLOEXEC，执行fork的时候，在父进程中的描述符会自动关闭，子进程中的描述符保留。
 * 场景：
 *     eventfd可以用于同一个进程之中的线程之间的通信。
 *     eventfd还可以用于同亲缘关系的进程之间的通信。
 *     eventfd用于不同亲缘关系的进程之间通信的话需要把eventfd放在几个进程共享的共享内存中（没有测试过）。
 */
int CreateEventFd() {
	int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (evtfd < 0) {
		LOG_FATAL("eventfd error: %d \n", errno);
	}

	return evtfd;
}

EventLoop::EventLoop()
	: looping_(false),
	  quit_(false),
	  thread_id_(CurrentThread::Tid()),
	  poller_(Poller::NewDefaultPoller(this)),
	  wakeup_fd_(CreateEventFd()),
	  wakeup_channel_(new Channel(this, wakeup_fd_)),
	  calling_pending_functors_(false) {
	if (loop_in_this_thread) {
		LOG_FATAL("Another EventLoop %p exists in this thread %d \n", loop_in_this_thread,
				  thread_id_);
	} else {
		loop_in_this_thread = this;
	}

	// 设置 wakeup_fd 的事件类型以及发生事件后的回调操作
	wakeup_channel_->SetReadCallback(std::bind(&EventLoop::HandleRead, this));
	// 每一个 EventLoop 都将监听 wakeup_channel 的 EPOLLIN 读事件
	wakeup_channel_->EnableReading();
}

EventLoop::~EventLoop() {
	wakeup_channel_->DisableAll();	// 给Channel移除所有感兴趣的事件
	wakeup_channel_->Remove();		// 把Channel从EventLoop上删除掉
	::close(wakeup_fd_);
	loop_in_this_thread = nullptr;
}

// 用来唤醒loop所在的线程的
// 向 wakeupfd_ 写一个数据，wakeup_channel 就发生读事件，当前 loop 线程就会被唤醒
void EventLoop::wakeup() {
	uint64_t one = 1;
	ssize_t n = write(wakeup_fd_, &one, sizeof(one));
	if (n != sizeof(one)) {
		LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n", n);
	}
}

// wake up 的回调函数
void EventLoop::HandleRead() {
	uint64_t one = 1;
	ssize_t n = read(wakeup_fd_, &one, sizeof(one));
	if (n != sizeof(one)) {
		LOG_ERROR("EventLoop::HandleRead reads %ld bytes instead of 8", n);
	}
}

// 开启事件循环
void EventLoop::Loop() {
	looping_ = true;
	quit_ = false;

	LOG_INFO("EventLoop %p start looping \n", this);

	while (!quit_) {
		active_channels_.clear();
		// 监听事件
		poll_return_time_ = poller_->Poll(kPollTimeMs, &active_channels_);
		// 执行回调函数
		for (Channel *channel : active_channels_) {
			// Poller 监听哪些 Channel 发生事件了, 然后上报给 EventLoop
			channel->HandleEvent(poll_return_time_);
		}

		/**
		 * 执行当前EventLoop事件循环需要处理的回调操作
		 * 对于线程数 >=2 的情况 IO线程 mainloop(mainReactor) 主要工作：
		 * 1.accept接收连接 => 将accept返回的connfd打包为Channel =>
		 *TcpServer::newConnection通过轮询将TcpConnection对象分配给subloop处理
		 *
		 * 2.mainloop调用queueInLoop将回调加入subloop（该回调需要subloop执行
		 *但subloop还在poller_->poll处阻塞） queueInLoop通过wakeup将subloop唤醒
		 **/
		// 执行任务队列中的任务，这些任务可能是线程池内的IO操作因为不能跨线程
		// 所以被转移到Reactor线程
		DoPendingFunctors();
	}

	LOG_INFO("EventLoop %p stop looping \n", this);

	looping_ = false;
}

// 退出事件循环 1. loop 在自己的线程中调用 Quit 2. 在非 loop 的西安测绘给你中调用 loop 的
void EventLoop::Quit() {
	quit_ = true;
	// 如果在其它线程中调用的 Quit, 唤醒对应的 loop
	if (!IsInLoopThread()) {
		wakeup();
	}
}

// 在当前的 EventLoop 中执行 cb
void EventLoop::RunInLoop(Functor cb) {
	// 这里如果是本线程内操作，就直接执行
	if (IsInLoopThread()) {
		cb();
	} else {  // 如果是跨线程操作，就放入队列, 就需要唤醒 loop 所在线程, 执行 cb
		QueueInLoop(std::move(cb));
	}
}

// 把 cb 放入队列中, 唤醒 EventLoop 所在的线程, 执行 cb
void EventLoop::QueueInLoop(Functor cb) {
	{
		std::unique_lock<std::mutex> lock(mutex_);
		pending_functors_.emplace_back(std::move(cb));
	}

	// 唤醒相应的需要执行上面回调操作的 loop 的线程
	// 跨线程或者
	// || calling_pending_functors_ 的作用是: 当前 EventLoop 正在执行回调, 但是 EventLoop
	// 又有了新的回调, 使得新加入的回调能够被处理, 避免延迟
	if (!IsInLoopThread() || calling_pending_functors_) {
		wakeup();  // 唤醒 loop 所在线程
	}
}

// 通过 EventLoop 的方法 调用 Poller 的方法
void EventLoop::UpdateChannel(Channel *channel) { poller_->UpdateChannel(channel); }

void EventLoop::RemoveChannel(Channel *channel) { poller_->RemoveChannel(channel); }

// 判断参数 channel 是否在当前 Poller 中
bool EventLoop::HasChannel(Channel *channel) { return poller_->HasChannel(channel); }

// 执行回调
void EventLoop::DoPendingFunctors() {
	std::vector<Functor> functors;
	calling_pending_functors_ = true;

	{
		std::unique_lock<std::mutex> lock(mutex_);
		// 交换的方式减少了锁的临界区范围 提升效率 同时避免了死锁
		// 如果执行functor()在临界区内 且functor()中调用queueInLoop()就会产生死锁
		functors.swap(pending_functors_);
	}

	for (const Functor &functor : functors) {
		functor();// 执行当前loop需要执行的回调操作
	}

	calling_pending_functors_ = false;
}