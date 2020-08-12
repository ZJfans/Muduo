#include"Acceptor.h"
#include"logger.h"
#include"InetAddress.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<errno.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop,const InetAddress& listenAddr,bool reuseport)
        :loop_(loop)
        ,acceptSocket_(createNonblocking())
        ,acceptChannel_(loop,acceptSocket_.fd())
        ,listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));

}
Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();//将acceptchannel注册到poller中，应该是mainloop里的poller
}

//listenfd有事件发生，有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr); //就是执行一个回调，把这个acceptfd，打包成channel，然后交给subloop
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit! \n",__FILE__,__FUNCTION__,__LINE__);
        }
    }

}