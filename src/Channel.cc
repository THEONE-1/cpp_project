#include "Channel.hh"
#include <sys/epoll.h>
#include "Logger.hh"
#include "EventLoop.hh"
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;


Channel::Channel(EventLoop* loop,int fd)
    :loop_(loop),fd_(fd),events_(0),revents_(0),index_(-1),tied_(false)
{}

Channel::~Channel()
{
    LOG_INFO("Channel::~Channel() fd=%d",fd_);
}

// tie方法用于防止Channel被手动删除掉，Channel的生命期与TcpConnection绑定在一起
// TcpConnection中创建的Channel对象通过tie绑定到TcpConnection对象上
// 当处理事件时，先判断tie_.lock()是否为空，若为空说明TcpConnection对象已经销毁，就不再处理事件
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;  // 将TcpConnection对象的智能指针存储为weak_ptr
    tied_ = true;  // 标记已绑定
}

//当改变channel所表示的event事件后，用于更新Channel在Poller中的状态
void Channel::update()
{
    loop_->updateChannel(this);
}

// 移除Channel
void Channel::remove()
{
    loop_->removeChannel(this);
}
void Channel::handleEvent(Timestamp receiveTime)
{
    // 如果Channel已经绑定了TcpConnection对象(tied_为true)
    if(tied_){
        // 尝试获取TcpConnection对象的强引用
        // 这里使用lock()将weak_ptr转换为shared_ptr
        // 目的是确保在执行事件处理时TcpConnection对象不会被销毁
        std::shared_ptr<void> guard=tie_.lock();
        // 如果获取成功,说明TcpConnection对象还存在
        if(guard){
            // 此时可以安全地处理事件
            // guard的生命周期会持续到handleEventWithGuard执行完毕
            // 保证了事件处理期间TcpConnection对象不会被销毁
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}

//根据poller通知channel发生的事件类型，调用相应的回调函数--》poller必然要有设置revents_的接口
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("Channel::handleEventWithGuard() revents_=%d",revents_);
    // EPOLLHUP表示对端关闭连接,且没有数据可读(EPOLLIN)时,调用关闭回调
    if((revents_&EPOLLHUP)&&!(revents_&EPOLLIN)){
        if(closeCallback_)
            closeCallback_();
    }   

    // EPOLLIN表示有数据可读
    // EPOLLPRI表示有紧急数据可读
    // EPOLLRDHUP表示对端关闭连接或者关闭写端
    if(revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)){
        if(readCallback_) readCallback_(receiveTime);
    }

    // EPOLLOUT表示可以写数据
    if(revents_ & EPOLLOUT){
        if(writeCallback_) writeCallback_();
    }

    // 发生错误时调用错误回调
    if(errorCallback_) errorCallback_();
}