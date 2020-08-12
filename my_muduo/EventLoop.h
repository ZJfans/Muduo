#pragma once

#include"noncopyable.h"
#include"Timestamp.h"
#include"CurrentThread.h"

#include<functional>
#include<vector>
#include<atomic>
#include<memory>
#include<mutex>

class Channel;
class Poller;

// 事件循环类   主要包含了2大模块 Channel poller(epoll的抽象)
class EventLoop:noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();
    
    void loop(); //开启事件循环
    void quit(); //退出事件循环

    Timestamp pollReturnTime() const { return pollReturnTime_;}

    void runInLoop(Functor cb); //在当前loop执行
    void queueInLoop(Functor cb);   //把cb放入队列中，唤醒loop所在线程，执行cb

    void wakeup();   //用来唤醒loop所在的线程

    //调用poller中的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hansChannel(Channel *channel);

    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}//判断EventLoop是否在当前线程
private:

    void handleRead();  //
    void doPendingFunctors();//执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;  //atomic
    std::atomic_bool quit_;     //标志退出loop循环
    
    const pid_t threadId_;//记录当前loop线程(创建它的线程)所在的id

    Timestamp pollReturnTime_;  //poller返回channel的时间点
    std::unique_ptr<Poller>poller_;

    int wakeupFd_;  //当mainloop获取一个新用户的channel，通过一个轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel>wakeupChannel_;

    ChannelList activeChannels_;
    
   std::atomic_bool callingPendingFunctors_; //标识当前loop是否有需要执行的回调操作
   std::vector<Functor>pendingFunctors_;//存储loop需要执行的回调操作
   std::mutex mutex_;  //互斥锁用来保护上面容器的线程安全操作


};