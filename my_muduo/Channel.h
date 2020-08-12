#pragma once

#include<functional>
#include<memory>
#include"noncopyable.h"
#include"Timestamp.h"

//只是使用了类型定义，所以只声明就ok，不用包含头文件
class EventLoop;

//Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
//还绑定了poller返回的具体事件
class Channel : noncopyable
{
public:
    using EventCallback =  std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel (EventLoop *loop,int fd);
    ~Channel();
    //fd的到poller通知后，处理事件
    void handleEvent(Timestamp receiveTime);//使用了Timestamp的对象，需要知道其的大小，所以需要包含其头文件，EventLoop只是一个指针
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb);}
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb);}
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb);}

    //防止channel被手动remove掉，channel还继续执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    //设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update();}  //其实就是注册fd对应的events事件  //用read的那一位和event_或一下，events_读那一位就变成1了，也就是读事件
    void disableReading() { events_ &= ~kReadEvent;update();} //先取反，read读那一位变成0了，再与一下，events_的read就变成0了，也就是禁止读了
    void enableWriting() { events_ |= kWriteEvent;update();}
    void disableWriting() { events_ &= kWriteEvent;update();}
    void disableAll() { events_ = kNoneEvent;update();}

    bool isNoneEvent() const { return events_ == kNoneEvent;}
    bool isWriting() const { return events_ == kWriteEvent;}
    bool isReading() const { return events_ == kReadEvent;}

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;//事件循环
    const int fd_;
    int events_;
    int revents_;//poller返回的具体发生的事件
    int index_; //在poller中的3种状态

    std::weak_ptr<void>tie_;//channel要析构（有2个断言），1.调用remove，此时析构中一个断言可以通过 2.如果此时还有回调函数在执行，则无法析构（断言实现）
    bool tied_;

    //// 因为channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;

};