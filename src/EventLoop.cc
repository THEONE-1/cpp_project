#include "EventLoop.hh"
#include<sys/eventfd.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/timerfd.h>
#include<iostream>
#include"Logger.hh"
#include "Channel.hh"
#include "Poller.hh"
#include<errno.h>
//防止一个线程创建多个EventLoop对象
__thread EventLoop* t_loopInThisThread=nullptr;

//定义默认poller IO复用的超时时间
const int kPollTimeMs=10000;

//创建eventfd,来唤醒subReactor来处理新来的channel
int createEventfd()
{
    int evtfd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(evtfd<0)
    {
        LOG_FATAL("eventfd error:%d",errno);
    }
    return evtfd;
}
//EventLoop的构造函数
EventLoop::EventLoop()
    :looping_(false),
    quit_(false),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this,wakeupFd_)),
    currentActiveChannel_(nullptr)
{
    LOG_DEBUG("EventLoop created %p in thread %d",this,threadId_);
    if(t_loopInThisThread) // t_loopInThisThread是线程局部变量,用于确保一个线程只能有一个EventLoop对象
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread=this;
    }
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread=nullptr;
}
void EventLoop::loop()
{
    looping_=true;
    quit_=false;
    LOG_INFO("EventLoop %p start looping",this);
    while(!quit_)
    {
        // 每次事件循环开始时,先清空activeChannels_列表
        // clear()只是清空容器中的元素,但保留了容器的容量,不会释放内存
        // 这样做可以避免内存频繁分配和释放,提高性能
        activeChannels_.clear();
        
        // poll会将新的活跃事件重新填充到activeChannels_中
        // 由于之前已经clear,所以不会有上一轮遗留的channel
        pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);
        
        // 遍历本轮活跃的channel并处理事件
        for(Channel* channel:activeChannels_)
        {
            channel->handleEvent(pollReturnTime_);
        }
        // 1. 其他线程(如IO线程)可能通过queueInLoop()添加了回调到pendingFunctors_
        // 2. 这些回调需要在当前loop线程中执行,以保证线程安全
        // 3. 如果不及时执行,会导致回调堆积,影响程序响应性能
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping",this);
    looping_=false;
}

// 退出事件循环 1.loop在自己线程中调用
void EventLoop::quit()
{
    quit_=true;
    if(!isInLoopThread())//绕过是在其他线程中调用的quit
    {
        wakeup();
    }
}
void EventLoop::wakeup()
{
    uint64_t one=1;
    ssize_t n=write(wakeupFd_,&one,sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8",n);
    }   
}
void EventLoop::runInLoop(Functor cb)
{
    // 判断当前线程是否是EventLoop所属线程
    // 如果是,直接在当前线程执行回调函数
    // 如果不是,就调用queueInLoop将回调函数加入队列
    // 这样做的原因:
    // 1. 保证线程安全 - 回调函数都在EventLoop线程中执行
    // 2. 避免加锁开销 - 同线程直接执行无需加锁
    // 3. 支持跨线程调用 - 其他线程可以给EventLoop线程添加任务
    if(isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}
// 将回调函数cb放入队列中,由EventLoop所属线程执行
// 1. 如果调用线程不是EventLoop线程,需要唤醒EventLoop线程
// 2. 如果当前正在执行回调,也需要唤醒以便及时处理新回调
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    if(!isInLoopThread()||callingPendingFunctors_)
    {
        wakeup();
    }
}
void EventLoop::handleRead()
{
    // 从wakeupFd_读取数据,用于处理跨线程唤醒
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one)
    {
        // 读取数据出错,记录日志
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8", n);
    }
}
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    // 标记正在执行回调函数,避免死锁
    callingPendingFunctors_=true;

    {
        // 加锁,保护pendingFunctors_的访问
        std::unique_lock<std::mutex> lock(mutex_);
        // 使用swap减小临界区范围,提高并发性
        // 直接遍历pendingFunctors_会导致锁持有时间过长
        // swap后把functors的内存交换给pendingFunctors_
        // 这样可以尽快释放锁,让其他线程继续添加回调
        functors.swap(pendingFunctors_);
    }

    // 执行回调函数
    // 此时无需加锁,因为functors是局部变量
    // 即使其他线程往pendingFunctors_添加回调也不会影响这里的执行
    for(const Functor& functor:functors)
    {
        functor();
    }
}
void EventLoop::updateChannel(Channel* channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel)
{
    return poller_->hasChannel(channel);
}
