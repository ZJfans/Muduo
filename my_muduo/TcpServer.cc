#include"TcpServer.h"
#include"logger.h"
#include"TcpConnection.h"

#include<functional>
#include<strings.h>

EventLoop* CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null!",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop,
                const InetAddress &listenAddr,
                const std::string &nameArg,
                Option option )
                :loop_(CheckLoopNotNull(loop))
                ,ipPort_(listenAddr.toIpPort())
                ,name_(nameArg)
                ,acceptor_(new Acceptor(loop,listenAddr,option == kReusePort))
                ,threadPool_(new EventLoopThreadPool(loop,name_))
                ,connectionCallback_()
                ,messageCallback_()
                ,nextConnId_(1)
                ,started_(0)
{   
    //这个回调是acceptor将已连接的fd打包成channel交给subloop 
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,
                                            std::placeholders::_1,std::placeholders::_2));
}
TcpServer::~TcpServer()
{   
    for(auto& item : connections_)
    {   
        //这个局部强指针，出作用域，自动释放new出来的资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        //销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed,conn)
        );
    }
}
//设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}
//开启服务器监听
void TcpServer::start()
{
    if(started_++ == 0)//防止一个tcpserver被start多次
    {
        threadPool_->start(threadInitCallback_);//启动底层loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen,acceptor_.get()));
    }
}
//打包,然后发送给subloop
void TcpServer::newConnection(int sockfd,const InetAddress &peerAddr)
{   
    //轮询算法，找到一个loop
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf,sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
        name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

    //通过sockfd获取其绑定的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local,sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen) < 0 )//系统调用，取到本段地址
    {
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localAddr(local);

    //创建根据连接成功的sockfd,创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
                                ioLoop,
                                connName,
                                sockfd,
                                localAddr,//本段ip与port
                                peerAddr));//对端的ip与port
    connections_[connName] = conn;
    //回调都是用户设置的，设置给TcpServer=>>TcpConnection=>>channel=>>poller  ==> notify channel
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    //如何关闭连接
    conn->setCloseCallback(
            std::bind(&TcpServer::removeConnection,this,std::placeholders::_1)
    );

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));


}
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop,this,conn)
    );
}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n",
    name_.c_str(),conn->name().c_str());

    size_t n = connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed,conn)
    );
}