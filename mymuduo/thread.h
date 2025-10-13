#pragma once

#include <sched.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include "noncopyable.h"

class Thread : Noncopyable{
public:
    // 线程函数
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc, const std::string& name = std::string());
    ~Thread();

    // 启动线程
    void Start();
    // join()线程
    void Join();

    // get/set
    bool GetStarted() const { return started_;}
    pid_t GetTid() const {return tid_;};
    const std::string& GetName() const {return name_;}
    static int GetNumCreated() {return num_created_;}
private:
    // 设置默认线程名称
    void SetDefaultName();
private:
    bool started_; // 是否开始
    bool joined_; // 是否 join()
    // 线程对象
    std::shared_ptr<std::thread> thread_;
    pid_t tid_; // 线程 Id, 在线程创建时再绑定
    ThreadFunc func_; // 线程执行函数
    std::string name_; // 线程名称
    static std::atomic<int> num_created_; // 创建的线程数量
};