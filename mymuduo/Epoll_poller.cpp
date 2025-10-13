#include "Epoll_poller.h"

#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "event_loop.h"
#include "logger.h"
#include "poller.h"
#include "timestamp.h"

// Channel 的状态
constexpr int kNew = -1;  // 某个channel还没添加至Poller, channel的成员index_初始化为-1
constexpr int kAdded = 1;	 // 某个channel已经添加至Poller
constexpr int kDeleted = 2;	 // 某个channel已经从Poller删除

EpollPoller::EpollPoller(EventLoop* loop)
	: Poller(loop),
	  epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)),
	  events_(kInitEventListSize) {
	if (epoll_fd_ < 0) {
		LOG_FATAL("epoll_create1 error:%d\n", errno);
	}
}

EpollPoller::~EpollPoller() { ::close(epoll_fd_); }

// 调用 epoll_wait
// timeout_ms: 超时时间
// active_channels: 传出参数, 哪些 Channel 被触发了
// returns: 发生事件的时间点
Timestamp EpollPoller::Poll(int timeout_ms, ChannelList* active_channels) {
	int num_events = ::epoll_wait(epoll_fd_, &(*events_.begin()),
								  static_cast<int>(events_.size()), timeout_ms);
	int save_errno = errno;
	Timestamp time_now(Timestamp::Now());

	if (num_events > 0) {
		LOG_INFO("%d events happened \n", num_events);
		// 将发生事件的 Channel 返回给 EventLoop
		FillActiveChannels(num_events, active_channels);
		// 扩容
		if (num_events == events_.size()) {
			events_.resize(events_.size() * 2);
		}
	} else if (num_events == 0) {
		LOG_DEBUG("%s timeout! \n", __FUNCTION__);
	} else {
		if (save_errno == EINTR) {
			errno = save_errno;
			LOG_ERROR("EpollPoller::Poll() err!");
		}
	}

	return time_now;
}

// 填写活跃的连接
void EpollPoller::FillActiveChannels(int num_events, ChannelList* active_channels) const {
	for (int i = 0; i < num_events; ++i) {
		Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
		// 设置当前 Channel 发生的事件
		channel->SetRevents(events_[i].events);
		active_channels->emplace_back(channel);
	}
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel
// removeChannel 更新当前监听的 Channel 的状态
void EpollPoller::UpdateChannel(Channel* channel) {
	// 获取该 Channel 在该 epoll 中的状态
	int index = channel->GetIndex();
	if (index == kNew || index == kDeleted) {
		if (index == kNew) {
			int fd = channel->GetFd();
			// 添加监听的 Channel
			channels_[fd] = channel;
		}

		channel->SetIndex(kAdded);
		// 调用 epoll_ctl
		Update(EPOLL_CTL_ADD, channel);
	} else {
		int fd = channel->GetFd();
		if (channel->IsNoneEvent()) {
			Update(EPOLL_CTL_DEL, channel);
			// 标记删除
			channel->SetIndex(kDeleted);
		} else {
			Update(EPOLL_CTL_MOD, channel);
		}
	}
}

// 将监听的 Channel 删除
void EpollPoller::RemoveChannel(Channel* channel) {
	// 从 channelMap 中删除
	int fd = channel->GetFd();
	channels_.erase(fd);
	// 从 poller 中删除
	int index = channel->GetIndex();
	if (index == kAdded) {
		Update(EPOLL_CTL_DEL, channel);
	}
	channel->SetIndex(kNew);
}

// 更新 channel 通道
void EpollPoller::Update(int operation, Channel* channel) {
	epoll_event event;
	bzero(&event, sizeof(event));
	event.events = channel->GetEvents();
	event.data.ptr = channel;
	int fd = channel->GetFd();

	if (::epoll_ctl(epoll_fd_, operation, fd, &event) < 0) {
		if (operation == EPOLL_CTL_DEL) {
			LOG_ERROR("epoll_ctl del error:%d\n", errno);
		} else {
			LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
		}
	}
}