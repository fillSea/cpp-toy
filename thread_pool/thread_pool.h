#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <utility>

// 线程池的工作模式
enum class PoolMode {
	MODE_FIXED,	 // 固定线程数量
	MODE_CACHED	 // 线程数量可动态增长
};

// 线程类型
class Thread {
public:
	// 线程函数对象类型
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func);
	~Thread();
	// 启动线程
	void Start();
	// 获取线程 id
	int GetId() const;

private:
	// 线程函数对象
	ThreadFunc func_;
	static std::atomic<int> generate_thread_id_;  // 静态成员变量, 用于生成线程 id
	int thread_id_;								  // 线程 id
};

// 线程池类型
class ThreadPool {
	using Task = std::function<void()>;

public:
	ThreadPool();
	~ThreadPool();
	// 设置线程池的工作模式
	void SetMode(PoolMode mode);
	// 开启线程池
	void Start(size_t init_thread_size = std::thread::hardware_concurrency());
	// 设置 task 任务队列上限阈值
	void SetTaskQueMaxSize(size_t max_size);
	// 设置线程池线程数量上限阈值
	void SetThreadMaxSize(size_t max_size);
	// 给线程池提交任务
	template <typename Func, typename... Args>
	auto SubmitTask(Func&& func, Args&&... args)
		-> std::future<decltype(std::forward<Func>(func)(std::forward<Args>(args)...))> {
		// 打包任务, 放入任务队列
		using RetType = decltype(std::forward<Func>(func)(std::forward<Args>(args)...));

		// 线程池未运行
		if (!CheckRunningState()) {
			std::cerr << "thread pool is not running, submit task fail." << std::endl;
			auto task_fail = std::make_shared<std::packaged_task<RetType()>>(
				[]() -> RetType { return RetType(); });

			(*task_fail)();

			return task_fail->get_future();
		}

		auto task = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RetType> result = task->get_future();

		// 获取锁
		std::unique_lock<std::mutex> lock(task_que_mtx_);
		// 等待任务队列有空余
		if (!not_full_.wait_for(lock, std::chrono::seconds(1), [&]() {
				return task_que_.size() < task_que_max_size_;
			})) {
			std::cerr << "task queue is full, submit task fail." << std::endl;
			auto task_fail = std::make_shared<std::packaged_task<RetType()>>(
				[]() -> RetType { return RetType(); });

			(*task_fail)();

			return task_fail->get_future();
		}

		// 将任务放入任务队列
		task_que_.emplace([task]() { (*task)(); });
		++task_size_;

		// 通知线程池有任务了
		not_empty_.notify_one();

		// cached 模式, 动态增加新线程
		if (pool_mode_ == PoolMode::MODE_CACHED && task_size_ > idle_thread_size_ &&
			current_thread_size_ < thread_max_size_) {
			// 创建新线程
			auto ptr = std::make_unique<Thread>(
				std::bind(&ThreadPool::ThreadFunc, this, std::placeholders::_1));
			int id = ptr->GetId();
			threads_.emplace(id, std::move(ptr));
			threads_[id]->Start();
			++current_thread_size_;
			++idle_thread_size_;
		}

		// 返回任务的 Result 对象
		return result;
	}
	// 禁止拷贝
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 定义线程函数
	void ThreadFunc(int thread_id);
	// 检查线程池是否正在运行
	bool CheckRunningState() const;

private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;	// 线程列表
	size_t init_thread_size_;									// 初始线程数量
	std::atomic<unsigned int> current_thread_size_;				// 当前线程池中的线程数量
	std::atomic<unsigned int> idle_thread_size_;				// 记录空闲线程的数量
	size_t thread_max_size_;									// 线程池线程数量上限阈值

	std::queue<Task> task_que_;			   // 任务队列
	std::atomic<unsigned int> task_size_;  // 任务队列中的任务数量
	size_t task_que_max_size_;			   // 任务队列数量上限阈值
	std::mutex task_que_mtx_;			   // 保证任务队列的线程安全

	std::condition_variable not_full_;	 // 任务队列不满条件变量
	std::condition_variable not_empty_;	 // 任务队列不空条件变量

	PoolMode pool_mode_;				 // 当前线程池的工作模式
	std::atomic<bool> is_running_;		 // 线程池的启动状态
	std::condition_variable exit_cond_;	 // 等待线程资源全部回收
};

#endif