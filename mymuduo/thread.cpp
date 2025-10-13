#include "thread.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include "current_thread.h"
#include <semaphore.h>

std::atomic<int> Thread::num_created_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
	: started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name) {
	SetDefaultName();
}

Thread::~Thread(){
    if (started_ && !joined_){
        // 分离线程, 使其变为守护线程, 之后会自动回收
        thread_->join();
    }
}

// 启动线程, 一个线程对象记录的就是一个新线程的详细信息
void Thread::Start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 开启线程
    thread_ = std::make_shared<std::thread>([&](){
        // 获取线程的 tid 值
        tid_ = CurrentThread::Tid();
        // 通知信号量
        sem_post(&sem);
        // 开启一个新线程, 专门执行该线程函数
        func_();
    });

    // 必须等待获取上面新创建的线程的 tid 值
    sem_wait(&sem);
}

void Thread::Join(){
    joined_ = true;
    thread_->join();
}

// 设置默认线程名称
void Thread::SetDefaultName(){
    int num = ++num_created_;
    if (name_.empty()){
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}