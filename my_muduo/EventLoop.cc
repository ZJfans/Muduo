#include"EventLoop.h"
#include"logger.h"
#include"Poller.h"
#include"Channel.h"

#include<sys/eventfd.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<memory>

//thread_local  这使得每个线程都有一个下面这个，而不是所有线程共用一个
__thread EventLoop *t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000;

//创建wakeupfd,用来notify唤醒subReactor处理新来的channel
int creatEventfd()
{
    int evtfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0 )
    {
       LOG_FATAL("eventfd error:%d \n",errno);
    }
    return evtfd;
}
EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threadId_(CurrentThread::tid())
    ,poller_(Poller::newDefaultPoller(this))
    ,wakeupFd_(creatEventfd())
    ,wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another Eventloop %p exists in this thread %d \n",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    //设置wakeupfd//睡眠的线程监听这个wakeupFd,如果EventLoop  read这个fd（刺激一下线程），说明线程该干活了。
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}
//开启事件循环
void EventLoop::loop() 
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EvnentLoop %p start looping \n",this);

    while(!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel* channel : activeChannels_)
        {
            //poller监听channel发生事件，上报EventLoop，然后通知channel处理事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作,比如往subloop注册新的channel//std::vector<Functor>pendingFunctors_;//存储loop需要执行的回调操作
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n",this);
    looping_ = false;
}
//退出事件循环
void EventLoop::quit() 
{
    quit_ = true;
    if(!isInLoopThread())//a想退出b线程，但b线程阻塞在poller,所以先唤醒b,让b执行到while，判断quit_,然后就可以退出了
    {
        wakeup();
    }
}
void EventLoop::handleRead()
{
    uint16_t one = 1;
    ssize_t n = read(wakeupFd_,&one,sizeof one);
    if( n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}
void EventLoop::runInLoop(Functor cb)//在当前loop执行
{
    if(isInLoopThread())//在当前的loop线程中，执行cb
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}
void EventLoop::queueInLoop(Functor cb)   //把cb放入队列中，唤醒loop所在线程，执行cb
{
    {
        std::unique_lock<std::mutex>lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();//唤醒loop所在线程
    }
}
void EventLoop::wakeup()  //用来唤醒loop所在的线程
{
    uint16_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}

    //调用poller中的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hansChannel(Channel *channel)
{
   return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors()//执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors)
    {
        functor();//执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}

