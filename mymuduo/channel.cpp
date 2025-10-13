#include "channel.h"

#include <sys/epoll.h>
#include <memory>
#include "timestamp.h"
#include "event_loop.h"

// 事件状态
const int Channel::kNoneEvent = 0;
// 读数据和 OOB 数据
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
// 写数据
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
	: loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel(){}

// 防止当 Channel 被手动 remove 掉, Channel 还在执行回调操作
void Channel::Tie(const std::weak_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}


// 更新监听的事件
// channel -> EventLoop -> poller
void Channel::Update(){
    // 通过 Channel 所属的 EventLoop 调用 poller 的相应方法修改 fd 的监听事件
    loop_->UpdateChannel(this);
}

// 在 Channel 所属的 EventLoop 中, 把当前的 Channel 删除掉
void Channel::Remove(){
    loop_->RemoveChannel(this);
}

// 事件回调
// receice_time: 发生事件的时间点
void Channel::HandleEvent(Timestamp receive_time){
    if (tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if (guard){
            HandleEventWithGuard(receive_time);
        }
    } else {
        HandleEventWithGuard(receive_time);
    }
}

// 实际的回调函数
void Channel::HandleEventWithGuard(Timestamp receive_time){
    // 对端关闭且没有读事件
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)){
        if (close_callback_){
            close_callback_();
        }
    }

    // 错误事件
    if (revents_ & EPOLLERR){
        if (error_callback_){
            error_callback_();
        }
    }

    // 可读或有紧急数据可读事件
    if (revents_ & (EPOLLIN | EPOLLPRI)){
        if (read_callback_){
            read_callback_(receive_time);
        }
    }

    // 可写事件
    if (revents_ & EPOLLOUT){
        if (write_callback_){
            write_callback_();
        }
    }
}