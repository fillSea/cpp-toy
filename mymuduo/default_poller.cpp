#include <cstdlib>
#include "Epoll_poller.h"
#include "poller.h"

// EventLoop 可以通过该接口获取默认的 IO 复用的具体实现
Poller* Poller::NewDefaultPoller(EventLoop* loop){
    if (getenv("MUDUO_USE_POOL")){
        return nullptr; // 生成 poll  的实例
    } else {
        return new EpollPoller(loop); // 生成 epoll 的实例
    }
}