#include"EpollPoller.hh"
#include"Logger.hh"
#include<unistd.h>
#include"Channel.hh"
#include"Timestamp.hh"
#include <strings.h>
// Channel的状态
const int kNew=-1;      // 新创建的Channel,还未添加到epoll中
const int kAdded=1;     // Channel已经添加到epoll中
const int kDeleted=2;   // Channel已从epoll中删除

EpollPoller::EpollPoller(EventLoop* loop)
    :Poller(loop),events_(kInitEventListSize)
{
    // EPOLL_CLOEXEC标志使得在exec()调用时自动关闭epollfd
    //    - 防止fd泄露到子进程
    //    - 符合"fork+exec"的最佳实践
    epollfd_=::epoll_create1(EPOLL_CLOEXEC);
    if(epollfd_<0)
    {
        LOG_FATAL("EpollPoller::EpollPoller()");
    }
}

EpollPoller::~EpollPoller()
{
    ::close(epollfd_);
}
//channel update remove => EventLoop updateChannel removeChannel => EpollPoller updateChannel removeChannel
/*
                 EventLoop =》poller.poll()
        ChannelList     Poller
                            ChannelMap  <fd,Channel*>
*/
void EpollPoller::fillActiveChannels(int numEvents,ChannelList* activeChannels) const
{
    for(int i=0;i<numEvents;++i)
    {
        // 获取事件对应的Channel    
        Channel* channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);//EventLoop就拿到了它的poller给它返回的活跃事件的channel列表

    }
}
// poll方法返回Timestamp的原因:
// 1. 记录事件发生的精确时间点
//    - 当epoll_wait返回时,说明有事件发生
//    - 通过返回当前时间戳,可以知道事件的具体发生时间
// 2. 用于上层回调函数
//    - Channel的ReadEventCallback需要时间戳参数
//    - 可以用于计算事件处理延迟、统计等
// 3. 保持接口统一
//    - Poller基类定义了返回Timestamp的接口
//    - 所有IO复用的实现都需要返回事件发生时间
Timestamp EpollPoller::poll(int timeoutMs,ChannelList* activeChannels)
{
    LOG_DEBUG("EpollPoller::poll()");
    // events_是vector<epoll_event>类型,begin()返回一个迭代器
    // 对迭代器解引用(*events_.begin())得到第一个元素
    // 再取地址(&*)就得到了第一个元素的地址,也就是数组的起始地址
    // 这样做是因为vector的迭代器不一定就是指针,
    // 通过&*可以保证获取真实的内存地址
    int numEvents=::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int savedErrno=errno;
    Timestamp now(Timestamp::now());
    if(numEvents>0)
    {
        LOG_DEBUG("%d events happened",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(static_cast<size_t>(numEvents)==events_.size())
        {
            events_.resize(events_.size()*2);
        }
    }
    else if(numEvents==0)
    {
        LOG_DEBUG("nothing happened");
    }
    else
    {
        if(savedErrno!=EINTR)
        {
            errno=savedErrno;
            LOG_ERROR("EpollPoller::poll()");
        }
    }
    return now;
}
void EpollPoller::updateChannel(Channel* channel)
{
     int state=channel->index();
     LOG_INFO("fd=%d events=%d state=%d",channel->fd(),channel->events(),state);
     if(state==kNew || state==kDeleted)
     {
        int fd=channel->fd();
        if(state==kNew)
        {
            channels_[fd]=channel;
        }
       channel->set_index(kAdded);
       update(EPOLL_CTL_ADD,channel);
     }
     else
     {
        int fd=channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);  // 从epoll中移除channel
            channel->set_index(kDeleted);   
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
     }
}
void EpollPoller::removeChannel(Channel* channel)
{
    const int fd=channel->fd();
    channels_.erase(fd);
    const int state=channel->index();
    if(state==kAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

void EpollPoller::update(int operation,Channel* channel)
{
    struct epoll_event event;
    bzero(&event,sizeof event);
    int fd=channel->fd();
    event.events=channel->events();
    event.data.ptr=channel;
    if(::epoll_ctl(epollfd_,operation,fd,&event)<0)
    {
       if(operation==EPOLL_CTL_DEL)
       {
        LOG_ERROR("epoll_ctl del error:%d",errno);
       }
       else
       {
        LOG_FATAL("epoll_ctl add/mod error:%d",errno);
       }
    }

}