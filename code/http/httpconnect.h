#ifndef HTTPCONNECT_H
#define HTTPCONNECT_H

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>

#include "../log/log.h"
#include "../sqlConnPool/sqlconnpool.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

using namespace std;

class HttpConnect
{
public:
    HttpConnect();
    ~HttpConnect();

    void init(int sockfd, const sockaddr_in& addr);
    void closeConnect();

    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);

    int getFd() const;
    int getPort() const;
    const char* getIP() const;
    sockaddr_in getAddr() const;

    bool process();

    int toWriteBytes()
    {
        return iov[0].iov_len + iov[1].iov_len;
    }

    bool isKeepAlive() const
    {
        return request.isKeepAlive();
    }

    static bool isET;
    static const char* srcDir;  // 资源的目录
    static atomic<int> userCnt; // 当前的客户端的连接数

private:
    int fd;
    struct sockaddr_in addr;

    bool isClose;

    int iovCnt;
    struct iovec iov[2];

    Buffer readBuffer;  // 读（请求）缓冲区，保存请求数据的内容
    Buffer writeBuffer; // 写（响应）缓冲区，保存响应数据的内容

    HttpRequest request;
    HttpResponse response;
};

#endif