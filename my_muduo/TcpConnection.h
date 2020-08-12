#pragma once

#include"noncopyable.h"
#include"InetAddress.h"
#include "Callbacks.h"
#include"Buffer.h"
#include"TcpServer.h"
#include"Timestamp.h"

#include<memory>
#include<string>
#include<atomic>

class Channel;
class EventLoop;
class Socket;

class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop,
                    const std::string &name,
                    int sockfd,
                    const InetAddress& localAddr,
                    const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }  //判断是否连接成功

    void setConnectionCallback(const ConnectionCallback& cb)
    { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb)
    { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb)
    { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,size_t highWaterMark)
    { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

    void setCloseCallback(const CloseCallback& cb)
    { closeCallback_ = cb; }
    
    void send(const std::string &buf);//发送数据

    //建立连接
    void connectEstablished();
    //连接销毁
    void connectDestroyed();
     //关闭连接
    void shutdown();
private:
    enum StateE { kDisconnected,kConnecting,kConnected,kDisconnecting };

    void setState(StateE state){ state_ = state; } 

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    
    void sendInLoop(const void* data,size_t len);
    
    void shutdownInLoop();

    EventLoop* loop_; //这里绝对不是mainloop。因为TcpConnection是给subloop管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel>channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;



    ConnectionCallback connectionCallback_;//有新连接的回调
    MessageCallback messageCallback_;//有读写消息的回调
    WriteCompleteCallback writeCompleteCallback_;//消息发送完成后的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;//水位线

    Buffer inputBuffer_;//接收数据的缓冲区
    Buffer outputBuffer_;//发送数据的缓冲区
};