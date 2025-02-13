#include"EventLoopThread.hh"
#include"EventLoop.hh"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,const std::string& name)
    :loop_(nullptr),exiting_(false),thread_(std::bind(&EventLoopThread::threadFunc,this),name),
    mutex_(),cond_(),callback_(cb)
{

}

EventLoopThread::~EventLoopThread()
{
    exiting_=true;
    if(loop_!=nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    // 开启新线程,执行threadFunc函数
    // 每个EventLoopThread都会创建一个新线程
    // 该线程会运行一个EventLoop对象,实现one loop per thread
    thread_.start();
    EventLoop* loop=nullptr;
    {
        std::unique_lock<std::mutex> Lock(mutex_);
        while(loop_==nullptr)//当threadFunc执行完，loop_被赋值，此时loop_不为空，退出循环
        {
            cond_.wait(Lock);
        }
        loop=loop_;
    }
    return loop;
}

//在单独的新线程里面执行
void EventLoopThread::threadFunc()
{
    EventLoop loop;//创建一个独立的EventLoop，和上面的线程一一对应，one loop one thread
    // 如果设置了初始化回调函数callback_,则在EventLoop对象创建后执行
    // 这允许用户在EventLoop运行前执行一些初始化操作
    // 比如设置定时器、注册IO事件处理等
    // callback_是在构造函数中传入的ThreadInitCallback类型的函数对象
    if(callback_)
    {
        callback_(&loop);
    }
    {
        std::unique_lock<std::mutex> Lock(mutex_);
        loop_=&loop;
        cond_.notify_one();
    }
    loop.loop();
    //到这说明服务程序退出
    std::unique_lock<std::mutex> Lock(mutex_);
    loop_=nullptr;
}