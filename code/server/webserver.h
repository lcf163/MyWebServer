#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../sqlConnPool/sqlconnpool.h"
#include "../threadPool/threadpool.h"
#include "../http/httpconnect.h"

using namespace std;

class WebServer
{
public:
    WebServer(
        int port, int trigMode, int timeoutMs, bool optLinger,
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum,
        int threadNum, int maxRequests,
        bool openLog, int logLevel, int logQueSize);
    
    ~WebServer();

    void start();
private:
    bool initSocket();
    void initEventMode(int trigMode);
    void addClient(int fd, sockaddr_in addr);

    void dealListen();
    void dealWrite(HttpConnect* client);
    void dealRead(HttpConnect* client);

    void sendError(int fd, const char* info);
    void extentTime(HttpConnect* client);
    void closeConnect(HttpConnect* client);

    void onRead(HttpConnect* client);
    void onWrite(HttpConnect* client);
    void onProcess(HttpConnect* client);

    static const int MAX_FD = 65536;  // 最大的文件描述符的数量
    static int setfdNonblock(int fd); // 设置文件描述符为非阻塞

    int port;        // 端口
    bool openLinger; // 是否打开优雅关闭
    int timeoutMs;   // 毫秒MS
    bool isClose;   // 是否关闭
    int listenFd;    // 监听的文件描述符
    char* srcDir;    // 资源的目录

    uint32_t listenEvent; // 监听的文件描述符的事件
    uint32_t connEvent;   // 连接的文件描述符的事件

    unique_ptr<HeapTimer> timer;           // 定时器
    unique_ptr<Epoller> epoller;           // epoll对象
    unordered_map<int, HttpConnect> users; // 保存客户端连接的信息
};

#endif