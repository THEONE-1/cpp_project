#pragma once
#include "Poller.hh"
#include<vector>
#include<sys/epoll.h>


// EpollPoller类继承自Poller,实现了基于epoll的IO复用
class EpollPoller:public Poller{
public:
    // 构造函数,创建epollfd
    EpollPoller(EventLoop* loop);
    // 析构函数,关闭epollfd
    ~EpollPoller();

    // 调用epoll_wait等待事件发生,并将活跃事件填充到activeChannels中
    Timestamp poll(int timeoutMs,ChannelList* activeChannels) override;
    // 更新Channel关注的事件
    void updateChannel(Channel* channel) override;
    // 从epoll中移除Channel
    void removeChannel(Channel* channel) override;
private:
    // EventList用于存储epoll_wait返回的事件
    using EventList=std::vector<struct epoll_event>;

    // events_的初始大小,muduo库的特点
    static const int kInitEventListSize=16;

    // 将epoll_wait返回的事件填充到activeChannels中
    void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;
    // 执行epoll_ctl操作
    void update(int operation,Channel* channel);
private:
    // epoll文件描述符
    int epollfd_;
    // 存储epoll_wait返回的事件数组
    EventList events_;

};