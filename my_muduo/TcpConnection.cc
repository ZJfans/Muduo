#include"TcpConnection.h"
#include"logger.h"
#include"Socket.h"
#include"Channel.h"
#include"EventLoop.h"

#include<functional>
#include<errno.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<strings.h>
#include<netinet/tcp.h>
#include<memory>
#include<string>

static EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection loop_ is null!",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                    const std::string &nameArg,
                    int sockfd,
                    const InetAddress& localAddr,
                    const InetAddress& peerAddr)
                :loop_(CheckLoopNotNull(loop))
                ,name_(nameArg)
                ,state_(kConnecting)
                ,reading_(true)
                ,socket_(new Socket(sockfd))
                ,channel_(new Channel(loop,sockfd))
                ,localAddr_(localAddr)
                ,peerAddr_(peerAddr)
                ,highWaterMark_(64*1024*1024) //64M
{
    //下面给channel设置回调事件，poller给channel通知感兴趣的事件
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1)
        );
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite,this)
    );
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose,this)
    );
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError,this)
    );

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n",name_.c_str(),sockfd);
    socket_->setKeepAlive(true);
}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
        name_.c_str(),channel_->fd(),(int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n > 0)
    {
        //已建立连接的用户，有可读事件发生，调用用户注册的onmessage
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}
void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n > 0 )
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this())
                    );
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();//如果数据没发送完，他是会阻塞的
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd = %d is down,no more writing \n",channel_->fd());
    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n",channel_->fd(),(int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   //执行连接关闭的回调
    closeCallback_(connPtr);    //关闭连接的回调
}
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",name_.c_str(),err);
}

//发送数据
void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(),buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(
                    &TcpConnection::sendInLoop,
                    this,
                    buf.c_str(),
                    buf.size()
                    ));
        }
    }
}
//发送数据应用写的快，内核发的慢，需要把数据写入到缓冲区，而且设置了水位回调
void TcpConnection::sendInLoop(const void* data,size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;//没法送完的数据
    bool faultError = false;//是否产生错误

    //如果调用过该连接的shutdown，不能再进行发送了,保证多线程安全
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected,give up writing!");
        return;
    }

    //表示channel_第一次开始写数据，缓冲区没有数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(),data,len);
        if( nwrote >= 0 )
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {   
                //在这里数据发送完毕，不用给channel设置epollout事件
                loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this())
                );
            }
        }
        else//出错
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop");
                if(errno == EPIPE || errno == ECONNRESET) 
                {
                    faultError = true;
                }
            }
        }
    }
    //说明当前一次write，没有把数据全部发送出去，剩余数据需要保存在缓冲区，给channel注册epollout事件，
    //poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用handlewrite回调方法
    //如果发完，write变为diswrite，但是发不完他就永远是epollout，LT模式始终会通知
    if(!faultError && remaining > 0)
    {
        //目前发送缓冲区剩余的待发送数据的长度
        size_t oldlen = outputBuffer_.readableBytes();
        if(oldlen + remaining >= highWaterMark_
            && oldlen < highWaterMark_  //如果大于的话，说明之前已经调用过水位回调了
            && highWaterMarkCallback_)
            {
                loop_->queueInLoop(
                    std::bind(highWaterMarkCallback_,shared_from_this(),oldlen+remaining)
                );
            }
            outputBuffer_.append((char*)data + nwrote,remaining);
            if(!channel_->isWriting())
            {
                channel_->enableWriting();//这里注册写事件
            }
    }
}
//建立连接
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());  //强智能指针，保证在channel调用TcpConnection里的函数时，TcpConnection存在。
    //这其实也可以给TcpConnection析构时加个断言，保证在析构时，已经没有channel在调用（但是这样可以吗，怎么知道没有channel了）
    channel_->enableReading();  //向poller注册channel的epollin事件

    //新连接建立
    connectionCallback_(shared_from_this());
}
//连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove(); //把channel从poller中删除
}
    //关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop,this)
        );
    }
}
void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())//说明outputBuffer中数据发送完了
    {
        socket_->shutdownWrite();//关闭写端->poller->EPLLHUP->closecallback_->handleclose
    }
}