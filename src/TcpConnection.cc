#include "TcpConnection.hh"
#include "Logger.hh"
#include "Socket.hh"
#include "Channel.hh"
#include "EventLoop.hh"
#include <errno.h>
#include<sys/socket.h>
#include<unistd.h>

EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                             const InetAddress &localAddr, const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(name),
      state_(kDisconnected),
      reading_(false),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) // 64M
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));
    LOG_INFO("TcpConnection::ctor[%s] at fd=%d", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d", name_.c_str(), channel_->fd());
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        // 调用用户设置的onMessage回调函数
        // shared_from_this()返回当前对象的智能指针
        // inputBuffer_保存了从socket读取到的数据
        // receiveTime是接收到数据的时间戳
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        // 将outputBuffer_中的数据写入到socket_中
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if (n > 0)
        {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
            
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_INFO("TcpConnection fd=%d is down,no more writing\n",channel_->fd());
    }
}
//poller=>channel::closeCallback=>TcpConnection::handleClose
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d is closed",channel_->fd());
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);//执行连接关闭的回调
    closeCallback_(connPtr);//关闭连接的回调，执行的是TcpServer::removeConnection
}
void TcpConnection::handleError()
{
    int optval;
    int err = 0;
    socklen_t optlen=sizeof optval;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0)
    {
        err=errno;
    }
    else
    {
        err=optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d",name_.c_str(),err);
}
void TcpConnection::sendInLoop(const void* data, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if(state_ == kDisconnecting)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    // 如果当前channel没有注册写事件，且输出缓冲区为空
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 直接发送数据
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            // 计算剩余未发送的数据长度
            remaining = len - nwrote;
            // 如果数据全部发送完成，且设置了写完成回调函数
            if(remaining == 0 && writeCompleteCallback_)
            {
                // 将写完成回调函数添加到loop的任务队列中
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if(errno!=EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno==EPIPE||errno==ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }
    // 如果数据没有全部发送完成，则将数据添加到输出缓冲区中,geichannel注册epollout事件，poller发现tcp的发送缓冲区有
    //空间，会通知相应的sock-channel调用writeCallback_回调方法，也就是TcpConnection::handleWrite方法，
    //handleWrite方法会调用socket的send方法，将outputBuffer_中的数据发送出去
    if(!faultError && remaining > 0)
    {
        //目前发送缓冲区剩余的待发送数据长度
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen+remaining>=highWaterMark_&&oldLen<highWaterMark_&&highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining));
        }
        //将剩余未发送的数据添加到outputBuffer_中
        // 这里是将未发送完的数据写入到输出缓冲区outputBuffer_中
        // 而上面outputBuffer_.readableBytes()是读取输出缓冲区中待发送的数据长度
        // 因为对于输出缓冲区来说:
        // 1. append是写入数据到缓冲区,等待后续发送
        // 2. readableBytes是读取缓冲区中还有多少数据未发送
        // 所以一个是写操作一个是读操作,但都是针对输出缓冲区
        outputBuffer_.append(static_cast<const char*>(data)+nwrote,remaining);
        if(!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}
void TcpConnection::send(const std::string& buf)
{
    if(state_==kConnected)
    {
        if(loop_->isInLoopThread())  // 如果是在当前IO线程中，直接调用sendInLoop
        {
            sendInLoop(buf.data(),buf.size());
        }
        else  // 如果是在其他线程中调用send，就需要把该函数转发到IO线程中去执行
        {
            // 通过runInLoop将sendInLoop函数转发到IO线程中执行
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop,this,buf.data(),buf.size()));
        }
    }
}
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());//将channel与TcpConnection绑定
    channel_->enableReading();
    connectionCallback_(shared_from_this());
}
void TcpConnection::connectDestroyed()
{
    if(state_==kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
}
void TcpConnection::shutdown()
{
    if(state_==kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())//说明outputBuffer_中没有待发送的数据
    {
        socket_->shutdownWrite();
        setState(kDisconnected);
    }
}