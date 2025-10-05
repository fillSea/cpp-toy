# 1. 概述
本项目实现了两种模式的线程池，分别是固定线程数模式和动态线程数模式。
用户可以根据需要选择不同的线程池模式，以满足不同的并发需求。
线程池类型：
- fixed 模式线程池：线程池里面的线程个数是固定不变的，一般是 ThreadPool 创建时根据当前机器的 CPU 核心数量进行指定。
- cached 模式线程池：线程池里面的线程个数是可动态增长的，根据任务的数量动态的增加线程的数量，但是会设置一个线程数量的阈值，任务处理完成，如果动态增长的线程空闲了 60 s 还没有处理其它任务，那么关闭线程，保持池中最初数量的线程即可。
# 2. 架构
![架构图](images/(施磊)线程池.png)
# 3. 功能
- 支持固定线程数模式和动态线程数模式
- 支持任务提交和执行
- 支持线程池的销毁
# 4. 测试代码
```cpp
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>
#include "thread_pool.h"

using ULL = unsigned long long;

ULL Run(ULL begin_, ULL end_){
        std::cout << "tid: " << std::this_thread::get_id() << " begin" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ULL sum = 0;
        for (ULL i = begin_; i <= end_; ++i){
            sum += i;
        }
        std::cout << "tid: " << std::this_thread::get_id() << " end" << std::endl;
        return sum;
    }

int main(){
    ThreadPool pool;
    pool.SetMode(PoolMode::MODE_CACHED);
    pool.Start();

    auto res1 = pool.SubmitTask(Run, 1, 100000);
    auto res2 = pool.SubmitTask(Run, 100001, 200000);
    auto res3 = pool.SubmitTask(Run, 200001, 300000);
    pool.SubmitTask(Run, 200001, 300000);
    pool.SubmitTask(Run, 200001, 300000);

    auto sum1 = res1.get();
    auto sum2 = res2.get();
    auto sum3 = res3.get();
    std::cout << "sum1: " << sum1 << std::endl;
    std::cout << "sum2: " << sum2 << std::endl;
    std::cout << "sum3: " << sum3 << std::endl;
    std::cout << "sum: " << sum1 + sum2 + sum3 << std::endl;


    ULL sum = 0;
    for (ULL i = 0; i <= 300000; ++i){
        sum += i;
    }

    std::cout << "check sum: " << sum << std::endl;

    getchar();

    return 0;
}
```