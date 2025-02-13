#pragma once

//noncopyable类是一个基类，用于禁止对象的复制和赋值操作,派生类可以正常析构和构造，但是无法进行拷贝和赋值
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};