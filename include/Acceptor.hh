#pragma once

#include "noncopyable.hh"
#include <functional>
#include "Socket.hh"
#include "Channel.hh"

class EventLoop;
class InetAddress;


class Acceptor:noncopyable
{
public:
    using NewConnectionCallback=std::function<void(int sockfd,const InetAddress&)>;
    Acceptor(EventLoop *loop,const InetAddress& listenAddr,bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb){newConnectionCallback_=std::move(cb);}
    bool listenning() const{return listenning_;}
    void listen();
private:
    void handleRead();

    EventLoop* loop_;//Acceptor用的是用户定义的那个baseloop，也就是mianloop
    
    Socket acceptSocket_;
    Channel acceptChannel_;

    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
};