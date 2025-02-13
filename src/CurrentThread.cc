#include "CurrentThread.hh"
#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread
{
    __thread int t_cachedTid=0;
    void cacheTid()
    {
        // 如果当前线程的tid还未缓存
        if(t_cachedTid==0)
        {
            // 通过系统调用获取真实线程ID并缓存
            // syscall(SYS_gettid)返回调用线程的tid
            // 转换成pid_t类型并存入线程局部变量t_cachedTid
            t_cachedTid=static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}