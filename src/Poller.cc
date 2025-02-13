#include "Poller.hh"
#include "Logger.hh"
#include "Channel.hh"

Poller::Poller(EventLoop* loop)
    :ownerLoop_(loop)
{
    LOG_INFO("Poller::Poller()");
}

bool Poller::hasChannel(Channel* channel) const{
    // 查找channel对应的文件描述符
    auto it=channels_.find(channel->fd());
    // 判断文件描述符是否存在于channels_中，并且对应的Channel对象是否相同
    return it!=channels_.end() && it->second==channel;
}

