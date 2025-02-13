#include "Poller.hh"
#include <cstdlib>
#include "EpollPoller.hh"
// 将newDefaultPoller方法单独放在一个文件中实现的好处:
// 1. 实现了基类Poller和具体实现EPollPoller的解耦
//    - Poller.cc中只包含基类的通用实现
//    - DefaultPoll.cc负责创建具体的Poller实例
// 2. 方便以后扩展不同的Poller实现
//    - 可以通过修改此文件来切换使用不同的Poller实现(如poll、epoll等)
//    - 不需要修改Poller基类的代码
// 3. 避免了循环依赖
//    - 如果在Poller.cc中实现,需要包含EPollPoller.hh
//    - 而EPollPoller继承自Poller,这样会造成循环依赖
Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if(::getenv("MUDUO_USE_POLL"))
    {
        return nullptr;
    }
    else
    {
        return new EpollPoller(loop);
    }
}