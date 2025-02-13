#pragma once

#include"noncopyable.hh"
#include<functional>
#include<thread>
#include<memory>
#include<string>
#include<atomic>
#include<unistd.h>

class Thread : public noncopyable
{
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc, const std::string& name = std::string());
    ~Thread();
    void start();
    void join();

    bool started() const {return started_;}
    pid_t tid() const {return tid_;}
    const std::string& name() const {return name_;}

    static int numCreated() {return numCreated_;}
    
private:
    void setDefaultName();
    bool started_;
    bool joined_;
    /* 直接使用std::thread会在构造时立即启动线程
     这里需要手动控制线程启动的时机(通过start()函数)
    因此使用指针来延迟构造thread对象*/
    std::shared_ptr<std::thread> thread_;
    std::string name_;  
    pid_t tid_;
    ThreadFunc func_;
    static std::atomic_int numCreated_;
};