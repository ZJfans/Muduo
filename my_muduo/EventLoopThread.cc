#include"EventLoopThread.h"
#include"EventLoop.h"

EventLoopThread::EventLoopThread (const ThreadInitCallback& cb,
        const std::string& name)
        :loop_(nullptr)
        ,exiting_(false)
        ,thread_(std::bind(&EventLoopThread::threadFunc,this),name)
        ,mutex_()
        ,cond_()
        ,callback_(cb)
{

}
EventLoopThread ::~EventLoopThread ()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread ::startLoop()
{   
    //看上面构造，他给thread_绑定了threadFunc,直接传过去了
    thread_.start();//启动底层一个新线程,1.创建一个新线程 2.执行新线程里func（），就是下面的threadFunc，绑定了loop和event
    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

//下面这个方法了，是在单独的新线程运行的
void EventLoopThread ::threadFunc()
{
    EventLoop loop;  //创建一个独立的eventloop，和上面的线程是一一对应的，one loop one pre thread
    if(callback_)
    {
      callback_(&loop);  
    }
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }
    loop.loop();

    std::unique_lock<std::mutex>lock(mutex_);
    loop_ = nullptr;
}