#pragma once

#include "noncopyable.hh"
#include "Thread.hh"
#include <mutex>
#include<condition_variable>

class EventLoop;

class EventLoopThread : noncopyable
{
public:
    using ThreadInitCallback =std::function<void(EventLoop*)>;
    EventLoopThread(const ThreadInitCallback& cb=ThreadInitCallback(),const std::string& name="EventLoopThread ");
    ~EventLoopThread();
    EventLoop* startLoop();
private:
    void threadFunc();
 
    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};