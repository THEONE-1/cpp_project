#pragma once

#include"noncopyable.hh"
#include<memory>
#include<atomic>
#include<string>
#include"Callbacks.hh"
#include"InetAddress.hh"
#include"Buffer.hh"
#include"Timestamp.hh"
class Channel;
class EventLoop;
class Socket;

/* TcpServer => Acceptor => 新连接,通过accept函数拿到connfd
 => TcpConnection 设置函数回调 =》Channel => EventLoop => Poller => 
 => 调用Channel的handleEvent() => 调用TcpConnection的handleRead() => 
 => 调用TcpConnection的onMessage() => 调用TcpServer的onMessage() => 
 => 调用TcpServer的messageCallback_() => 调用TcpConnection的messageCallback_() */
class TcpConnection :noncopyable,public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop* loop,const std::string& name,int sockfd,
                  const InetAddress& localAddr,const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const {return loop_;}
    const std::string& name() const {return name_;}
    const InetAddress& LocalAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const {return peerAddr_;}

    bool connected() const {return state_==kConnected;}
    bool disconnected() const {return state_==kDisconnected;}
    bool connecting() const {return state_==kConnecting;}

    void send(const std::string& buf);
    void shutdown();
    
    void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_=cb;}
    void setMessageCallback(const MessageCallback& cb) {messageCallback_=cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_=cb;}
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb){highWaterMarkCallback_=cb;}
    void setCloseCallback(const CloseCallback& cb){closeCallback_=cb;}
    
    //连接建立时调用
    void connectEstablished();
    //连接关闭时调用
    void connectDestroyed();

    
private:
    enum StateE{kDisconnected,kConnecting,kConnected,kDisconnecting};
    void setState(StateE s){state_=s;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
  

    EventLoop* loop_;//不是用户定义的loop
    const std::string name_;
    std::atomic_int state_;
    bool reading_;//是否正在读取

    std::unique_ptr<Socket> socket_;  //用于管理TCP连接对应的socket文件描述符
    std::unique_ptr<Channel> channel_;//用于管理socket_上的事件

    const InetAddress localAddr_;//本地地址
    const InetAddress peerAddr_;//对端地址

    ConnectionCallback connectionCallback_;//新连接到来时的回调
    MessageCallback messageCallback_;//有读消息时的回调
    WriteCompleteCallback writeCompleteCallback_;//消息发送完成时的回调
    HighWaterMarkCallback highWaterMarkCallback_;//高水位回调
    CloseCallback closeCallback_;//连接关闭时的回调
    size_t highWaterMark_;//高水位标记

    Buffer inputBuffer_;//输入缓冲区
    Buffer outputBuffer_;//输出缓冲区   
};