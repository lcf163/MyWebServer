#include "webserver.h"

using namespace std;

// 服务器相关参数
WebServer::WebServer(
    int port, int trigMode, int timeoutMs, bool optLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd,
    const char* dbName, int connPoolNum,
    int threadNum, int maxRequests,
    bool openLog, int logLevel, int logQueSize):
    port(port), openLinger(optLinger), timeoutMs(timeoutMs), isClose(false),
    timer(new HeapTimer()), epoller(new Epoller())
{
    // 获取当前的工作目录（底层使用 malloc）
    srcDir = getcwd(nullptr, 256);
    assert(srcDir);
    strncat(srcDir, "/resources/", 16); // 拼接资源路径

    // 当前所有连接数
    HttpConnect::userCnt = 0;
    HttpConnect::srcDir = srcDir;

    // 线程池，实例初始化
    ThreadPool::instance()->init(threadNum, maxRequests);

    // 数据库连接池，实例初始化
    SqlConnPool::instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // 设置不同套接字的触发模式
    initEventMode(trigMode);
    if (!initSocket()) isClose = true;

    // 初始化日志实例
    if(openLog) {
        Log::instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port, optLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent & EPOLLET ? "ET": "LT"),
                            (connEvent & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConnect::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer()
{
    // 回收监听套接字
    close(listenFd);
    isClose = true;
    // 回收路径动态缓存
    free(srcDir);
    // 关闭数据库连接池
    SqlConnPool::instance()->destroy();
}

// 设置不同套接字的触发模式
void WebServer::initEventMode(int trigMode)
{
    // 监听事件：初始化
    listenEvent = EPOLLRDHUP;
    // 连接事件：对端断开，并设置 oneshot
    connEvent = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent |= EPOLLET;
        break;
    case 2:
        listenEvent |= EPOLLET;
        break;
    case 3:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    default:
        listenEvent |= EPOLLET;
        connEvent |= EPOLLET;
        break;
    }
    // 判断连接套接字是否为ET触发模式
    HttpConnect::isET = (connEvent & EPOLLET);
}

/* 
    epoll 循环监听事件，根据事件类型调用相应方法
    延迟计算思想，方法及参数会被打包放到线程池的任务队列中
*/
void WebServer::start()
{
    int timeMs = -1; // epoll wait timeout == -1, 无事件将阻塞
    if (!isClose) {LOG_INFO("========= Server start =========");}
    while (!isClose)
    {
        // 该连接超时，返回下一个计时器的超时时间
        if (timeoutMs > 0) {
            timeMs = timer->getNextTick();
        }

        /* 
            利用 epoll 的 epoll_wait 实现定时功能

            在计时器超时前唤醒一次epoll，判断是否有新事件到达：
                如果有事件发生，epoll_wait() 返回
                如果没有新事件，下次调用getNextTick时，将超时的堆顶计时器删除
            这样做的目的是为了让 epoll_wait() 调用次数变少，提高效率。  
        */
        int eventCnt = epoller->wait(timeMs);

        // 循环处理事件表
        for (int i = 0; i < eventCnt; i ++)
        {
            // 事件套接字
            int fd = epoller->getEventfd(i);
            // 事件内容
            uint32_t events = epoller->getEvents(i);

            // 监听套接字只有连接事件
            if (fd == listenFd) {
                dealListen(); // 处理监听的操作，接受客户端连接
            }
            // 错误的一些情况
            else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) 
            {
                assert(users.count(fd) > 0);
                closeConnect(&users[fd]); // 关闭连接
            }
            // 读事件
            else if (events & EPOLLIN)
            {
                assert(users.count(fd) > 0);
                dealRead(&users[fd]);  // 处理读操作
            }
            // 写事件
            else if (events & EPOLLOUT)
            {
                assert(users.count(fd) > 0);
                dealWrite(&users[fd]); // 处理写操作
            }
            else
            {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

// 发送错误
void WebServer::sendError(int fd, const char* info)
{
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0)
    {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

// 关闭连接套接字，并从 epoll 事件表中删除相应事件
void WebServer::closeConnect(HttpConnect* client)
{
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    epoller->delfd(client->getFd());
    client->closeConnect();
}

// 为连接注册事件和设置计时器
void WebServer::addClient(int fd, sockaddr_in addr)
{
    assert(fd > 0);
    // users 是哈希表（套接字是键，HttpConnect 对象是值）
    // 初始化 HttpConnect 对象
    users[fd].init(fd, addr);
    // 添加计时器，到期关闭连接
    if(timeoutMs > 0)
    {
        timer->add(fd, timeoutMs, bind(&WebServer::closeConnect, this, &users[fd]));
    }
    epoller->addfd(fd, EPOLLIN | connEvent);
    // 套接字设置非阻塞
    setfdNonblock(fd);
    LOG_INFO("Client[%d] in!", users[fd].getFd());
}

// 新建连接套接字，ET 模式会一次将连接队列读完
void WebServer::dealListen()
{
    struct sockaddr_in addr; // 保存连接的客户端
    socklen_t len = sizeof(addr);
    // epoll的ET模式，一次性读完所有数据
    do
    {
        int fd = accept(listenFd, (struct sockaddr*)&addr, &len);
        if (fd <= 0) { return; }
        if (HttpConnect::userCnt >= MAX_FD)
        {
            sendError(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        addClient(fd, addr);
    } while (listenEvent & EPOLLET);
}

// 将读函数和参数用 std::bind 绑定，加入线程池的任务队列
void WebServer::dealRead(HttpConnect* client)
{
    assert(client);
    extentTime(client);
    // 非静态成员函数需要传递 this 指针，作为第一个参数
    ThreadPool::instance()->addTask(std::bind(&WebServer::onRead, this, client));
}

// 将写函数和参数用 std::bind 绑定，加入线程池的任务队列
void WebServer::dealWrite(HttpConnect* client)
{
    assert(client);
    extentTime(client);
    // 非静态成员函数需要传递 this 指针，作为第一个参数
    ThreadPool::instance()->addTask(std::bind(&WebServer::onWrite, this, client));
}

// 重置计时器
void WebServer::extentTime(HttpConnect* client)
{
    assert(client);
    if (timeoutMs > 0) {
        timer->adjust(client->getFd(), timeoutMs);
    }
}

// 读函数：先接收再处理（在子线程中执行读取数据）
void WebServer::onRead(HttpConnect* client)
{
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    // 客户端发送EOF
    if (ret <= 0 && readErrno != EAGAIN)
    {
        closeConnect(client);
        return;
    }
    onProcess(client);
}

/* 
    处理函数：处理业务逻辑，解析HTTP请求

    判断读入的请求报文是否完整，决定是继续监听读还是监听写：
        如果请求不完整，继续读；
        如果请求完整，根据请求内容生成响应报文，并发送。
    oneshot 需要再次监听
*/
void WebServer::onProcess(HttpConnect* client)
{
    if (client->process())
    {
        epoller->modfd(client->getFd(), connEvent | EPOLLOUT);
    }
    else
    {
        epoller->modfd(client->getFd(), connEvent | EPOLLIN);
    }
}

/* 
    写函数：发送响应报文，大文件需要分多次发送
    由于设置了 oneshot，需要再次监听读
*/
void WebServer::onWrite(HttpConnect* client)
{
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    // 发送完毕
    if (client->toWriteBytes() == 0)
    {
        // 传输完成    
        if (client->isKeepAlive())
        {
            onProcess(client);
            return;
        }
    }
    // 发送失败
    else if (ret < 0)
    {
        // 缓存满导致，继续监听写
        if (writeErrno == EAGAIN)
        {
            epoller->modfd(client->getFd(), connEvent | EPOLLOUT);
            return;
        }
    }
    // 其他原因导致，关闭连接
    closeConnect(client);
}

// 创建监听套接字（设置属性，绑定端口，向epoll注册连接事件）
bool WebServer::initSocket()
{
    int ret;
    struct sockaddr_in addr;
    if (port > 65536 || port < 1024)
    {
        LOG_ERROR("port: %d error!", port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    // optLinger 需要的参数
    struct linger optLinger = {0};
    if (openLinger)
    {
        // 优雅关闭，最多等待20s接受客户端关闭确认
        optLinger.l_onoff = 1;
        optLinger.l_linger = 20;
    }

    // 创建监听套接字
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        LOG_ERROR("port: %d create socket error!", port);
        return false;
    }

    // 套接字设置优雅关闭
    ret = setsockopt(listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret == -1)
    {
        close(listenFd);
        LOG_ERROR("port: %d init linger error!", port);
        return false;
    }

    int optval = 1;
    // 套接字设置端口复用（端口处于 TIME_WAIT 时，也可以被 bind）
    ret = setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1)
    {
        LOG_ERROR("set socket error!");
        close(listenFd);
        return false;
    }

    // 套接字绑定端口
    ret = bind(listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1)
    {
        LOG_ERROR("bind port: %d error!", port);
        close(listenFd);
        return false;
    }

    // 套接字设为可接受连接状态，并指明请求队列大小
    ret = listen(listenFd, 6);
    if (ret == -1)
    {
        LOG_ERROR("listen port: %d error!", port);
        close(listenFd);
        return false;
    }

    // 向 epoll 注册监听套接字连接事件
    ret = epoller->addfd(listenFd, listenEvent | EPOLLIN);
    if (ret == 0)
    {
        LOG_ERROR("Add listen error!");
        close(listenFd);
        return false;
    }

    // 套接字设置非阻塞（优雅关闭还是会导致close阻塞）
    setfdNonblock(listenFd);
    LOG_INFO("Server port: %d", port);
    return true;
}

// 套接字设置非阻塞
int WebServer::setfdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
