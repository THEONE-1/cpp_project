#include "Acceptor.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "Logger.hh"
#include "InetAddress.hh"
#include <unistd.h>

static int creatNonblocking()
{
    int sockfd=::socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d",__FILE__,__FUNCTION__,__LINE__,errno );
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop,const InetAddress& listenAddr,bool reuseport)
:loop_(loop),
acceptSocket_(creatNonblocking()),
acceptChannel_(loop,acceptSocket_.fd()),
listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    //TcpServer::start()中会调用Acceptor::listen() 有新用户连接，要执行一个回调（connfd=>>Channel=>>subloop)
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_=true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}
void Acceptor::handleRead()
{
    InetAddress peeraddr;
    int connfd=acceptSocket_.accept(&peeraddr);
    if(connfd>=0)
    {
        // newConnectionCallback_是一个回调函数对象
        // 它是在TcpServer中设置的，类型是function<void(int,const InetAddress&)>
        // 当有新连接到来时会被调用，用于处理新连接
        if(newConnectionCallback_)
        {
            // 调用回调函数，传入新连接的fd和对端地址
            // 在TcpServer中会将新连接分发给subloop处理
            newConnectionCallback_(connfd,peeraddr);
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno==EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit",__FILE__,__FUNCTION__,__LINE__);
        }
    }

}