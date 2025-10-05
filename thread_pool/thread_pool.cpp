#include "thread_pool.h"

#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

// 定义默认值
constexpr int kTaskQueMaxSize = 1024;// 任务队列上限阈值
constexpr int kThreadMaxSize = 300;// 线程池线程数量上限阈值
constexpr int kThreadMaxIdleTime = 60;	// 单位: 秒， 线程最大空闲时间阈值


//------------------------------ Thread 实现
std::atomic<int> Thread::generate_thread_id_(0);

Thread::Thread(ThreadFunc func) : func_(std::move(func)), thread_id_(generate_thread_id_++) {}

Thread::~Thread() {}

// 启动线程
void Thread::Start() {
	// 创建一个线程来执行线程函数
	std::thread t(func_, thread_id_);
	t.detach();	 // 分离线程, 线程执行完后自动销毁
}

// 获取线程 id
int Thread::GetId() const { return thread_id_; }


//-------------------------------- ThreadPool 实现
ThreadPool::ThreadPool()
	: init_thread_size_(0),
	  current_thread_size_(0),
	  idle_thread_size_(0),
	  thread_max_size_(kThreadMaxSize),
	  task_size_(0),
	  task_que_max_size_(kTaskQueMaxSize),
	  pool_mode_(PoolMode::MODE_FIXED),
	  is_running_(false) {}

ThreadPool::~ThreadPool() {
	is_running_ = false;
	// 等待线程池里面所有的线程返回, 有两种状态: 阻塞 & 正在执行
	std::unique_lock<std::mutex> lock(task_que_mtx_);
	not_empty_.notify_all();
	exit_cond_.wait(lock, [&]() { return threads_.empty(); });
}


// 设置线程池的工作模式
void ThreadPool::SetMode(PoolMode mode) {
	if (CheckRunningState()) {
		return;
	}
	pool_mode_ = mode;
}

// 设置 task 任务队列上限阈值
void ThreadPool::SetTaskQueMaxSize(size_t max_size) {
	if (CheckRunningState()) {
		return;
	}
	task_que_max_size_ = max_size;
}

// 设置线程池线程数量上限阈值
void ThreadPool::SetThreadMaxSize(size_t max_size) {
	if (CheckRunningState()) {
		return;
	}

	thread_max_size_ = max_size;
}

// 开启线程池
void ThreadPool::Start(size_t init_thread_size) {
	is_running_ = true;
	init_thread_size_ = std::min(init_thread_size, thread_max_size_);
	current_thread_size_ = init_thread_size_;
	// 创建线程, 传递线程函数 ThreadFunc
	for (size_t i = 0; i < init_thread_size_; ++i) {
		auto ptr = std::make_unique<Thread>(
			std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
		int id = ptr->GetId();
		threads_.emplace(id, std::move(ptr));
	}
	// 启动所有线程
	for (size_t i = 0; i < init_thread_size_; ++i) {
		threads_[i]->Start();
		++idle_thread_size_;
	}
}

// 定义线程函数, 从任务队列中消费任务
void ThreadPool::ThreadFunc(int thread_id) {
	auto last_time =
		std::chrono::high_resolution_clock().now();	 // 记录线程上一次执行的时间
	while (true) {
		Task task;
		{
			// 获取锁
			std::unique_lock<std::mutex> lock(task_que_mtx_);
			while (task_que_.empty()) {
				// 线程池停止
				if (!is_running_) {
					threads_.erase(thread_id);
					exit_cond_.notify_all();
					return;
				}
				// cached 模式下, 可能已经创建了很多线程, 但是空闲时间超过 60s,
				// 应该把多余的线程结束回收掉 当前时间 - 上一次线程执行的时间 > 60s
				if (pool_mode_ == PoolMode::MODE_CACHED) {
					// 超时返回
					if (std::cv_status::timeout ==
						not_empty_.wait_for(lock, std::chrono::seconds(1))) {
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(
							now - last_time);
						if (dur.count() >= kThreadMaxIdleTime &&
							current_thread_size_ > init_thread_size_) {
							// 回收当前线程
							// 记录线程数量的相关变量修改
							// 把线程对象从线程列表容器中删除
							threads_.erase(thread_id);
							--current_thread_size_;
							--idle_thread_size_;
							return;	 // 该线程函数结束执行, 则当前线程会自动回收
						}
					}
				} else {
					// 等待任务队列有任务
					not_empty_.wait(lock);
				}
			}
			--idle_thread_size_;
			// 从任务队列中取任务
			task = std::move(task_que_.front());
			task_que_.pop();
			--task_size_;
			// 如果有剩余任务, 通知其它线程可以执行
			if (!task_que_.empty()) {
				not_empty_.notify_all();
			}
			// 通知任务队列可以继续提交任务
			not_full_.notify_all();
		}

		// 当前线程执行任务
		if (task != nullptr) {
			task();  // 需要获取任务的返回值, 并设置到 Result 对象中
		}
		++idle_thread_size_;
		last_time = std::chrono::high_resolution_clock().now();
	}
}



// 检查线程池是否正在运行
bool ThreadPool::CheckRunningState() const { return is_running_; }



