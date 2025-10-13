#pragma once

#include <sys/epoll.h>
#include <vector>
#include "channel.h"
#include "event_loop.h"
#include "poller.h"
#include "timestamp.h"

class EpollPoller : public Poller{
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    Timestamp Poll(int timeout_ms, ChannelList* active_channels) override;
    void UpdateChannel(Channel* channel) override;
    void RemoveChannel(Channel* channel) override;
private:
    // 填写活跃的连接
    void FillActiveChannels(int num_events, ChannelList* active_channels) const;
    // 更新 channel 通道
    void Update(int operation, Channel* channel);
private:
    using EventList = std::vector<epoll_event>;

    // 初始的存储发生事件的数量
    static const int kInitEventListSize = 16;

    int epoll_fd_;
    EventList events_;// 发生事件的列表
};