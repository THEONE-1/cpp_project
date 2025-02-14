#include "mymuduo/TcpServer.hh"
#include<string>
#include<functional>

class EchoServer
{
public:
    EchoServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg)
        :server_(loop, listenAddr, nameArg), loop_(loop)
    {
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        server_.setThreadNum(4);
    }

    void start()
    {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn);  // 声明
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);  // 声明
    
    EventLoop* loop_;
    TcpServer server_;
};

// 定义
void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
    if(conn->connected())
    {
        LOG_INFO("conn UP:%s", conn->peerAddress().toIpPort().c_str());
    }
    else
    {
        LOG_INFO("conn DOWN:%s", conn->peerAddress().toIpPort().c_str());
    }
}

void EchoServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
    std::string msg = buf->retriveAllAsString();
    conn->send(msg);
    conn->shutdown();
}

int main()
{
    EventLoop loop;
    InetAddress listenAddr(8080);
    EchoServer server(&loop, listenAddr, "EchoServer-01");
    server.start();
    loop.loop();
    return 0;
}