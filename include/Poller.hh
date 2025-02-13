#pragma once
#include "noncopyable.hh"
#include "Timestamp.hh"
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>
class Channel;
class EventLoop;

// Poller类的主要作用:
// 1. 封装了epoll的创建、销毁、添加、删除、修改等操作
// 2. 提供了获取活跃事件的方法
// 3. 提供了更新Channel的方法
class Poller:noncopyable{
public:
    // epoll_wait返回活跃事件后,将对应Channel添加到该列表中
    using ChannelList=std::vector<Channel*>;
protected:
    // ChannelMap用于存储Channel对象的映射关系
    // 通过fd作为键,Channel*作为值
    using ChannelMap=std::unordered_map<int,Channel*>;
    ChannelMap channels_;
    
public:
    Poller(EventLoop* loop);
    virtual ~Poller();
    // poll方法的作用是等待事件的发生，并将活跃的事件添加到activeChannels列表中
    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannels)=0;
    virtual void updateChannel(Channel* channel)=0;
    virtual void removeChannel(Channel* channel)=0;
    // 判断fd是否在channels_中
    bool hasChannel(Channel* channel) const;
    static Poller* newDefaultPoller(EventLoop* loop);
private:
    EventLoop* ownerLoop_;
};