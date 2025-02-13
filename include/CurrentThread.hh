#pragma once

namespace CurrentThread
{
    extern __thread int t_cachedTid;
    // 缓存线程ID
    void cacheTid();
    // tid()是一个简短的函数,内联可以消除函数调用开销
    inline int tid()
    {
        if(__builtin_expect(t_cachedTid==0,0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}