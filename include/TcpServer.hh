#pragma once

#include"noncopyable.hh"
#include"EventLoop.hh"
#include"Acceptor.hh"
#include"InetAddress.hh"
#include"TcpConnection.hh"
#include"Logger.hh"
#include"EventLoopThreadPool.hh"
#include"Callbacks.hh"  
#include"Buffer.hh"
#include"Timestamp.hh"
#include<unordered_map>

class TcpServer:noncopyable
{
public:
    using ThreadInitCallback=std::function<void(EventLoop*)>;
    enum Option{
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop* loop,const InetAddress& listenAddr,const std::string& nameArg,Option option=kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback& cb){threadInitCallback_=cb;}
    void setThreadNum(int numThreads);
    void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_=cb;}
    void setMessageCallback(const MessageCallback& cb){messageCallback_=cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_=cb;}

    //启动服务器
    void start();
private:
    using ConnectionMap=std::unordered_map<std::string,TcpConnectionPtr>;

    //当有新用户连接时，会创建一个TcpConnection对象
    void newConnection(int sockfd,const InetAddress& peerAddr);
    //连接关闭时，移除对应的TcpConnection对象
    void removeConnection(const TcpConnectionPtr& conn);
    //在loop线程中移除TcpConnection对象
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    EventLoop* loop_;//用户定义的loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;//监听新连接
    std::unique_ptr<EventLoopThreadPool> threadPool_;//one loop per thread

    ConnectionCallback connectionCallback_;//新连接到来时的回调
    MessageCallback messageCallback_;//有读消息时的回调
    WriteCompleteCallback writeCompleteCallback_;//消息发送完成时的回调

    ThreadInitCallback threadInitCallback_;//线程初始化回调

    std::atomic_int started_;//服务器是否启动
    int nextConnId_;//下一个连接的id
    ConnectionMap connections_;//保存所有连接

};