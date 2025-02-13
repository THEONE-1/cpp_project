#include "Buffer.hh"
#include <sys/uio.h>
#include <unistd.h>
/*从fd上读取数据，Poller工作在LT模式
   Buffer缓冲区是有大小的。但是从fd上读取数据时，不知道tcp数据的最终大小
*/
ssize_t Buffer::readFd(int fd, int* savaErrno)
{
    char extrabuf[65536]={0};//栈上的内存
    struct iovec vec[2];
    const size_t writeable= writeableBytes();//Buffer底层数组的剩余可写空间
    // 第一个缓冲区指向Buffer的可写空间
    vec[0].iov_base=beginWrite();  // 获取可写空间的起始地址
    vec[0].iov_len=writeable;      // 可写空间的大小
    
    // 第二个缓冲区指向栈上的额外缓冲区
    vec[1].iov_base=extrabuf;      // 栈上额外缓冲区的起始地址
    vec[1].iov_len=sizeof extrabuf;// 栈上额外缓冲区的大小65536字节
    // 这样通过readv可以一次性读取数据到多个缓冲区


    // 确定使用的缓冲区数量:
    // 如果Buffer的可写空间小于额外缓冲区大小(65536字节)，则同时使用两个缓冲区(iovcnt=2)
    // 否则Buffer空间足够大，只使用Buffer一个缓冲区(iovcnt=1)
    int iovcnt = (writeable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n=readv(fd, vec, iovcnt);
    if(n<0)
    {
        *savaErrno=errno;
    }
    else if(n<=writeable)
    {
        writer_index_+=n;
    }
    else{//extrabuf里面也写数据了
        writer_index_=buffer_.size();
        append(extrabuf, n-writeable);
    }
    return n;
}
ssize_t Buffer::writeFd(int fd, int* savaErrno)
{
    // 从Buffer中读取数据写入到fd中
    // peek()返回可读数据的起始位置
    // readableBytes()返回可读数据的长度
    // write()将数据写入fd
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n<0)
    {
        *savaErrno=errno;
    }
    return n;
}