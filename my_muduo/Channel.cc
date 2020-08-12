#include"Channel.h"
#include"EventLoop.h"
#include"logger.h"

#include<sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

//loop_为这个channel所属的eventloop
Channel::Channel (EventLoop *loop,int fd)
    :loop_(loop)
    ,fd_(fd)
    ,events_(0)
    ,revents_(0)
    ,index_(-1)
    ,tied_(false)
    {}
Channel::~Channel()
{
    //3个断言，1.保证channel是由remove调用的析构 2.保证析构时没有回调操作在进行 3.保证这个channel是由本EventLoop析构的
}

//一个新Channel创建时，channel会手握一个指向TcpConnection的弱智能指针
void Channel::tie(const std::shared_ptr<void>&obj)
{
    tie_= obj;
    tied_ = true;
}
/*
*当改变fd对应的events事件，update负责在poller里面更改fd相应的事件epoll_ctl
*通过本EventLoop调用唯一的poller，注册fd的events事件
更新
*/
void Channel::update()
{
    // add code...
    loop_->updateChannel(this);
}
//删除这个channel，vector<channel*>里
void Channel::remove()
{
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
   if(tied_)        //表示tid过
    {
        std::shared_ptr<void> guard = tie_.lock(); //如果提升成功，说明对象(TcpConnection)存在
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else            //这里时不确定TcpConnection是否存在的，为何可以直接执行
    {
        handleEventWithGuard(receiveTime);
    }
}
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n",revents_);

    if((revents_ & EPOLLHUP) && (revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
       if(readCallback_)
       {
           readCallback_(receiveTime);
       }
    }

    if(revents_ & EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
}