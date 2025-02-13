#pragma once

#include "noncopyable.hh"
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include "CurrentThread.hh"
#include "Timestamp.hh"

class Channel;
class Poller;

//事件循环类，主要包含Channel和Poller(epoll的抽象)
class EventLoop:noncopyable
{
public:
    // 用于存储回调函数的类型别名
    using Functor=std::function<void()>;
    // 用于存储Channel指针的容器类型别名
    using ChannelList=std::vector<Channel*>;

    // 构造函数,创建EventLoop对象
    EventLoop();
    // 析构函数
    ~EventLoop();

    // 返回poller返回发生事件的时间点
    Timestamp pollReturnTime() const {return pollReturnTime_;}

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();
    // 唤醒loop所在线程
    void wakeup();

    // 更新Channel通道
    void updateChannel(Channel* channel);
    // 移除Channel通道
    void removeChannel(Channel* channel);
    // 判断EventLoop对象是否拥有channel
    bool hasChannel(Channel* channel);

    // 判断当前线程是否在自己的线程中
    bool isInLoopThread() const {return threadId_==CurrentThread::tid();}

    // 在当前loop线程中执行回调函数
    void runInLoop(Functor cb);
    // 把回调函数放入队列中,并唤醒loop所在线程，执行cb
    void queueInLoop(Functor cb);

private:
    // 用于处理wakeupfd上的可读事件
    void handleRead();
    // 执行上层回调函数
    void doPendingFunctors();
    
private:
    std::atomic<bool> looping_;//是否在事件循环中
    std::atomic<bool> quit_;//是否退出事件循环

    int threadId_;//当前线程ID

    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    int wakeupFd_;//当mainloop获取一个新用户的channel，通过轮询算法选择一个subloop，通过wakeupFd唤醒
    std::unique_ptr<Channel> wakeupChannel_;//wakeupChannel用于唤醒subloop

    ChannelList activeChannels_;
    Channel* currentActiveChannel_;//当前活跃的channel

    std::vector<Functor> pendingFunctors_;//待执行的回调函数列表
    std::atomic<bool> callingPendingFunctors_;//是否正在调用回调函数
    std::atomic<bool> eventHandling_;//是否正在处理事件
    
    std::mutex mutex_;
};