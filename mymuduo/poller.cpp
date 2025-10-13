#include "poller.h"
#include "channel.h"

Poller::Poller(EventLoop* loop): owner_loop_(loop){}


// 判断参数 channel 是否在当前 Poller 中
bool Poller::HasChannel(Channel* channel) const{
    auto it = channels_.find(channel->GetFd());

    return it != channels_.end() && it->second == channel;
}