#pragma once

#include<vector>
#include<string>
#include<algorithm>

class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
                :buffer_(kCheapPrepend + initialSize)
                ,readerIndex_(kCheapPrepend)
                ,writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }
    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }
    size_t preoendableBytes() const
    {
        return readerIndex_;
    }
    const char* peek()
    {
        //返回缓冲区中可读数据的起始地址
        return begin() + readerIndex_;
    }

    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ += len; //这个好理解，读完一部分，将可读数据的偏移len
        }
        else
        {
           retrieveAll();//如果一下子读完，就重置一下readerIndex，但是为什么要把wreterIndex也重置了？？？除非不写只读才符合这样的操作
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }
    //把onMessage函数上报的Buffer_数据，转换成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len)
    {
        //assert(len <= readableBytes());
        //把缓冲区的可读数据读出来
        std::string result(peek(),len);
        retrieve(len);
        return result;
    }
    void makeSpace(size_t len)
    {
        if(writableBytes() + preoendableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex_,
                      begin()+writerIndex_,
                      begin()+kCheapPrepend);
                      readerIndex_ = kCheapPrepend;
                      writerIndex_ = readerIndex_ + readable;
        }
    }

    void ensureWriteableBytes(size_t len)
    {
        if(writableBytes() < len )
        {
            makeSpace(len);//扩容
        }
    }
    //把[data,data+len]的数据添加到writeable缓冲区
    void append(const char *data,size_t len)
    {
        ensureWriteableBytes(len);
        std::copy(data,data+len,beginWrite());
        writerIndex_ += len;
    }
    char* beginWrite()
    {
        return begin() + writerIndex_;
    }
    //从fd上读取数据
    ssize_t readFd(int fd,int* saveErrno);
    //从fd发送数据
    ssize_t writeFd(int fd,int* saveErrno);
private:
    char* begin()
    {
        //*buffer_.begin()为第一个元素，取地址就是拿到了buffer_的地址
        return &*buffer_.begin();
    }
    const char* begin() const
    {
        return &*buffer_.begin();
    }
    std::vector<char>buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};