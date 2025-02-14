#include "Socket.hh"
#include "InetAddress.hh"
#include "Logger.hh"
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<arpa/inet.h>
#include<unistd.h>

Socket::~Socket()
{
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    // sizeof(struct sockaddr_in)和对象大小相同,都是16字节。
    if(::bind(sockfd_,(sockaddr*)localaddr.getSockAddr(),sizeof(struct sockaddr_in))<0) 
    {
        LOG_FATAL("bind sockfd:%d failed",sockfd_);
    }
}
void Socket::listen()
{
    if(::listen(sockfd_,1024)<0)
    {
        LOG_FATAL("listen sockfd:%d failed",sockfd_);
    }
}

int Socket::accept(InetAddress* peeraddr)
{
    /*
    1.accept函数的参数不合法
    对返回的connfd没有设置非阻塞
    */
    sockaddr_in addr;
    bzero(&addr,sizeof addr);
    socklen_t len=sizeof(addr);
    int connfd=::accept4(sockfd_,(sockaddr*)&addr,&len,SOCK_NONBLOCK|SOCK_CLOEXEC);//返回的connfd是非阻塞的
    if(connfd<0)
    {
        LOG_ERROR("accept error");
    }
    peeraddr->setSockAddr(addr);
    return connfd;
}

void Socket::shutdownWrite()
{
    ::shutdown(sockfd_,SHUT_WR);
}

void Socket::setTcpNoDelay(bool on)
{
    int optval=on?1:0;
    ::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof optval);
}

void Socket::setReuseAddr(bool on)  // 允许重用本地地址
{
    int optval=on?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof optval);
}

void Socket::setReusePort(bool on)
{
    int optval=on?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval=on?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof optval);
}