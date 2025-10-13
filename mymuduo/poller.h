#pragma once

#include "noncopyable.h"
#include "timestamp.h"
#include <unordered_map>
#include <vector>

class Channel;
class EventLoop;

// 多路事件分发器的基类
class Poller : Noncopyable {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;
    // 给所有 IO 复用保留统一的接口
    virtual Timestamp Poll(int timeout_ms, ChannelList* active_channels) = 0;
    // 更新 channel 在 poller 中的事件
    virtual void UpdateChannel(Channel* channel) = 0;
    virtual void RemoveChannel(Channel* channel) = 0;
    // 判断参数 channel 是否在当前 Poller 中
    bool HasChannel(Channel* channel) const;
    // EventLoop 可以通过该接口获取默认的 IO 复用的具体实现
    static Poller* NewDefaultPoller(EventLoop* loop);
protected:
    // key: fd; value: 对应的 channel*
    using ChannelMap = std::unordered_map<int, Channel*>;

    ChannelMap channels_; // 保存监听的fd对应的Channel指针
private:
    EventLoop *owner_loop_; // Poller 所属的事件循环 EventLoop
};