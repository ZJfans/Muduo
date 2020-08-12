#include"Buffer.h"

#include<errno.h>
#include<sys/uio.h>
#include<unistd.h>

/*
*从fd读取数据  poller在LT模式
*Buffer缓冲区有大小，但是从fd读数据，不知道tcp上最终的大小
*/
 ssize_t Buffer::readFd(int fd,int* saveErrno)
 {
     char extrabuf[65536] = {0};    //栈上的内存
     struct iovec vec[2];
     const size_t writable = writableBytes();//可用的写空间
     vec[0].iov_base = begin() + writerIndex_;
     vec[0].iov_len = writable;

     vec[1].iov_base = extrabuf;
     vec[1].iov_len = sizeof extrabuf;

     const int iovcnt = (writable < sizeof extrabuf)?2:1;
     const ssize_t n = ::readv(fd,vec,iovcnt);
     if(n < 0 )
     {
         *saveErrno = errno;
     }
     else if(n <= writable)
     {
         writerIndex_ += n;
     }
     else
     {
         writerIndex_ = buffer_.size();
         append(extrabuf,n-writerIndex_);//此处已写满，所以从writerIndex开始再往后写
     }
     return n;
 }
 ssize_t Buffer::writeFd(int fd,int* saveErrno)
 {
     ssize_t n = ::write(fd,peek(),readableBytes());
     if( n < 0)
     {
         *saveErrno = errno;
     }
     return n;
 }