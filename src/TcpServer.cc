#include"TcpServer.hh"
#include"Logger.hh"

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
    nextConnId_(1)
{
    //当有新用户连接时会执行TcpServer::newConnection回调
        acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
        std::placeholders::_1,std::placeholders::_2));
}

void TcpServer::newConnection(int sockfd,const InetAddress& peerAddr)
{
    EventLoop* ioLoop=threadPool_->getNextLoop();//从线程池中获取一个EventLoop
    char buf[64];
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