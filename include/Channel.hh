#pragma once
#include "noncopyable.hh"
#include<functional>
#include"Timestamp.hh"
#include<memory>
class EventLoop;

/*
 * Channel 封装了文件描述符和其感兴趣的事件
 * 每个Channel对象只属于一个EventLoop
 * 每个Channel对象只负责一个文件描述符的IO事件分发
 * Channel 会把不同的IO事件分发为不同的回调函数
 */

// Channel类的主要作用:
// 1. 封装了文件描述符(fd)和其感兴趣的事件(events)
// 2. 绑定了事件发生时的回调函数
// 3. 是EventLoop和文件描述符事件的桥梁
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;
    
    Channel(EventLoop* loop,int fd);
    ~Channel();

    //fd得到poller通知以后,调用handleEvent处理事件
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    // 使用std::move可以避免拷贝构造函数的调用,直接转移回调函数对象的所有权
    // 因为回调函数对象(std::function)的拷贝开销较大,使用移动语义可以提高性能
    void setReadCallback(ReadEventCallback cb){readCallback_=std::move(cb);}
    void setWriteCallback(EventCallback cb){writeCallback_=std::move(cb);}
    void setCloseCallback(EventCallback cb){closeCallback_=std::move(cb);}
    void setErrorCallback(EventCallback cb){errorCallback_=std::move(cb);}

    void tie(const std::shared_ptr<void>& obj);

    int fd() const {return fd_;}
    int events() const {return events_;}
    void set_revents(int revt){revents_=revt;}

    //设置fd相应的事件状态
    void enableReading(){events_ |=kReadEvent;update();}
    void disableReading(){events_ &=~kReadEvent;update();}
    void enableWriting(){events_ |=kWriteEvent;update();}
    void disableWriting(){events_ &=~kWriteEvent;update();}
    void disableAll(){events_=kNoneEvent;update();}

  // 判断channel是否为空
    bool isNoneEvent() const {return events_==kNoneEvent;}
    // 判断channel是否关注写事件
    bool isWriting() const {return events_ & kWriteEvent;}
    // 判断channel是否关注读事件
    bool isReading() const {return events_ & kReadEvent;}

    //更新channel感兴趣的事件状态
    void update();
    void remove();
    
    int index(){return index_;}
    void set_index(int idx){index_=idx;}

    EventLoop* ownerLoop(){return loop_;}


private:
    void handleEventWithGuard(Timestamp receiveTime);

private:
    // 定义事件类型常量
    // kNoneEvent表示不关注任何事件
    // kReadEvent表示关注读事件(EPOLLIN)
    // kWriteEvent表示关注写事件(EPOLLOUT)
    static const int kNoneEvent;  // 不关注任何事件
    static const int kReadEvent;  // 关注读事件
    static const int kWriteEvent; // 关注写事件
    
    EventLoop* loop_;// 事件循环
    int fd_;// 文件描述符
    int events_;// 感兴趣的事件
    int revents_;// 实际发生的事件
    int index_;// 在Poller中的索引  
    std::weak_ptr<void> tie_;// 弱指针,用于管理Channel的生命周期
    bool tied_;// 是否已绑定
    
    //因为channel通道里面能够获知fd最终发生的具体事件revvent_,所以它负责调用这些回调函数

    // 读事件回调需要传入接收数据的时间戳,所以使用ReadEventCallback
    ReadEventCallback readCallback_;
    // 其他事件回调不需要额外参数,所以使用EventCallback
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};