#include "TcpServer.hh"
#include "Logger.hh"
#include <strings.h>
#include <sys/socket.h>
#include "TcpConnection.hh"

EventLoop* CheckLoopNotNull(EventLoop* loop, const char* name)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const std::string& nameArg,
                     Option option)
    :loop_(loop),
    ipPort_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop,listenAddr,option==kReusePort)),
    threadPool_(new EventLoopThreadPool(loop,name_)),
    connectionCallback_(),
    messageCallback_(),
    nextConnId_(1),
    started_(0)
{
    //当有新用户连接时会执行TcpServer::newConnection回调
        acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
        std::placeholders::_1,std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for(auto& item:connections_)
    {
        TcpConnectionPtr conn(item.second);//这个局部的shared_ptr智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象
        item.second=nullptr;
        //销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
    }
}

void TcpServer::newConnection(int sockfd,const InetAddress& peerAddr)
{
    EventLoop* ioLoop=threadPool_->getNextLoop();//从线程池中获取一个EventLoop
    char buf[64];
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName=name_+buf;

    LOG_INFO("TcpServer::newConnection[%s] - new connection[%s] from %s",name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的本机IP地址和端口号
    sockaddr_in local;
    bzero(&local,sizeof local);
    socklen_t addrlen=sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0)
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    //根据连接成功的sockfd，创建TcpConnectionPtr智能指针
    TcpConnectionPtr conn(new TcpConnection(ioLoop,connName,sockfd,localAddr,peerAddr));
    connections_[connName]=conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    //设置了如何关闭连接的回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));
}

//设置底层loop的数量
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

//开启服务器监听
void TcpServer::start(){
    if(started_++==0)
    {
        threadPool_->start(threadInitCallback_);//启动底层的loop线程池
        // 通过runInLoop调用Acceptor::listen
        // 将listen的调用放到IO线程中执行，确保线程安全
        // acceptor_.get()获取原始指针，因为acceptor_是unique_ptr
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop[%s] - connection %s",name_.c_str(),conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop* ioLoop=conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));

}