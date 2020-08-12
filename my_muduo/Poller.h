#pragma once

#include"noncopyable.h"
#include"Timestamp.h"
                                            //抽象基类，如果要用派生类，不要包含派生类，这样不好虽然语法没问题
#include<vector>
#include<unordered_map>

class Channel;
class EventLoop;

//muduo库中多路事件分发器的核心IO复用模块
class Poller
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    //给IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs,ChannelList *activeChannels) = 0;
    virtual void updateChannel(Channel *channel) = 0;//传的就是this指针
    virtual void removeChannel(Channel *channel) = 0;

    //判断channel是否在当前pooler当中
    bool hasChannel(Channel *channel)const;

    //EventLoop通过该接口，获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    //int是fd
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;//poller属于哪个EventLoop
};