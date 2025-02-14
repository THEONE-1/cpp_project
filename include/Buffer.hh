#pragma once
#include <vector>
#include <string>
#include<algorithm>

/*
    KCheapPrepend |  reader |  writer |
*/
class Buffer{

public:
    static const size_t kInitialSize = 1024;
    static const size_t kCheapPrepend = 8; // 预留8字节空间，用于记录数据包的长度等信息

    explicit Buffer(size_t initialSize = kInitialSize)
        :buffer_(kCheapPrepend + initialSize),
        reader_index_(kCheapPrepend),
        writer_index_(kCheapPrepend)
    {}

    size_t readableBytes() const{
        return writer_index_ -reader_index_;
    }
    size_t writeableBytes() const
    {
        return buffer_.size() - writer_index_;
    }
    // 返回可以用于在数据前面添加数据的字节数
    // prepend意味着在已有数据的前面添加新数据
    // 这里返回reader_index_，表示在读取位置之前的空闲空间大小
    size_t prependableBytes() const
    {
        return reader_index_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin() + reader_index_;
    }

    // 从缓冲区中读取指定长度的数据，更新读指针位置
    // 如果len小于等于可读数据长度，则更新读指针位置
    void retrieve(size_t len)
    {
        if(len <= readableBytes())
        {
            reader_index_ += len;
        }
        else{
           retrieveAll();
        }
    }

    void retrieveAll()
    {
        reader_index_ = kCheapPrepend;
        writer_index_ = kCheapPrepend;
    }
    
    //把onMessage函数报的Buffer数据转成string类型的数据返回
    std::string retriveAllAsString()
    {
        return retriveAsString(readableBytes());
    }

    // 从缓冲区中读取指定长度的数据，并以字符串形式返回
    // len: 要读取的字节数
    // 返回值: 读取的数据转换成的字符串
    std::string retriveAsString(size_t len)
    {
        // 创建string，使用peek()返回的可读数据起始位置和指定长度构造
        std::string result(peek(), len);
        // 更新已读位置
        retrieve(len);
        // 返回读取的数据
        return result;
    }

    void ensureWriteableBytes(size_t len)
    {
        if(writeableBytes() < len)
        {
            makeSpace(len);
        }
   
    }

    // 向缓冲区追加数据
    // 1. 确保缓冲区有足够空间容纳新数据
    // 2. 将数据拷贝到写指针位置
    // 3. 更新写指针位置
    void append(const char* data, size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data, data + len, beginWrite());
        writer_index_ += len;
    }
    char* beginWrite()
    {
        return begin() + writer_index_;
    }
    const char* beginRead() const
    {
        return begin() + reader_index_;
    }

    ssize_t readFd(int fd, int* savaErrno);
    ssize_t writeFd(int fd, int* savaErrno);
private:
    // 返回缓冲区的起始地址
    // 由于buffer_是vector<char>，buffer_.begin()返回迭代器
    // 对迭代器解引用(*buffer_.begin())得到第一个元素，再取地址(&*buffer_.begin())得到底层数组的起始地址
    const char* begin() const
    {
        return &*buffer_.begin();
    }
    char* begin()
    {
        return &*buffer_.begin();

    }

    void makeSpace(size_t len)
    {
        if(writeableBytes() + prependableBytes() - kCheapPrepend< len )
        {
            buffer_.resize(writer_index_ + len);
        }
        else{
            // 如果可写空间 + 预留空间 - kCheapPrepend >= len，说明通过整理空间就可以满足写入需求
            // 获取当前可读数据的大小
            size_t readable = readableBytes();
            // 将可读数据移动到kCheapPrepend之后的位置，腾出reader_index_之前的空间
            std::copy(begin() + reader_index_, begin() + writer_index_, begin() + kCheapPrepend);
            // 重置读指针到kCheapPrepend位置
            reader_index_ = kCheapPrepend;
            // 写指针移动到可读数据之后，这样reader_index_到writer_index_之间就是可读数据
            writer_index_ = reader_index_ + readable;
        }
    }
   
     
    std::vector<char> buffer_;
    size_t reader_index_;
    size_t writer_index_;

};