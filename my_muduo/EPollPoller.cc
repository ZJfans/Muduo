#include"EPollPoller.h"
#include"logger.h"
#include"Channel.h"
#include<errno.h>
#include<unistd.h>
#include<cstring>
#include<sys/epoll.h>

const int KNew = -1;
const int KAdded = 1;
const int KDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    ,events_(kInitEventListSize)
{
    if(epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n",errno);
    }
}
EPollPoller::~EPollPoller() 
{
    ::close(epollfd_);
}
//重写基类poller的方法
Timestamp EPollPoller::poll(int timeoutMs,ChannelList *activeChannels) 
{
    LOG_INFO("func=%s => fd total count:%lu \n",__FUNCTION__,channels_.size());

    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);

        if(numEvents == events_.size())
        {
            events_.resize(events_.size()*2);//扩容
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout! \n",__FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}
//channel通过EventLoop来调用poller中的方法
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd = %d events=%d index=%d \n",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index == KNew || index == KDeleted)
    {
        if(index == KNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(KAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(KDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
}
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd = %d \n",__FUNCTION__,fd);

    int index = channel->index();
    if(index == KAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(KNew);
   
}
//填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i=0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的poller给它返回的所有发生事件的channel列表了
    }
}
    //更新channel通道
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    bzero(&event, sizeof event);
    
    int fd = channel->fd();

    event.events = channel->events();
    
    

    event.data.fd = fd;        //以下2行奇葩

    event.data.ptr = channel;

    

    
    
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}