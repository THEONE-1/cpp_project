#include"EventLoopThreadPool.hh"
#include"EventLoopThread.hh"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop,const std::string& nameArg)
    :baseloop_(baseLoop),name_(nameArg),started_(false),numThreads_(0),next_(0)
{

}

EventLoopThreadPool::~EventLoopThreadPool()
{}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
    started_=true;
    for(int i=0;i<numThreads_;i++)
    {
        char buf[name_.size()+32];
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread* t=new EventLoopThread(cb,buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startLoop());//底层创建线程，绑定一个新的EventLoop，并返回该loop的指针
    }
    //整个服务端只有一个线程，运行baseloop_
    if(numThreads_==0&&cb)
    {
        cb(baseloop_);
    }  
}
//如果工作在多线程中，baseloop_默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop()
{
    EventLoop* loop=baseloop_;
    if(!loops_.empty())
    {
        loop=loops_[next_];
        ++next_;
        if(static_cast<size_t>(next_)>=loops_.size())
        {
            next_=0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
    if(loops_.empty())
    {
        return std::vector<EventLoop*>(1,baseloop_);
    }
    return loops_;
}